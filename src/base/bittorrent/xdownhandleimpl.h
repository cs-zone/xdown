
#pragma once

#include <functional>

#include <libtorrent/fwd.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QVector>
#include <QMap>

#include "speedmonitor.h"
#include "infohash.h"
#include "torrenthandle.h"
#include "torrentinfo.h"
#include "session_struct.h"


namespace BitTorrent
{
    class Session;

    struct CreateXDownParams
    {
        CreateXDownParams() = default;
        // source 输入url原始参数
        // 以下2个大小的值用于从下载配置中加载
        // fileSize 文件大小
        // completedSize 已完成的大小
        explicit CreateXDownParams(const QString &source, const qlonglong fileSize = 0, qlonglong completedSize = 0);
        int ParseRequestHeader(const QString &strParam);
        int ParseDelRequestHeader(const QString &strParam);
        int ParseDealWithRequestHeader();
        int ParseRequestUriOption(const QString &strParam);


        // for both new and restored torrents
        QString name;

        QString uriFileName;

#ifdef __ENABLE_CATEGORY__
        QString category;
#endif

        QString savePath;

        int downloadLimit = -1;
        bool forced = false;
        bool paused = false;

        qlonglong fileSize = 0;
        qlonglong completedSize = 0;

        qlonglong fileIndex = 0;

        int loadFromCfg = 0;

        //下载并发数
        int downConcurrent = 0;

        // 下载url
        QString url;

        // 用户输入的原始url以及参数
        QString source;

        // http header
        QMap<QString, QString> reqHeaderMap;
        QVector<QString>       reqDelHeaderList;

        // aria2 uri option
        QMap<QString, QString> reqUriOptionMap;

        // 请求内置参数，不解析外部
        QMap<QString, QString> defUriOptionMap;

        qint64 m_addedTime;
        qint64 m_completeTime;

        bool restored = false;

        // for restored torrents
        // 是否限速
        qreal ratioLimit = TorrentHandle::USE_GLOBAL_RATIO;

        // ui传递进来的，需要保存到配置里面
        QMap<QString, QString> uiHeaderMap;
        QMap<QString, QString> uiOptionMap;
    };

    class XDownHandleImpl final : public QObject, public TorrentHandle
    {
        Q_DISABLE_COPY(XDownHandleImpl)
        Q_DECLARE_TR_FUNCTIONS(BitTorrent::XDownHandleImpl)

    public:
        XDownHandleImpl(Session *session, const CreateXDownParams &params);
        ~XDownHandleImpl() override;

        bool operator<(const XDownHandleImpl& o) const {
            if (m_retryTimestamp > o.m_retryTimestamp) return true;
            return false;
        }

        bool isRuningTask() {
            return m_session != nullptr;
        }

        // get file info times 
        int getRetryFileInfo() { return m_retryFileInfoTimes; }
        void setRetryFileInfo(int iVal) {  m_retryFileInfoTimes = iVal; }
        void increaseRetryFileInfo() { m_retryFileInfoTimes++; }

        qlonglong getRetryFileInfoTimestamp() { return m_retryFileInfoTimestamp; }
        void setRetryFileInfoTimestamp(qlonglong iVal) { m_retryFileInfoTimestamp = iVal; }

        bool isValid() const;

        void clearPeers() override;

        TaskHandleType getHandleType() const override;

        qint64 getRetryTimestamp() const {  return m_retryTimestamp; }
        void setRetryTimestamp(qint64 iValue) {  m_retryTimestamp = iValue; }

        qlonglong getFileIndex() { return m_fileIndex;  }

        void setState(BitTorrent::TorrentState iVal) {m_state = iVal;}

        QString url() const override;
        QString source() const override;

        InfoHash hash() const override;
        QString name() const override;

        void setHandleActionType(TaskHandleActionType iValue) override;
        TaskHandleActionType getHandleActionType() const override;

        bool checkIsUpdateHandle() const override;

        aria2::A2Gid getGid() const override;
        void setGid(const aria2::A2Gid m_value) override;

        void setDownloadStatus(aria2::DownloadStatus dStatus) {m_downloadStatus = dStatus;}
        aria2::DownloadStatus getDownloadStatus() { return m_downloadStatus; }

        int getCompleteActionType() { return m_completeActionType; }
        QString getCompleteActionBuffer1() const { return m_completeActionBuffer1; }
        QString getCompleteActionBuffer2() const { return m_completeActionBuffer2; }
        QString getCompleteActionBuffer3() const { return m_completeActionBuffer3; }

        void setCompleteActionType(int iVal) { m_completeActionType = iVal; }
        void setCompleteActionBuffer1(const QString &strVal) { m_completeActionBuffer1 = strVal; }
        void setCompleteActionBuffer2(const QString &strVal) { m_completeActionBuffer2 = strVal; }
        void setCompleteActionBuffer3(const QString &strVal) { m_completeActionBuffer3 = strVal; }


        qint64 getRefreshWidgetTimestamp() { return m_refreshWidgetTimestamp; }
        void setRefreshWidgetTimestamp(qint64 iVal) { m_refreshWidgetTimestamp = iVal; }

        // 更新文件大小
        void setTotalSize(qlonglong iValue);
        // 更新文件已完成大小
        void setCompletedSize(qlonglong iValue);

        void setDownSpeed(long iValue);
        long getDownSpeed() const;
        QString getFmtDownSpeed() const;

        int getRetryValue();
        void setRetryValue(int iValue);
        void addRetryValue();

        // 下载并发数
        void setDownConcurrent(int iValue) { m_downConcurrent = iValue; }
        int getDownConcurrent() const { return m_downConcurrent; }

        // uint64_t  m_downIndex = 0 ;
        // 下载序号
        void setDownIndex(uint64_t iValue) { m_downIndex = iValue; }
        uint64_t getDownIndex() const { return m_downIndex; }

        // 刷新速度的时间戳
        void setItemProcessTick(qint64 lValue) { m_lItemProcessTick = lValue; }
        qint64 getItemProcessTick() const { return m_lItemProcessTick; }

        void startParsePathAndFileName(const QString &strFullFileName);

        QDateTime creationDate() const override;
        QString creator() const override;
        QString comment() const override;
        bool isPrivate() const override;
        qlonglong totalSize() const override;
        qlonglong wantedSize() const override;
        qlonglong completedSize() const override;
        //qlonglong incompletedSize() const override;
        qlonglong pieceLength() const override;
        qlonglong wastedSize() const override;
        QString currentTracker() const override;

        QString savePath(bool actual = false) const override;
        QString rootPath(bool actual = false) const override;
        QString contentPath(bool actual = false) const override;

        bool useTempPath() const override;

        bool isAutoTMMEnabled() const override;
        void setAutoTMMEnabled(bool enabled) override;


        //bool hasRootFolder() const override;

        int filesCount() const override;
        int piecesCount() const override;
        int piecesHave() const override;
        qreal progress() const override;
        QDateTime addedTime() const override;
        void setAddedTime(qint64 val);

        qreal ratioLimit() const override;
        int seedingTimeLimit() const override;

        QString filePath(int index) const override;
        QString fileName(int index) const override;

        qlonglong fileSize(int index) const override;
        QStringList absoluteFilePaths() const override;
        QVector<DownloadPriority> filePriorities() const override;

        TorrentInfo info() const override;
        bool isSeed() const override;
        bool isPaused() const override;
        //bool isResumed() const override;
        bool isQueued() const override;
        bool isForced() const override;
        bool isChecking() const override;
        bool isDownloading() const override;
        bool isUploading() const override;
        bool isCompleted() const override;
        bool isActive() const override;
        bool isInactive() const override;
        bool isErrored() const override;
        bool isSequentialDownload() const override;
        bool hasFirstLastPiecePriority() const override;
        TorrentState state() const override;
        bool hasMetadata() const override;
        bool hasMissingFiles() const override;
        bool hasError() const override;
        bool hasFilteredPieces() const override;
        int queuePosition() const override;
        QVector<TrackerEntry> trackers() const override;
        QHash<QString, TrackerInfo> trackerInfos() const override;
        QVector<QUrl> urlSeeds() const override;
        QString error() const override;

        void setErrorCode(int iValue) { m_errorCode = iValue; }
        int getErrorCode() const { return m_errorCode; }

        void setErrorMessage(const QString &strValue) override;
        QString getErrorMessage() const override;


        qlonglong totalDownload() const override;
        qlonglong totalUpload() const override;
        qlonglong activeTime() const override;
        qlonglong finishedTime() const override;
        qlonglong seedingTime() const override;
        qlonglong eta() const override;
        QVector<qreal> filesProgress() const override;
        int seedsCount() const override;
        int peersCount() const override;
        int leechsCount() const override;
        int totalSeedsCount() const override;
        int totalPeersCount() const override;
        int totalLeechersCount() const override;
        int completeCount() const override;
        int incompleteCount() const override;
        QDateTime lastSeenComplete() const override;
        QDateTime completedTime() const override;
        void setCompletedTime(qint64 val);
        qlonglong timeSinceUpload() const override;
        qlonglong timeSinceDownload() const override;
        qlonglong timeSinceActivity() const override;
        int downloadLimit() const override;
        int uploadLimit() const override;
        bool superSeeding() const override;
        QVector<PeerInfo> peers() const override;
        QBitArray pieces() const override;
        QBitArray downloadingPieces() const override;
        QVector<int> pieceAvailability() const override;
        qreal distributedCopies() const override;
        qreal maxRatio() const override;
        int maxSeedingTime() const override;
        qreal realRatio() const override;
        int uploadPayloadRate() const override;
        int downloadPayloadRate() const override;
        qlonglong totalPayloadUpload() const override;
        qlonglong totalPayloadDownload() const override;
        int connectionsCount() const override;
        int connectionsLimit() const override;
        qlonglong nextAnnounce() const override;
        QVector<qreal> availableFileFractions() const override;
        void setName(const QString &name) override;

        void setSavePath(const QString &strValue);
        QString getSavePath() const { return m_savePath; }

        QString getFileName() const { return m_name; }

        void setDesotryTick()  { m_destoryTick = QDateTime::currentMSecsSinceEpoch(); }
        qint64 getDestoryTick() { return m_destoryTick;  }

        QString getItemHash() const override;
        void setItemHash(const QString &strHash) {
            m_itemHash = strHash;
        }
        void setDeleteOption(DeleteOption dValue) { m_deleteOption = dValue; }
        DeleteOption getDeleteOption() { return m_deleteOption; }

        void setSequentialDownload(bool enable) override;
        void setFirstLastPiecePriority(bool enabled) override;
        void pause() override;
        //void resume(bool forced = false) override;
        void resume(TorrentOperatingMode mode = TorrentOperatingMode::AutoManaged) override;
        void move(QString path) override;
        void forceReannounce(int index = -1) override;
        void forceDHTAnnounce() override;
        void forceRecheck() override;
        void renameFile(int index, const QString &name) override;
        void prioritizeFiles(const QVector<DownloadPriority> &priorities) override;
        void setRatioLimit(qreal limit) override;
        void setSeedingTimeLimit(int limit) override;
        void setUploadLimit(int limit) override;
        void setDownloadLimit(int limit) override;
        void setSuperSeeding(bool enable) override;
        void flushCache() const override;
        void addTrackers(const QVector<TrackerEntry> &trackers) override;
        void replaceTrackers(const QVector<TrackerEntry> &trackers) override;
        void addUrlSeeds(const QVector<QUrl> &urlSeeds) override;
        void removeUrlSeeds(const QVector<QUrl> &urlSeeds) override;
        bool connectPeer(const PeerAddress &peerAddress) override;

        QString createMagnetURI() const override;

        bool needSaveResumeData() const;

        // Session interface
        //lt::torrent_handle nativeHandle() const;

        void handleAlert(const lt::alert *a);
        void handleStateUpdate(const lt::torrent_status &nativeStatus);
        void handleTempPathChanged();

        void handleAppendExtensionToggled();
        void saveResumeData();
        void handleStorageMoved(const QString &newPath, const QString &errorMessage);

        void updateState(aria2::DownloadEvent dEvent, long iValue = 0, const QString &strErrMessage = "");

        aria2::DownloadEvent getDownloadEvent();

public:
        QMap<QString, QString> getReqHeaderMap() { return m_reqHeaderMap; }
        QMap<QString, QString> getReqUriOptionMap() { return m_reqUriOptionMap; }
        QMap<QString, QString> getDefUriOptionMap() { return m_defUriOptionMap; }

        QMap<QString, QString>* getReqUriOptionMapPtr() { return &m_reqUriOptionMap; }

        QMap<QString, QString> getUIHeaderMap() { return m_UIHeaderMap; }
        QMap<QString, QString> getUIOptionMap() { return m_UIOptionMap; }
    private:
        typedef std::function<void ()> EventTrigger;

#if (LIBTORRENT_VERSION_NUM < 10200)
        using LTFileIndex = int;
#else
        using LTFileIndex = lt::file_index_t;
#endif

        void updateStatus();
        void updateStatus(const lt::torrent_status &nativeStatus);
        void updateState();

        

        void updateTorrentInfo();

        void handleFastResumeRejectedAlert(const lt::fastresume_rejected_alert *p);
        void handleFileCompletedAlert(const lt::file_completed_alert *p);
        void handleFileRenamedAlert(const lt::file_renamed_alert *p);
        void handleFileRenameFailedAlert(const lt::file_rename_failed_alert *p);
        void handleMetadataReceivedAlert(const lt::metadata_received_alert *p);
        void handlePerformanceAlert(const lt::performance_alert *p) const;
        void handleSaveResumeDataAlert(const lt::save_resume_data_alert *p);
        void handleSaveResumeDataFailedAlert(const lt::save_resume_data_failed_alert *p);
        void handleTorrentCheckedAlert(const lt::torrent_checked_alert *p);
        void handleTorrentFinishedAlert(const lt::torrent_finished_alert *p);
        void handleTorrentPausedAlert(const lt::torrent_paused_alert *p);
        void handleTorrentResumedAlert(const lt::torrent_resumed_alert *p);
        void handleTrackerErrorAlert(const lt::tracker_error_alert *p);
        void handleTrackerReplyAlert(const lt::tracker_reply_alert *p);
        void handleTrackerWarningAlert(const lt::tracker_warning_alert *p);

        void resume_impl(bool forced);
        bool isMoveInProgress() const;
        QString actualStorageLocation() const;
        bool isAutoManaged() const;
        void setAutoManaged(bool enable);

        void adjustActualSavePath();
        void adjustActualSavePath_impl();
        //void move_impl(QString path, MoveStorageMode mode);
        //void moveStorage(const QString &newPath, MoveStorageMode mode);
        void manageIncompleteFiles();
        void setFirstLastPiecePriorityImpl(bool enabled, const QVector<DownloadPriority> &updatedFilePrio = {});

        Session *const m_session;
        //lt::torrent_handle m_nativeHandle;
        lt::torrent_status m_nativeStatus;

        // 下载状态
        TorrentState m_state = TorrentState::XDown_Paused;

        aria2::DownloadEvent m_event = aria2::DownloadEvent::EVENT_ON_DOWNLOAD_NONE;

        aria2::DownloadStatus m_downloadStatus = aria2::DownloadStatus::DOWNLOAD_NONE;

        int m_errorCode = 0;
        QString m_errorMessage = "";

        // disk fileName save path 
        QString m_filePath = "";

        qint64  m_destoryTick = 0;

        DeleteOption m_deleteOption = DeleteOption::Torrent;


        QMap<QString, QString> m_reqHeaderMap;
        QMap<QString, QString> m_reqUriOptionMap;

        QMap<QString, QString> m_defUriOptionMap;


        QMap<QString, QString> m_UIHeaderMap;
        QMap<QString, QString> m_UIOptionMap;
        

        int  m_downConcurrent = 16;

        uint64_t  m_downIndex = 0 ;

        TorrentInfo m_torrentInfo;
        SpeedMonitor m_speedMonitor;

        // m_moveFinishedTriggers is activated only when the following conditions are met:
        // all file rename jobs complete, all file move jobs complete
        QQueue<EventTrigger> m_moveFinishedTriggers;
        int m_renameCount = 0;
        bool m_storageIsMoving = false;

        // Until libtorrent provide an "old_name" field in `file_renamed_alert`
        // we will rely on this workaround to remove empty leftover folders
       //  QHash<LTFileIndex, QVector<QString>> m_oldPath;

        TaskHandleType m_handleType;

        TaskHandleActionType m_taskHandleActionType = TaskHandleActionType::ActionType_None;

        long m_downSpeed = 0;

        qint64 m_lItemProcessTick = 0;

        InfoHash m_hash;

        aria2::A2Gid m_gid = 0;

        QString m_itemHash = "";

        QString m_name;


        QString m_url;


        QString m_source;


        qlonglong m_fileIndex = 0;
        int m_retryValue = 0;

        QString m_savePath;
        QString m_category;
        QSet<QString> m_tags;
        qreal m_ratioLimit;

        qlonglong m_fileSize;
        qlonglong m_completedSize;

        QHash<QString, TrackerInfo> m_trackerInfos;

        int m_seedingTimeLimit;
        bool m_hasSeedStatus;
        bool m_tempPathDisabled;
        bool m_fastresumeDataRejected = false;
        bool m_hasMissingFiles = false;
        bool m_hasRootFolder;
        bool m_needsToSetFirstLastPiecePriority = false;
        bool m_useAutoTMM;

        bool m_unchecked = false;

        bool m_paused = false;

        int m_completeActionType;
        QString m_completeActionBuffer1;
        QString m_completeActionBuffer2;
        QString m_completeActionBuffer3;

        qint64 added_time = 0;
        qint64 completed_time = 0;

        qint64 m_retryTimestamp = 0;
        qint64 m_refreshWidgetTimestamp = 0;

        int m_retryFileInfoTimes = 0;
        qint64 m_retryFileInfoTimestamp = 0;
    };
}
