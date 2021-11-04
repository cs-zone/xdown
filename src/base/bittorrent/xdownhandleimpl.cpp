#include "xdownhandleimpl.h"

#include <algorithm>
#include <memory>
#include <type_traits>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#include <libtorrent/address.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/time.hpp>
#include <libtorrent/version.hpp>

#if (LIBTORRENT_VERSION_NUM >= 10200)
#include <libtorrent/storage_defs.hpp>
#include <libtorrent/write_resume_data.hpp>
#else
#include <libtorrent/storage.hpp>
#endif

#include <QBitArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QUrl>

#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/tristatebool.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "downloadpriority.h"
#include "peeraddress.h"
#include "peerinfo.h"
#include "ltunderlyingtype.h"
#include "session.h"
#include "trackerentry.h"


using namespace BitTorrent;

#if (LIBTORRENT_VERSION_NUM >= 10200)
namespace libtorrent
{
    namespace aux
    {
        template <typename T, typename Tag>
        uint qHash(const strong_typedef<T, Tag> &key, const uint seed)
        {
            return ::qHash((std::hash<strong_typedef<T, Tag>> {})(key), seed);
        }
    }
}
#endif

namespace
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    using LTDownloadPriority = int;
    using LTPieceIndex = int;
    using LTQueuePosition = int;
#else
    using LTDownloadPriority = lt::download_priority_t;
    using LTPieceIndex = lt::piece_index_t;
    using LTQueuePosition = lt::queue_position_t;
#endif

    std::vector<LTDownloadPriority> toLTDownloadPriorities(const QVector<DownloadPriority> &priorities)
    {
        std::vector<LTDownloadPriority> out;
        out.reserve(priorities.size());

        std::transform(priorities.cbegin(), priorities.cend()
                       , std::back_inserter(out), [](BitTorrent::DownloadPriority priority)
        {
            return static_cast<LTDownloadPriority>(
                        static_cast<LTUnderlyingType<LTDownloadPriority>>(priority));
        });
        return out;
    }

    using ListType = lt::entry::list_type;

    ListType setToEntryList(const QSet<QString> &input)
    {
        ListType entryList;
        for (const QString &setValue : input)
            entryList.emplace_back(setValue.toStdString());
        return entryList;
    }
}


// CreateTorrentParams
#define ARG_HEADER " --header "
#define ARG_DEL_HEADER " --dheader "
#define ARG_URI_PREFIX " -- "

CreateXDownParams::CreateXDownParams(const QString &strSource, const qlonglong fileSize, qlonglong completedSize)
{
    bool bUsedOut = false;
    source = strSource.trimmed();
    int iPosVal = source.indexOf(" ");
    if (iPosVal == -1) {
        url = source;
    }
    else {
        url = source.mid(0, iPosVal);
        
        QString strParam = source.mid(iPosVal).trimmed();
        // 1. http header
        ParseRequestHeader(strParam);
        // 2. delete header
        ParseDelRequestHeader(strParam);

        ParseDealWithRequestHeader();


        ParseRequestUriOption(strParam);
        if (reqUriOptionMap.contains("out") && name.length() < 1 ) {
            QString strFileName = reqUriOptionMap["out"];
            if (strFileName.length() > 0) {
                bUsedOut = true;
                name = strFileName;
            }
        }

    }

    if (bUsedOut) {
        uriFileName = name;
    }
    else {
        QString strTmpName(url);
        int iPosValue = strTmpName.lastIndexOf("/");
        if (iPosValue > 0) {
            strTmpName = strTmpName.mid(iPosValue + 1);
        }
        iPosValue = strTmpName.indexOf("?");
        if (iPosValue > 0) {
            strTmpName = strTmpName.mid(0, iPosValue);
        }
        iPosValue = strTmpName.indexOf("#");
        if (iPosValue > 0) {
            strTmpName = strTmpName.mid(0, iPosValue);
        }
        uriFileName = strTmpName;
    }
}


int CreateXDownParams::ParseRequestHeader(const QString &strParam)
{
    // parse
    // 1. header
    // --header
    int iParsed = 0;
    QString tmpParam = strParam;
    while (true) {
        int iFindPos = tmpParam.indexOf(ARG_HEADER);
        if (iFindPos == -1) {
            break;
        }
        else {
            QString strHeadValue = "";
            QString strParamLast = tmpParam.mid(iFindPos + strlen(ARG_HEADER) + 1).trimmed();
            if (strParamLast.startsWith("\"")) {
                int iLastValue = strParamLast.indexOf("\"", 1);
                if (iLastValue != -1) {
                    strHeadValue = strParamLast.mid(1, iLastValue - 1);
                    tmpParam = strParamLast.mid(iLastValue + 1).trimmed();
                }
                else {
                    break;
                }
            }
            else {
                int iLastValue = strParamLast.indexOf(" ");
                if (iLastValue != -1) {
                    strHeadValue = strParamLast.mid(0, iLastValue);
                    tmpParam = strParamLast.mid(iLastValue + 1).trimmed();
                }
                else {
                    strHeadValue = strParamLast;
                    tmpParam = "";
                }
            }
            if (strHeadValue.length() > 0) {
                int iPosVal = strHeadValue.indexOf(":");
                if (iPosVal != -1) {
                    // find
                    QString strKey = strHeadValue.mid(0, iPosVal).trimmed();
                    QString strValue = strHeadValue.mid(iPosVal + 1).trimmed();
                    reqHeaderMap.insert(strKey, strValue);
                    ++iParsed;
                }
            }
        }
    }
    return iParsed;
}

int CreateXDownParams::ParseDelRequestHeader(const QString &strParam)
{
    // parse
    // 1. del header
    // --dheader
    int iParsed = 0;
    QString tmpParam = strParam;
    while (true) {
        int iFindPos = tmpParam.indexOf(ARG_DEL_HEADER);
        if (iFindPos == -1) {
            break;
        }
        else {
            QString strHeadValue = "";
            QString strParamLast = tmpParam.mid(iFindPos + strlen(ARG_DEL_HEADER) + 1).trimmed();
            if (strParamLast.startsWith("\"")) {
                int iLastValue = strParamLast.indexOf("\"", 1);
                if (iLastValue != -1) {
                    strHeadValue = strParamLast.mid(1, iLastValue);
                    tmpParam = strParamLast.mid(iLastValue + 1).trimmed();
                }
                else {
                    break;
                }
            }
            else {
                int iLastValue = strParamLast.indexOf(" ");
                if (iLastValue != -1) {
                    strHeadValue = strParamLast.mid(0, iLastValue);
                    tmpParam = strParamLast.mid(iLastValue + 1).trimmed();
                }
                else {
                    strHeadValue = strParamLast;
                    tmpParam = "";
                }
            }
            if (strHeadValue.length() > 0) {
                reqDelHeaderList.push_back(strHeadValue);
                ++iParsed;
            }
        }
    }
    return iParsed;
}

int CreateXDownParams::ParseDealWithRequestHeader()
{
    int iParsed = 0;
    if (reqDelHeaderList.size() > 0) {
        QHash<QString, int> tmpDelHash;
        for (auto iter = reqDelHeaderList.begin(); iter != reqDelHeaderList.end(); iter++) {
            tmpDelHash.insert((*iter).toLower(), 1);
        }
        for (auto iter = reqHeaderMap.begin(); iter != reqHeaderMap.end(); ) {
            const QString strHeadKey = iter.key();
            if (tmpDelHash.contains(strHeadKey.toLower())) {
                // find
                iter = reqHeaderMap.erase(iter);
                iParsed++;
            }
            else {
                iter++;
            }
        }
    }
    return iParsed;
}

int CreateXDownParams::ParseRequestUriOption(const QString &strParam)
{
    int iFindPos = 0;
    int iParsed = 0;
    QString tmpParam = strParam;
    int iMaxLoop = 100;
    int iCurLoop = 0;
    while (true) {
        if (iCurLoop++ > iMaxLoop) {
            break;
        }
        iFindPos = tmpParam.indexOf(ARG_URI_PREFIX);
        if (iFindPos == -1) {
            break;
        }
        else {
            QString strUriKey = "";
            QString strUriVal = "";
            QString strParamLast = tmpParam.mid(iFindPos + strlen(ARG_URI_PREFIX)).trimmed();
            int iFirstValue = strParamLast.indexOf(" ");
            if (iFirstValue != -1) {
                strUriKey = strParamLast.mid(0, iFirstValue).trimmed();
                strParamLast = strParamLast.mid(iFirstValue + 1).trimmed();
                if (strParamLast.indexOf("\"") == 0) {
                    int iLastValue = strParamLast.indexOf("\"",1);
                    if (iLastValue != -1) {
                        strUriVal = strParamLast.mid(0, iLastValue + 1).trimmed();
                        tmpParam = strParamLast.mid(iLastValue + 1).trimmed();
                    }
                    else {
                        strUriVal = strParamLast;
                        tmpParam = "";
                    }
                }
                else {
                    int iLastValue = strParamLast.indexOf(" ");
                    if (iLastValue != -1) {
                        strUriVal = strParamLast.mid(0, iLastValue + 1).trimmed();
                        tmpParam = strParamLast.mid(iLastValue + 1).trimmed();
                    }
                    else {
                        strUriVal = strParamLast;
                        tmpParam = "";
                    }
                }

                if (strUriKey.length() > 0 && strUriVal.length() > 0) {
                    if (strUriVal.startsWith("\"") && strUriVal.endsWith("\"")) {
                        strUriVal = strUriVal.mid(1, strUriVal.length() - 2);
                    }
                }
                if (strUriKey.length() > 0 && strUriVal.length() > 0) {
                    if ("--" + strUriKey.toLower() == ARG_HEADER || "--" + strUriKey.toLower() == ARG_DEL_HEADER) {
                        continue;
                    }
                    reqUriOptionMap.insert(strUriKey, strUriVal);
                    ++iParsed;
                }
            }
            else {
                break;
            }
        }
    }
    return iParsed;
}


XDownHandleImpl::XDownHandleImpl(Session *session, const CreateXDownParams &params)
    : QObject(session)
    , m_session(session)
    , m_name(params.name)
    , m_url(params.url)
    , m_source(params.source)
    , m_fileSize(params.fileSize)
    , m_completedSize(params.completedSize)
    , m_fileIndex(params.fileIndex)
    , m_downConcurrent(params.downConcurrent)

    , m_reqHeaderMap(params.reqHeaderMap)
    , m_reqUriOptionMap(params.reqUriOptionMap)
    , m_defUriOptionMap(params.defUriOptionMap)

    , m_UIHeaderMap(params.uiHeaderMap)
    , m_UIOptionMap(params.uiOptionMap)

    , m_handleType(TaskHandleType::XDown_Handle)

    , m_ratioLimit(params.ratioLimit)
    , m_gid(0)
    , m_seedingTimeLimit(0)
{
    if (params.savePath.length() > 0) {
        m_savePath = params.savePath;
    }
    else {
        m_savePath = Utils::Fs::toNativePath(params.savePath);
    }

    if (params.uiOptionMap.contains(QT_UI_SAVE_PATH_CTRL) && savePath().length() < 1) {
        const QString strSavePath = params.uiOptionMap[QT_UI_SAVE_PATH_CTRL];
        QDir *saveDir = new QDir;
        bool pathExist = saveDir->exists(strSavePath);
        if (pathExist) {
            setSavePath(strSavePath);
        }
        delete saveDir;
    }

    /*if (m_useAutoTMM) {
        m_savePath = Utils::Fs::toNativePath(m_session->categorySavePath(m_category));
    }*/

    updateStatus();
    //m_hash = InfoHash(m_nativeStatus.info_hash);

    // NB: the following two if statements are present because we don't want
    // to set either sequential download or first/last piece priority to false
    // if their respective flags in data are false when a torrent is being
    // resumed. This is because, in that circumstance, this constructor is
    // called with those flags set to false, even if the torrent was set to
    // download sequentially or have first/last piece priority enabled when
    // its resume data was saved. These two settings are restored later. But
    // if we set them to false now, both will erroneously not be restored.
    //if (!params.restored || params.sequential)
    //    setSequentialDownload(params.sequential);
    //if (!params.restored || params.firstLastPiecePriority)
    //    setFirstLastPiecePriority(params.firstLastPiecePriority);

    //if (!params.restored && hasMetadata()) {
    //    if (filesCount() == 1)
    //        m_hasRootFolder = false;
    //}
}

XDownHandleImpl::~XDownHandleImpl() {}

void XDownHandleImpl::clearPeers()
{
}

bool XDownHandleImpl::isValid() const
{
    return true;
}

// ignore
InfoHash XDownHandleImpl::hash() const
{
    return m_hash;
}

TaskHandleType XDownHandleImpl::getHandleType() const
{
    return m_handleType;
}

#include <qdebug.h>


QString XDownHandleImpl::url() const
{
    return m_url;
}

QString XDownHandleImpl::source() const 
{
    return m_source;
}

void XDownHandleImpl::setHandleActionType(TaskHandleActionType iValue)
{
    m_taskHandleActionType = iValue;
}

TaskHandleActionType XDownHandleImpl::getHandleActionType() const
{
    return m_taskHandleActionType;
}

bool XDownHandleImpl::checkIsUpdateHandle() const {
    if (m_taskHandleActionType == TaskHandleActionType::ActionType_UpdateTxt
        || m_taskHandleActionType == TaskHandleActionType::ActionType_UpdateZip
        || m_taskHandleActionType == TaskHandleActionType::ActionType_Version_Dns_Over_Https) {
        return true;
    }
    return false;
}



QString XDownHandleImpl::name() const
{
    //qDebug() << " XDownHandleImpl name " << m_url;
    QString name = m_name;
    if (!name.isEmpty()) return name;
    return m_url;
}
// ignore
QDateTime XDownHandleImpl::creationDate() const
{
    return {};
}
// ignore
QString XDownHandleImpl::creator() const
{
    return "";
}
// ignore
QString XDownHandleImpl::comment() const
{
    return "";
}
// ignore
bool XDownHandleImpl::isPrivate() const
{
    return false;
}

qlonglong XDownHandleImpl::totalSize() const
{
    return m_fileSize;
}

void XDownHandleImpl::setTotalSize(qlonglong iValue) 
{
    m_fileSize = iValue;
}

void XDownHandleImpl::setCompletedSize(qlonglong iValue)
{
    m_completedSize = iValue;
}

void XDownHandleImpl::setDownSpeed(long iValue)
{
    m_downSpeed = iValue;
}

int XDownHandleImpl::getRetryValue()
{
    return m_retryValue;
}

void XDownHandleImpl::setRetryValue(int iValue)
{
    m_retryValue = iValue;
}

void XDownHandleImpl::addRetryValue()
{
    ++m_retryValue;
}

long XDownHandleImpl::getDownSpeed() const
{
    return m_downSpeed;
}

QString XDownHandleImpl::getFmtDownSpeed() const
{
    QString strSpeed = "-";
    long dSpeedValue = m_downSpeed;
    if (dSpeedValue < 0)
    {
    }
    else if (dSpeedValue < 1024)
    {
        strSpeed = QString("%1 B/s").arg(QString::number(dSpeedValue));
    }
    else if (dSpeedValue < 1024 * 1024)
    {
        strSpeed = QString("%1 KB/s").arg(QString::number((double)(dSpeedValue / 1024.0), 'f', 2));
    }
    else if (dSpeedValue < 1024 * 1024 * 1024)
    {
        strSpeed = QString("%1 MB/s").arg(QString::number((double)(dSpeedValue / (1024 * 1024.0)), 'f', 2));
    }
    else
    {
        strSpeed = QString("%1 GB/s").arg(QString::number((double)(dSpeedValue / (1024 * 1024 * 1024.0)), 'f', 2));
    }
    return strSpeed;
}

qlonglong XDownHandleImpl::wantedSize() const
{
    if (m_fileSize > 0) {
        return m_fileSize;
    }
    else {
        return 0L;
    }
}

qlonglong XDownHandleImpl::completedSize() const
{
    return m_completedSize;
}
// ignore
//qlonglong XDownHandleImpl::incompletedSize() const
//{
//    return m_completedSize;
//}
// ignore
qlonglong XDownHandleImpl::pieceLength() const
{
    return 0L;
}
// ignore
qlonglong XDownHandleImpl::wastedSize() const
{
    return 0L;
}
// ignore
QString XDownHandleImpl::currentTracker() const
{
    return "";
}

QString XDownHandleImpl::savePath(bool actual) const
{
    if (m_savePath.length() > 0) {
        return m_savePath;
    }
    else {
        return  Utils::Fs::toUniformPath(m_savePath);
    }
}

void XDownHandleImpl::setSavePath(const QString &strValue)
{
    m_savePath = strValue;
}

void XDownHandleImpl::startParsePathAndFileName(const QString &strFullFileName)
{
    if (!(strFullFileName.startsWith("http://")
        || strFullFileName.startsWith("https://")
        || strFullFileName.startsWith("ftp://"))) {
        int iPosFindName = strFullFileName.lastIndexOf("/");
        if (iPosFindName == -1) {
            iPosFindName = strFullFileName.lastIndexOf("\\");
        }
        QString strFilePath = strFullFileName.mid(0, iPosFindName);
        QString strFileName = strFullFileName.mid(iPosFindName + 1);
        setSavePath(strFilePath);
        setName(strFileName);
    }
}

// ignore
QString XDownHandleImpl::rootPath(bool actual) const
{
    return {};
}

QString XDownHandleImpl::contentPath(const bool actual) const
{
    return savePath(actual);
}

// ignore
bool XDownHandleImpl::isAutoTMMEnabled() const
{
    return true;
}
// ignore
void XDownHandleImpl::setAutoTMMEnabled(bool enabled)
{
}
// ignore
//bool XDownHandleImpl::hasRootFolder() const
//{
//    return false;
//}

QString XDownHandleImpl::actualStorageLocation() const
{
    return QString::fromStdString(m_nativeStatus.save_path);
}
// ignore
bool XDownHandleImpl::isAutoManaged() const
{
    return false;
}
// ignore
void XDownHandleImpl::setAutoManaged(const bool enable)
{
}

// ignore
QVector<TrackerEntry> XDownHandleImpl::trackers() const
{
    QVector<TrackerEntry> entries;
    return entries;
}

QHash<QString, TrackerInfo> XDownHandleImpl::trackerInfos() const
{
    return m_trackerInfos;
}

void XDownHandleImpl::addTrackers(const QVector<TrackerEntry> &trackers)
{
}

void XDownHandleImpl::replaceTrackers(const QVector<TrackerEntry> &trackers)
{
}

QVector<QUrl> XDownHandleImpl::urlSeeds() const
{
    QVector<QUrl> urlSeeds;
    return urlSeeds;
}

void XDownHandleImpl::addUrlSeeds(const QVector<QUrl> &urlSeeds)
{
}

void XDownHandleImpl::removeUrlSeeds(const QVector<QUrl> &urlSeeds)
{
}

bool XDownHandleImpl::connectPeer(const PeerAddress &peerAddress)
{
    return true;
}

bool XDownHandleImpl::needSaveResumeData() const
{
    return false;
}

void XDownHandleImpl::saveResumeData()
{
}

int XDownHandleImpl::filesCount() const
{
    return 1;
}

int XDownHandleImpl::piecesCount() const
{
    return 1;
}

int XDownHandleImpl::piecesHave() const
{
    return false;
}

// 下载进度
qreal XDownHandleImpl::progress() const
{
    if (m_fileSize <= 0) {
        return 0.f;
    }
    if (m_completedSize <= 0) {
        return 0.f;
    }

    if (m_completedSize == m_fileSize) {
        return 1.f;
    }
    const qreal progress = static_cast<qreal>(m_completedSize) / m_fileSize;
    Q_ASSERT((progress >= 0.f) && (progress <= 1.f));
    return progress;
}


qreal XDownHandleImpl::ratioLimit() const
{
    return m_ratioLimit;
}

int XDownHandleImpl::seedingTimeLimit() const
{
    return m_seedingTimeLimit;
}

QString XDownHandleImpl::filePath(int index) const
{
    return savePath(true);
}

QString XDownHandleImpl::fileName(int index) const
{
    return m_name;
}

aria2::A2Gid XDownHandleImpl::getGid() const
{
    return m_gid;
}

void XDownHandleImpl::setGid(const aria2::A2Gid m_value)
{
    m_gid = m_value;
}

qlonglong XDownHandleImpl::fileSize(int index) const
{
    return m_fileSize;
}

// Return a list of absolute paths corresponding
// to all files in a torrent
QStringList XDownHandleImpl::absoluteFilePaths() const
{
    QStringList res;
    return res;
}

QVector<DownloadPriority> XDownHandleImpl::filePriorities() const
{
    QVector<DownloadPriority> ret;
    return ret;
}

TorrentInfo XDownHandleImpl::info() const
{
    return m_torrentInfo;
}

bool XDownHandleImpl::isPaused() const
{
    if (isCompleted()) return false;
    return  m_state == TorrentState::XDown_Paused;
}
//
//bool XDownHandleImpl::isResumed() const
//{
//    return false;
//}


bool XDownHandleImpl::isQueued() const
{
    if (isPaused()) {
        return false;
    }
    if (m_downloadStatus == aria2::DownloadStatus::DOWNLOAD_WAITING) {
        return true;
    }
    return false;
}

bool XDownHandleImpl::isChecking() const
{
    return false;
}

bool XDownHandleImpl::isDownloading() const
{
    if (isCompleted()) return false;
    return m_state == TorrentState::XDown_Downloading;
}

bool XDownHandleImpl::isUploading() const
{
    return false;
}

bool XDownHandleImpl::isCompleted() const
{
    return m_fileSize > 0 && m_fileSize == m_completedSize;
}

bool XDownHandleImpl::isActive() const
{
    if (isPaused()) {
        return false;
    }
    return true;
}

bool XDownHandleImpl::isInactive() const
{
    return !isActive();
}

// TODO 
bool XDownHandleImpl::isErrored() const
{
    if (isCompleted()) return false;
    return m_state == TorrentState::XDown_Error;
}

bool XDownHandleImpl::isSeed() const
{
    return false;
}

bool XDownHandleImpl::isForced() const
{
    return false;
}

bool XDownHandleImpl::isSequentialDownload() const
{
    return false;
}

bool XDownHandleImpl::hasFirstLastPiecePriority() const
{
    return false;
}

TorrentState XDownHandleImpl::state() const
{
    return m_state;
}

void XDownHandleImpl::updateState()
{
    
}

void XDownHandleImpl::updateState(aria2::DownloadEvent dEvent, long iValue, const QString &strErrMessage)
{
    bool bError = false;
    switch (dEvent)
    {
    case aria2::DownloadEvent::EVENT_ON_DOWNLOAD_NONE:
        setErrorCode(0);
        setErrorMessage("");
        m_state = TorrentState::XDown_Paused;
        break;
    case aria2::DownloadEvent::EVENT_ON_DOWNLOAD_START:
        setErrorCode(0);
        setErrorMessage("");
        m_state = TorrentState::XDown_Downloading;
        break;
    case aria2::EVENT_ON_DOWNLOAD_PAUSE:
    case aria2::EVENT_ON_DOWNLOAD_STOP:
        setErrorCode(0);
        setErrorMessage("");
        m_state = TorrentState::XDown_Paused;
        break;
    case aria2::EVENT_ON_DOWNLOAD_COMPLETE:
        setErrorCode(0);
        setErrorMessage("");
        m_state = TorrentState::XDown_Completed;
        break;
    case aria2::EVENT_ON_DOWNLOAD_ERROR:
        m_state = TorrentState::XDown_Error;
        if (iValue > 0 && strErrMessage.length() > 0) {
            setErrorCode(iValue);
            setErrorMessage(strErrMessage);
            bError = true;
        }
        break;
    case aria2::EVENT_ON_DOWNLOAD_REMOVE:
        m_state = TorrentState::XDown_Removing;
        break;
    default:
        setErrorCode(0);
        setErrorMessage("");
        m_state = TorrentState::XDown_Paused;
        break;
    }
    if (dEvent) {
        m_event = dEvent;
    }
    if (!bError) {
        // 没有错误
        setErrorCode(0);
    }
}

aria2::DownloadEvent XDownHandleImpl::getDownloadEvent()
{
    return m_event;
}

bool XDownHandleImpl::hasMetadata() const
{
    return false;
}

bool XDownHandleImpl::hasMissingFiles() const
{
    return false;
}

// TODO 
bool XDownHandleImpl::hasError() const
{
    if (m_errorCode) {
        return true;
    }
    return false;
}

bool XDownHandleImpl::hasFilteredPieces() const
{
    return false;
}

int XDownHandleImpl::queuePosition() const
{
    return 0;
}

QString XDownHandleImpl::error() const
{
    return m_errorMessage;
}

void XDownHandleImpl::setErrorMessage(const QString &strValue)
{
    m_errorMessage = strValue;
}

QString XDownHandleImpl::getErrorMessage() const
{
    return m_errorMessage;
}

qlonglong XDownHandleImpl::totalDownload() const
{
    return m_completedSize;
}

qlonglong XDownHandleImpl::totalUpload() const
{
    return 0L;
}

qlonglong XDownHandleImpl::activeTime() const
{
    return lt::total_seconds(m_nativeStatus.active_duration);
}

qlonglong XDownHandleImpl::finishedTime() const
{
    return lt::total_seconds(m_nativeStatus.finished_duration);
}

qlonglong XDownHandleImpl::seedingTime() const
{
    return lt::total_seconds(m_nativeStatus.seeding_duration);
}

// 剩余x时间完成下载
qlonglong XDownHandleImpl::eta() const
{
    if (isPaused()) return MAX_ETA;
    long speedVal = getDownSpeed();
    if (speedVal < 1) {
        // 没有速度
        return MAX_ETA;
    }

    if (m_completedSize > 0 && m_completedSize == m_fileSize) {
        // 完成
        return MAX_ETA;
    }

    return (wantedSize() - completedSize()) / speedVal;
}

QVector<qreal> XDownHandleImpl::filesProgress() const
{
    QVector<qreal> result;
    return result;
}

int XDownHandleImpl::seedsCount() const
{
    return 0;
}

int XDownHandleImpl::peersCount() const
{
    return 0;
}

int XDownHandleImpl::leechsCount() const
{
    return 0;
}

int XDownHandleImpl::totalSeedsCount() const
{
    return 0;
}

int XDownHandleImpl::totalPeersCount() const
{
    return 0;
}

int XDownHandleImpl::totalLeechersCount() const
{
    return 0;
}

int XDownHandleImpl::completeCount() const
{
    return 0;
}

int XDownHandleImpl::incompleteCount() const
{
    return 0;
}

QDateTime XDownHandleImpl::lastSeenComplete() const
{
    return {};
}


QDateTime XDownHandleImpl::addedTime() const
{
    return QDateTime::fromSecsSinceEpoch(added_time);
}

void XDownHandleImpl::setAddedTime(qint64 iValue)
{
    added_time = iValue;
}

QDateTime XDownHandleImpl::completedTime() const
{
    if (completed_time > 0 && m_fileSize == m_completedSize && m_fileSize > 0 )
        return QDateTime::fromSecsSinceEpoch(completed_time);
    else
        return {};
}

void XDownHandleImpl::setCompletedTime(qint64 iValue) 
{
    completed_time = iValue;
}

qlonglong XDownHandleImpl::timeSinceUpload() const
{
    return -1;
}

qlonglong XDownHandleImpl::timeSinceDownload() const
{
    if (m_nativeStatus.last_download.time_since_epoch().count() == 0)
        return -1;
    return lt::total_seconds(lt::clock_type::now() - m_nativeStatus.last_download);
}

qlonglong XDownHandleImpl::timeSinceActivity() const
{
    const qlonglong upTime = timeSinceUpload();
    const qlonglong downTime = timeSinceDownload();
    return ((upTime < 0) != (downTime < 0))
        ? std::max(upTime, downTime)
        : std::min(upTime, downTime);
}

int XDownHandleImpl::downloadLimit() const
{
    //return m_nativeHandle.download_limit();
    qDebug() << "====Entry XDownHandleImpl";
    return 0;
}

int XDownHandleImpl::uploadLimit() const
{
    return 0;
}

bool XDownHandleImpl::superSeeding() const
{
    return false;
}

QVector<PeerInfo> XDownHandleImpl::peers() const
{
    QVector<PeerInfo> peers;
    return peers;
}

QBitArray XDownHandleImpl::pieces() const
{
    QBitArray result;
    return result;
}

QBitArray XDownHandleImpl::downloadingPieces() const
{
    QBitArray result;
    return result;
}

QVector<int> XDownHandleImpl::pieceAvailability() const
{
    std::vector<int> avail;
    return Vector::fromStdVector(avail);
}

qreal XDownHandleImpl::distributedCopies() const
{
    return m_nativeStatus.distributed_copies;
}

qreal XDownHandleImpl::maxRatio() const
{
    if (m_ratioLimit == USE_GLOBAL_RATIO)
        return m_session->globalMaxRatio();

    return m_ratioLimit;
}

// TODO 
int XDownHandleImpl::maxSeedingTime() const
{
    if (m_seedingTimeLimit == USE_GLOBAL_SEEDING_TIME)
        return m_session->globalMaxSeedingMinutes();

    return m_seedingTimeLimit;
}

qreal XDownHandleImpl::realRatio() const
{
    return 0;
}

int XDownHandleImpl::uploadPayloadRate() const
{
    return 0;
}

// 下载速度
int XDownHandleImpl::downloadPayloadRate() const
{
    return m_downSpeed;
}

qlonglong XDownHandleImpl::totalPayloadUpload() const
{
    return m_nativeStatus.total_payload_upload;
}

qlonglong XDownHandleImpl::totalPayloadDownload() const
{
    return m_nativeStatus.total_payload_download;
}

int XDownHandleImpl::connectionsCount() const
{
    return m_nativeStatus.num_connections;
}

int XDownHandleImpl::connectionsLimit() const
{
    return m_nativeStatus.connections_limit;
}

qlonglong XDownHandleImpl::nextAnnounce() const
{
    return lt::total_seconds(m_nativeStatus.next_announce);
}

void XDownHandleImpl::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        //m_session->handleTorrentNameChanged(this);
    }
}


void XDownHandleImpl::move(QString path)
{
}


QString XDownHandleImpl::getItemHash() const
{
    return m_itemHash;
}

void XDownHandleImpl::forceReannounce(int index)
{
    //m_nativeHandle.force_reannounce(0, index);
}

void XDownHandleImpl::forceDHTAnnounce()
{
    //m_nativeHandle.force_dht_announce();
}

void XDownHandleImpl::forceRecheck()
{
    /*if (!hasMetadata()) return;

    m_nativeHandle.force_recheck();
    m_unchecked = false;*/
}

void XDownHandleImpl::setSequentialDownload(const bool enable)
{
}

void XDownHandleImpl::setFirstLastPiecePriority(const bool enabled)
{
    //setFirstLastPiecePriorityImpl(enabled);
}

void XDownHandleImpl::setFirstLastPiecePriorityImpl(const bool enabled, const QVector<DownloadPriority> &updatedFilePrio)
{
    // Download first and last pieces first for every file in the torrent

//    if (!hasMetadata()) {
//        m_needsToSetFirstLastPiecePriority = enabled;
//        return;
//    }
//
//#if (LIBTORRENT_VERSION_NUM < 10200)
//    const std::vector<LTDownloadPriority> filePriorities = !updatedFilePrio.isEmpty() ? toLTDownloadPriorities(updatedFilePrio)
//                                                                           : nativeHandle().file_priorities();
//    std::vector<LTDownloadPriority> piecePriorities = nativeHandle().piece_priorities();
//#else
//    const std::vector<LTDownloadPriority> filePriorities = !updatedFilePrio.isEmpty() ? toLTDownloadPriorities(updatedFilePrio)
//                                                                           : nativeHandle().get_file_priorities();
//    std::vector<LTDownloadPriority> piecePriorities = nativeHandle().get_piece_priorities();
//#endif
//    // Updating file priorities is an async operation in libtorrent, when we just updated it and immediately query it
//    // we might get the old/wrong values, so we rely on `updatedFilePrio` in this case.
//    for (int index = 0; index < static_cast<int>(filePriorities.size()); ++index) {
//        const LTDownloadPriority filePrio = filePriorities[index];
//        if (filePrio <= LTDownloadPriority {0})
//            continue;
//
//        // Determine the priority to set
//        const LTDownloadPriority newPrio = enabled ? LTDownloadPriority {7} : filePrio;
//        const TorrentInfo::PieceRange extremities = info().filePieces(index);
//
//        // worst case: AVI index = 1% of total file size (at the end of the file)
//        const int nNumPieces = std::ceil(fileSize(index) * 0.01 / pieceLength());
//        for (int i = 0; i < nNumPieces; ++i) {
//            piecePriorities[extremities.first() + i] = newPrio;
//            piecePriorities[extremities.last() - i] = newPrio;
//        }
//    }
//
//    m_nativeHandle.prioritize_pieces(piecePriorities);
//
//    LogMsg(tr("Download first and last piece first: %1, torrent: '%2'")
//        .arg((enabled ? tr("On") : tr("Off")), name()));
//
//    saveResumeData();
}

// TODO 
void XDownHandleImpl::pause()
{
    if (getErrorCode() > 0) {
        updateState(aria2::DownloadEvent::EVENT_ON_DOWNLOAD_NONE);
    }
    if (isPaused()) return;

    setAutoManaged(false);

    m_session->handleXDownPaused(this);
}

void XDownHandleImpl::resume(const TorrentOperatingMode mode)
{
    resume_impl(true);
}

// TODO 
void XDownHandleImpl::resume_impl(bool forced)
{
    m_session->handleXDownResumed(this);

}


void XDownHandleImpl::renameFile(const int index, const QString &name)
{
    //m_oldPath[LTFileIndex {index}].push_back(filePath(index));
    //++m_renameCount;
    //m_nativeHandle.rename_file(LTFileIndex {index}, Utils::Fs::toNativePath(name).toStdString());
}

void XDownHandleImpl::handleStateUpdate(const lt::torrent_status &nativeStatus)
{
    updateStatus(nativeStatus);
}

void XDownHandleImpl::handleStorageMoved(const QString &newPath, const QString &errorMessage)
{
    /*m_storageIsMoving = false;

    if (!errorMessage.isEmpty())
        LogMsg(tr("Could not move torrent: %1. Reason: %2").arg(name(), errorMessage), Log::CRITICAL);
    else
        LogMsg(tr("Successfully moved torrent: %1. New path: %2").arg(name(), newPath));

    updateStatus();
    saveResumeData();

    while ((m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
        m_moveFinishedTriggers.takeFirst()();*/
}

void XDownHandleImpl::handleTrackerReplyAlert(const lt::tracker_reply_alert *p)
{
    //const QString trackerUrl(p->tracker_url());
    //qDebug("Received a tracker reply from %s (Num_peers = %d)", qUtf8Printable(trackerUrl), p->num_peers);
    //// Connection was successful now. Remove possible old errors
    //m_trackerInfos[trackerUrl] = {{}, p->num_peers};

    //m_session->handleTorrentTrackerReply(this, trackerUrl);
}

void XDownHandleImpl::handleTrackerWarningAlert(const lt::tracker_warning_alert *p)
{
    //const QString trackerUrl = p->tracker_url();
    //const QString message = p->warning_message();

    //// Connection was successful now but there is a warning message
    //m_trackerInfos[trackerUrl].lastMessage = message; // Store warning message

    //m_session->handleTorrentTrackerWarning(this, trackerUrl);
}

void XDownHandleImpl::handleTrackerErrorAlert(const lt::tracker_error_alert *p)
{
    //const QString trackerUrl = p->tracker_url();
    //const QString message = p->error_message();

    //m_trackerInfos[trackerUrl].lastMessage = message;

    //// Starting with libtorrent 1.2.x each tracker has multiple local endpoints from which
    //// an announce is attempted. Some endpoints might succeed while others might fail.
    //// Emit the signal only if all endpoints have failed.
    //const QVector<TrackerEntry> trackerList = trackers();
    //const auto iter = std::find_if(trackerList.cbegin(), trackerList.cend(), [&trackerUrl](const TrackerEntry &entry)
    //{
    //    return (entry.url() == trackerUrl);
    //});
    //if ((iter != trackerList.cend()) && (iter->status() == TrackerEntry::NotWorking))
    //    m_session->handleTorrentTrackerError(this, trackerUrl);
}

void XDownHandleImpl::handleTorrentCheckedAlert(const lt::torrent_checked_alert *p)
{
    /*Q_UNUSED(p);
    qDebug("\"%s\" have just finished checking", qUtf8Printable(name()));

    if (m_fastresumeDataRejected && !m_hasMissingFiles) {
        saveResumeData();
        m_fastresumeDataRejected = false;
    }

    updateStatus();

    if (!m_hasMissingFiles) {
        if ((progress() < 1.0) && (wantedSize() > 0))
            m_hasSeedStatus = false;
        else if (progress() == 1.0)
            m_hasSeedStatus = true;

        adjustActualSavePath();
        manageIncompleteFiles();
    }

    m_session->handleTorrentChecked(this);*/
}

void XDownHandleImpl::handleTorrentFinishedAlert(const lt::torrent_finished_alert *p)
{
    /*Q_UNUSED(p);
    qDebug("Got a torrent finished alert for \"%s\"", qUtf8Printable(name()));
    qDebug("Torrent has seed status: %s", m_hasSeedStatus ? "yes" : "no");
    m_hasMissingFiles = false;
    if (m_hasSeedStatus) return;

    updateStatus();
    m_hasSeedStatus = true;

    adjustActualSavePath();
    manageIncompleteFiles();

    const bool recheckTorrentsOnCompletion = Preferences::instance()->recheckTorrentsOnCompletion();
    if (isMoveInProgress() || (m_renameCount > 0)) {
        if (recheckTorrentsOnCompletion)
            m_moveFinishedTriggers.append([this]() { forceRecheck(); });
        m_moveFinishedTriggers.append([this]() { m_session->handleTorrentFinished(this); });
    }
    else {
        if (recheckTorrentsOnCompletion && m_unchecked)
            forceRecheck();
        m_session->handleTorrentFinished(this);
    }*/
}

void XDownHandleImpl::handleTorrentPausedAlert(const lt::torrent_paused_alert *p)
{
   /* Q_UNUSED(p);

    updateStatus();
    m_speedMonitor.reset();

    m_session->handleTorrentPaused(this);*/
}

void XDownHandleImpl::handleTorrentResumedAlert(const lt::torrent_resumed_alert *p)
{
    //Q_UNUSED(p);

    //m_session->handleTorrentResumed(this);
}

void XDownHandleImpl::handleSaveResumeDataAlert(const lt::save_resume_data_alert *p)
{
//#if (LIBTORRENT_VERSION_NUM < 10200)
//    const bool useDummyResumeData = !(p && p->resume_data);
//    auto resumeDataPtr = std::make_shared<lt::entry>(useDummyResumeData
//        ? lt::entry {}
//        : *(p->resume_data));
//#else
//    const bool useDummyResumeData = !p;
//    auto resumeDataPtr = std::make_shared<lt::entry>(useDummyResumeData
//        ? lt::entry {}
//        : lt::write_resume_data(p->params));
//#endif
//    lt::entry &resumeData = *resumeDataPtr;
//
//    updateStatus();
//
//    if (useDummyResumeData) {
//        resumeData["qBt-magnetUri"] = createMagnetURI().toStdString();
//        resumeData["paused"] = isPaused();
//        resumeData["auto_managed"] = isAutoManaged();
//        // Both firstLastPiecePriority and sequential need to be stored in the
//        // resume data if there is no metadata, otherwise they won't be
//        // restored if qBittorrent quits before the metadata are retrieved:
//        resumeData["qBt-firstLastPiecePriority"] = hasFirstLastPiecePriority();
//        resumeData["qBt-sequential"] = isSequentialDownload();
//
//        resumeData["qBt-addedTime"] = addedTime().toSecsSinceEpoch();
//    }
//    else {
//        const auto savePath = resumeData.find_key("save_path")->string();
//        resumeData["save_path"] = Profile::instance()->toPortablePath(QString::fromStdString(savePath)).toStdString();
//    }
//    resumeData["qBt-savePath"] = m_useAutoTMM ? "" : Profile::instance()->toPortablePath(m_savePath).toStdString();
//    resumeData["qBt-ratioLimit"] = static_cast<int>(m_ratioLimit * 1000);
//    resumeData["qBt-seedingTimeLimit"] = m_seedingTimeLimit;
//    resumeData["qBt-category"] = m_category.toStdString();
//    resumeData["qBt-tags"] = setToEntryList(m_tags);
//    resumeData["qBt-name"] = m_name.toStdString();
//    resumeData["qBt-seedStatus"] = m_hasSeedStatus;
//    resumeData["qBt-tempPathDisabled"] = m_tempPathDisabled;
//    resumeData["qBt-queuePosition"] = (static_cast<int>(nativeHandle().queue_position()) + 1); // qBt starts queue at 1
//    resumeData["qBt-hasRootFolder"] = m_hasRootFolder;
//
//#if (LIBTORRENT_VERSION_NUM < 10200)
//    if (m_nativeStatus.stop_when_ready) {
//#else
//    if (m_nativeStatus.flags & lt::torrent_flags::stop_when_ready) {
//#endif
//        // We need to redefine these values when torrent starting/rechecking
//        // in "paused" state since native values can be logically wrong
//        // (torrent can be not paused and auto_managed when it is checking).
//        resumeData["paused"] = true;
//        resumeData["auto_managed"] = false;
//    }
//
//    m_session->handleTorrentResumeDataReady(this, resumeDataPtr);
}

void XDownHandleImpl::handleSaveResumeDataFailedAlert(const lt::save_resume_data_failed_alert *p)
{
    //// if torrent has no metadata we should save dummy fastresume data
    //// containing Magnet URI and qBittorrent own resume data only
    //if (p->error.value() == lt::errors::no_metadata) {
    //    handleSaveResumeDataAlert(nullptr);
    //}
    //else {
    //    LogMsg(tr("Save resume data failed. Torrent: \"%1\", error: \"%2\"")
    //        .arg(name(), QString::fromLocal8Bit(p->error.message().c_str())), Log::CRITICAL);
    //    m_session->handleTorrentResumeDataFailed(this);
    //}
}

void XDownHandleImpl::handleFastResumeRejectedAlert(const lt::fastresume_rejected_alert *p)
{
    //m_fastresumeDataRejected = true;

    //if (p->error.value() == lt::errors::mismatching_file_size) {
    //    // Mismatching file size (files were probably moved)
    //    m_hasMissingFiles = true;
    //    LogMsg(tr("File sizes mismatch for torrent '%1', pausing it.").arg(name()), Log::CRITICAL);
    //}
    //else {
    //    LogMsg(tr("Fast resume data was rejected for torrent '%1'. Reason: %2. Checking again...")
    //        .arg(name(), QString::fromStdString(p->message())), Log::WARNING);
    //}
}

void XDownHandleImpl::handleFileRenamedAlert(const lt::file_renamed_alert *p)
{
//    // We don't really need to call updateStatus() in this place.
//    // All we need to do is make sure we have a valid instance of the TorrentInfo object.
//    m_torrentInfo = TorrentInfo {m_nativeHandle.torrent_file()};
//
//    // remove empty leftover folders
//    // for example renaming "a/b/c" to "d/b/c", then folders "a/b" and "a" will
//    // be removed if they are empty
//    const QString oldFilePath = m_oldPath[p->index].takeFirst();
//    const QString newFilePath = Utils::Fs::toUniformPath(p->new_name());
//
//    if (m_oldPath[p->index].isEmpty())
//        m_oldPath.remove(p->index);
//
//    QVector<QStringRef> oldPathParts = oldFilePath.splitRef('/', QString::SkipEmptyParts);
//    oldPathParts.removeLast();  // drop file name part
//    QVector<QStringRef> newPathParts = newFilePath.splitRef('/', QString::SkipEmptyParts);
//    newPathParts.removeLast();  // drop file name part
//
//#if defined(Q_OS_WIN)
//    const Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
//#else
//    const Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive;
//#endif
//
//    int pathIdx = 0;
//    while ((pathIdx < oldPathParts.size()) && (pathIdx < newPathParts.size())) {
//        if (oldPathParts[pathIdx].compare(newPathParts[pathIdx], caseSensitivity) != 0)
//            break;
//        ++pathIdx;
//    }
//
//    for (int i = (oldPathParts.size() - 1); i >= pathIdx; --i) {
//        QDir().rmdir(savePath() + Utils::String::join(oldPathParts, QLatin1String("/")));
//        oldPathParts.removeLast();
//    }
//
//    --m_renameCount;
//    while (!isMoveInProgress() && (m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
//        m_moveFinishedTriggers.takeFirst()();
//
//    if (isPaused() && (m_renameCount == 0))
//        saveResumeData();  // otherwise the new path will not be saved
}

void XDownHandleImpl::handleFileRenameFailedAlert(const lt::file_rename_failed_alert *p)
{
    //LogMsg(tr("File rename failed. Torrent: \"%1\", file: \"%2\", reason: \"%3\"")
    //    .arg(name(), filePath(static_cast<LTUnderlyingType<LTFileIndex>>(p->index))
    //         , QString::fromLocal8Bit(p->error.message().c_str())), Log::WARNING);

    //m_oldPath[p->index].removeFirst();
    //if (m_oldPath[p->index].isEmpty())
    //    m_oldPath.remove(p->index);

    //--m_renameCount;
    //while (!isMoveInProgress() && (m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
    //    m_moveFinishedTriggers.takeFirst()();

    //if (isPaused() && (m_renameCount == 0))
    //    saveResumeData();  // otherwise the new path will not be saved
}

void XDownHandleImpl::handleFileCompletedAlert(const lt::file_completed_alert *p)
{
    //// We don't really need to call updateStatus() in this place.
    //// All we need to do is make sure we have a valid instance of the TorrentInfo object.
    //m_torrentInfo = TorrentInfo {m_nativeHandle.torrent_file()};

    //qDebug("A file completed download in torrent \"%s\"", qUtf8Printable(name()));
    //if (m_session->isAppendExtensionEnabled()) {
    //    QString name = filePath(static_cast<LTUnderlyingType<LTFileIndex>>(p->index));
    //    if (name.endsWith(QB_EXT)) {
    //        const QString oldName = name;
    //        name.chop(QB_EXT.size());
    //        qDebug("Renaming %s to %s", qUtf8Printable(oldName), qUtf8Printable(name));
    //        renameFile(static_cast<LTUnderlyingType<LTFileIndex>>(p->index), name);
    //    }
    //}
}

void XDownHandleImpl::handleMetadataReceivedAlert(const lt::metadata_received_alert *p)
{
    //Q_UNUSED(p);
    //qDebug("Metadata received for torrent %s.", qUtf8Printable(name()));
    //updateStatus();
    //if (m_session->isAppendExtensionEnabled())
    //    manageIncompleteFiles();
    //if (!m_hasRootFolder)
    //    m_torrentInfo.stripRootFolder();
    //if (filesCount() == 1)
    //    m_hasRootFolder = false;
    //m_session->handleTorrentMetadataReceived(this);

    //if (isPaused()) {
    //    // XXX: Unfortunately libtorrent-rasterbar does not send a torrent_paused_alert
    //    // and the torrent can be paused when metadata is received
    //    m_speedMonitor.reset();
    //    m_session->handleTorrentPaused(this);
    //}

    //// If first/last piece priority was specified when adding this torrent, we can set it
    //// now that we have metadata:
    //if (m_needsToSetFirstLastPiecePriority) {
    //    setFirstLastPiecePriority(true);
    //    m_needsToSetFirstLastPiecePriority = false;
    //}
}

void XDownHandleImpl::handlePerformanceAlert(const lt::performance_alert *p) const
{
    /*LogMsg((tr("Performance alert: ") + QString::fromStdString(p->message()))
           , Log::INFO);*/
}

void XDownHandleImpl::handleTempPathChanged()
{
    //adjustActualSavePath();
}


void XDownHandleImpl::handleAppendExtensionToggled()
{
    /*if (!hasMetadata()) return;

    manageIncompleteFiles();*/
}

void XDownHandleImpl::handleAlert(const lt::alert *a)
{
    switch (a->type())
    {
    //case lt::file_renamed_alert::alert_type:
    //    handleFileRenamedAlert(static_cast<const lt::file_renamed_alert*>(a));
    //    break;
    //case lt::file_rename_failed_alert::alert_type:
    //    handleFileRenameFailedAlert(static_cast<const lt::file_rename_failed_alert*>(a));
    //    break;
    //case lt::file_completed_alert::alert_type:
    //    // TODO
    //    handleFileCompletedAlert(static_cast<const lt::file_completed_alert*>(a));
    //    break;
    //case lt::torrent_finished_alert::alert_type:
    //    handleTorrentFinishedAlert(static_cast<const lt::torrent_finished_alert*>(a));
    //    break;
    case lt::save_resume_data_alert::alert_type:
        handleSaveResumeDataAlert(static_cast<const lt::save_resume_data_alert*>(a));
        break;
    case lt::save_resume_data_failed_alert::alert_type:
        handleSaveResumeDataFailedAlert(static_cast<const lt::save_resume_data_failed_alert*>(a));
        break;
    //case lt::torrent_paused_alert::alert_type:
    //    handleTorrentPausedAlert(static_cast<const lt::torrent_paused_alert*>(a));
    //    break;
    //case lt::torrent_resumed_alert::alert_type:
    //    handleTorrentResumedAlert(static_cast<const lt::torrent_resumed_alert*>(a));
    //    break;
    //case lt::tracker_error_alert::alert_type:
    //    handleTrackerErrorAlert(static_cast<const lt::tracker_error_alert*>(a));
    //    break;
    //case lt::tracker_reply_alert::alert_type:
    //    handleTrackerReplyAlert(static_cast<const lt::tracker_reply_alert*>(a));
    //    break;
    //case lt::tracker_warning_alert::alert_type:
    //    handleTrackerWarningAlert(static_cast<const lt::tracker_warning_alert*>(a));
    //    break;
    //case lt::metadata_received_alert::alert_type:
    //    handleMetadataReceivedAlert(static_cast<const lt::metadata_received_alert*>(a));
    //    break;
    //case lt::fastresume_rejected_alert::alert_type:
    //    handleFastResumeRejectedAlert(static_cast<const lt::fastresume_rejected_alert*>(a));
    //    break;
    //case lt::torrent_checked_alert::alert_type:
    //    handleTorrentCheckedAlert(static_cast<const lt::torrent_checked_alert*>(a));
    //    break;
    case lt::performance_alert::alert_type:
        handlePerformanceAlert(static_cast<const lt::performance_alert*>(a));
        break;
    }
}

void XDownHandleImpl::manageIncompleteFiles()
{
    /*const bool isAppendExtensionEnabled = m_session->isAppendExtensionEnabled();
    const QVector<qreal> fp = filesProgress();
    if (fp.size() != filesCount()) {
        qDebug() << "skip manageIncompleteFiles because of invalid torrent meta-data or empty file-progress";
        return;
    }

    for (int i = 0; i < filesCount(); ++i) {
        QString name = filePath(i);
        if (isAppendExtensionEnabled && (fileSize(i) > 0) && (fp[i] < 1)) {
            if (!name.endsWith(QB_EXT)) {
                const QString newName = name + QB_EXT;
                qDebug() << "Renaming" << name << "to" << newName;
                renameFile(i, newName);
            }
        }
        else {
            if (name.endsWith(QB_EXT)) {
                const QString oldName = name;
                name.chop(QB_EXT.size());
                qDebug() << "Renaming" << oldName << "to" << name;
                renameFile(i, name);
            }
        }
    }*/
}

void XDownHandleImpl::adjustActualSavePath()
{
    /*if (!isMoveInProgress())
        adjustActualSavePath_impl();
    else
        m_moveFinishedTriggers.append([this]() { adjustActualSavePath_impl(); });*/
}

void XDownHandleImpl::adjustActualSavePath_impl()
{
    //const bool needUseTempDir = useTempPath();
    //const QDir tempDir {m_session->torrentTempPath(info())};
    //const QDir currentDir {actualStorageLocation()};
    //const QDir targetDir {needUseTempDir ? tempDir : QDir {savePath()}};

    //if (targetDir == currentDir) return;

    //if (!needUseTempDir) {
    //    if ((currentDir == tempDir) && (currentDir != QDir {m_session->tempPath()})) {
    //        // torrent without root folder still has it in its temporary save path
    //        // so its temp path isn't equal to temp path root
    //        const QString currentDirPath = currentDir.absolutePath();
    //        m_moveFinishedTriggers.append([currentDirPath]
    //        {
    //            qDebug() << "Removing torrent temp folder:" << currentDirPath;
    //            Utils::Fs::smartRemoveEmptyFolderTree(currentDirPath);
    //        });
    //    }
    //}

    //moveStorage(Utils::Fs::toNativePath(targetDir.absolutePath()), MoveStorageMode::Overwrite);
}

//lt::torrent_handle XDownHandleImpl::nativeHandle() const
//{
//    return m_nativeHandle;
//}

void XDownHandleImpl::updateTorrentInfo()
{
    /*if (!hasMetadata()) return;

    m_torrentInfo = TorrentInfo(m_nativeStatus.torrent_file.lock());*/
}

bool XDownHandleImpl::isMoveInProgress() const
{
    return m_storageIsMoving;
}

bool XDownHandleImpl::useTempPath() const
{
    return !m_tempPathDisabled && m_session->isTempPathEnabled() && !(isSeed() || m_hasSeedStatus);
}

void XDownHandleImpl::updateStatus()
{
    //updateStatus(m_nativeHandle.status());
}

void XDownHandleImpl::updateStatus(const lt::torrent_status &nativeStatus)
{
    //m_nativeStatus = nativeStatus;

    //updateState();
    //updateTorrentInfo();

    //// NOTE: Don't change the order of these conditionals!
    //// Otherwise it will not work properly since torrent can be CheckingDownloading.
    //if (isChecking())
    //    m_unchecked = false;
    //else if (isDownloading())
    //    m_unchecked = true;

    //m_speedMonitor.addSample({nativeStatus.download_payload_rate
    //    , nativeStatus.upload_payload_rate});
}

void XDownHandleImpl::setRatioLimit(qreal limit)
{
    /*if (limit < USE_GLOBAL_RATIO)
        limit = NO_RATIO_LIMIT;
    else if (limit > MAX_RATIO)
        limit = MAX_RATIO;

    if (m_ratioLimit != limit) {
        m_ratioLimit = limit;
        m_session->handleTorrentShareLimitChanged(this);
    }*/
}

void XDownHandleImpl::setSeedingTimeLimit(int limit)
{
    /*if (limit < USE_GLOBAL_SEEDING_TIME)
        limit = NO_SEEDING_TIME_LIMIT;
    else if (limit > MAX_SEEDING_TIME)
        limit = MAX_SEEDING_TIME;

    if (m_seedingTimeLimit != limit) {
        m_seedingTimeLimit = limit;
        m_session->handleTorrentShareLimitChanged(this);
    }*/
}

void XDownHandleImpl::setUploadLimit(const int limit)
{
    //m_nativeHandle.set_upload_limit(limit);
}

// TODO 
void XDownHandleImpl::setDownloadLimit(const int limit)
{
    //m_nativeHandle.set_download_limit(limit);
}

void XDownHandleImpl::setSuperSeeding(const bool enable)
{
//#if (LIBTORRENT_VERSION_NUM < 10200)
//    m_nativeHandle.super_seeding(enable);
//#else
//    if (enable)
//        m_nativeHandle.set_flags(lt::torrent_flags::super_seeding);
//    else
//        m_nativeHandle.unset_flags(lt::torrent_flags::super_seeding);
//#endif
}

void XDownHandleImpl::flushCache() const
{
    //m_nativeHandle.flush_cache();
}

QString XDownHandleImpl::createMagnetURI() const
{
    return m_url;
}

void XDownHandleImpl::prioritizeFiles(const QVector<DownloadPriority> &priorities)
{
//    if (!hasMetadata()) return;
//    if (priorities.size() != filesCount()) return;
//
//    // Save first/last piece first option state
//    const bool firstLastPieceFirst = hasFirstLastPiecePriority();
//
//    // Reset 'm_hasSeedStatus' if needed in order to react again to
//    // 'torrent_finished_alert' and eg show tray notifications
//    const QVector<qreal> progress = filesProgress();
//    const QVector<DownloadPriority> oldPriorities = filePriorities();
//    for (int i = 0; i < oldPriorities.size(); ++i) {
//        if ((oldPriorities[i] == DownloadPriority::Ignored)
//            && (priorities[i] > DownloadPriority::Ignored)
//            && (progress[i] < 1.0)) {
//            m_hasSeedStatus = false;
//            break;
//        }
//    }
//
//    qDebug() << Q_FUNC_INFO << "Changing files priorities...";
//    m_nativeHandle.prioritize_files(toLTDownloadPriorities(priorities));
//
//    qDebug() << Q_FUNC_INFO << "Moving unwanted files to .unwanted folder and conversely...";
//    const QString spath = savePath(true);
//    for (int i = 0; i < priorities.size(); ++i) {
//        const QString filepath = filePath(i);
//        // Move unwanted files to a .unwanted subfolder
//        if (priorities[i] == DownloadPriority::Ignored) {
//            const QString oldAbsPath = QDir(spath).absoluteFilePath(filepath);
//            const QString parentAbsPath = Utils::Fs::branchPath(oldAbsPath);
//            // Make sure the file does not already exists
//            if (QDir(parentAbsPath).dirName() != ".unwanted") {
//                const QString unwantedAbsPath = parentAbsPath + "/.unwanted";
//                const QString newAbsPath = unwantedAbsPath + '/' + Utils::Fs::fileName(filepath);
//                qDebug() << "Unwanted path is" << unwantedAbsPath;
//                if (QFile::exists(newAbsPath)) {
//                    qWarning() << "File" << newAbsPath << "already exists at destination.";
//                    continue;
//                }
//
//                const bool created = QDir().mkpath(unwantedAbsPath);
//                qDebug() << "unwanted folder was created:" << created;
//#ifdef Q_OS_WIN
//                if (created) {
//                    // Hide the folder on Windows
//                    qDebug() << "Hiding folder (Windows)";
//                    std::wstring winPath = Utils::Fs::toNativePath(unwantedAbsPath).toStdWString();
//                    DWORD dwAttrs = ::GetFileAttributesW(winPath.c_str());
//                    bool ret = ::SetFileAttributesW(winPath.c_str(), dwAttrs | FILE_ATTRIBUTE_HIDDEN);
//                    Q_ASSERT(ret != 0); Q_UNUSED(ret);
//                }
//#endif
//                QString parentPath = Utils::Fs::branchPath(filepath);
//                if (!parentPath.isEmpty() && !parentPath.endsWith('/'))
//                    parentPath += '/';
//                renameFile(i, parentPath + ".unwanted/" + Utils::Fs::fileName(filepath));
//            }
//        }
//
//        // Move wanted files back to their original folder
//        if (priorities[i] > DownloadPriority::Ignored) {
//            const QString parentRelPath = Utils::Fs::branchPath(filepath);
//            if (QDir(parentRelPath).dirName() == ".unwanted") {
//                const QString oldName = Utils::Fs::fileName(filepath);
//                const QString newRelPath = Utils::Fs::branchPath(parentRelPath);
//                if (newRelPath.isEmpty())
//                    renameFile(i, oldName);
//                else
//                    renameFile(i, QDir(newRelPath).filePath(oldName));
//
//                // Remove .unwanted directory if empty
//                qDebug() << "Attempting to remove .unwanted folder at " << QDir(spath + '/' + newRelPath).absoluteFilePath(".unwanted");
//                QDir(spath + '/' + newRelPath).rmdir(".unwanted");
//            }
//        }
//    }
//
//    // Restore first/last piece first option if necessary
//    if (firstLastPieceFirst)
//        setFirstLastPiecePriorityImpl(true, priorities);
}

QVector<qreal> XDownHandleImpl::availableFileFractions() const
{
    //const int filesCount = this->filesCount();
    //if (filesCount < 0) return {};

    //const QVector<int> piecesAvailability = pieceAvailability();
    //// libtorrent returns empty array for seeding only torrents
    //if (piecesAvailability.empty()) return QVector<qreal>(filesCount, -1.);

    QVector<qreal> res;
    //res.reserve(filesCount);
    //const TorrentInfo info = this->info();
    //for (int i = 0; i < filesCount; ++i) {
    //    const TorrentInfo::PieceRange filePieces = info.filePieces(i);

    //    int availablePieces = 0;
    //    for (int piece = filePieces.first(); piece <= filePieces.last(); ++piece) {
    //        availablePieces += (piecesAvailability[piece] > 0) ? 1 : 0;
    //    }
    //    res.push_back(static_cast<qreal>(availablePieces) / filePieces.size());
    //}
    return res;
}
