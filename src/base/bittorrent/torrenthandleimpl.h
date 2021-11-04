/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#pragma once

#include <functional>

#include <libtorrent/add_torrent_params.hpp>
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

#include "infohash.h"
#include "speedmonitor.h"
#include "torrenthandle.h"
#include "torrentinfo.h"

namespace BitTorrent
{
    class Session;
    struct AddTorrentParams;

    struct LoadTorrentParams
    {
        lt::add_torrent_params ltAddTorrentParams {};

        QString name;
#ifdef __ENABLE_CATEGORY__
        QString category;
        QSet<QString> tags;
#endif
        QString savePath;
        TorrentContentLayout contentLayout = TorrentContentLayout::Original;
        bool firstLastPiecePriority = false;
        bool hasSeedStatus = false;
        bool forced = false;
        bool paused = false;


        qreal ratioLimit = TorrentHandle::USE_GLOBAL_RATIO;
        int seedingTimeLimit = TorrentHandle::USE_GLOBAL_SEEDING_TIME;

        bool restored = false;  // is existing torrent job?
    };

    enum class MoveStorageMode
    {
        KeepExistingFiles,
        Overwrite
    };

    enum class MaintenanceJob
    {
        None,
        HandleMetadata
    };

    class TorrentHandleImpl final : public QObject, public TorrentHandle
    {
        Q_DISABLE_COPY(TorrentHandleImpl)
        Q_DECLARE_TR_FUNCTIONS(BitTorrent::TorrentHandleImpl)

    public:
        TorrentHandleImpl(Session *session, lt::session *nativeSession
                          , const lt::torrent_handle &nativeHandle, const LoadTorrentParams &params);
        ~TorrentHandleImpl() override;

        bool isValid() const;




        TaskHandleType getHandleType() const override;

        void setHandleActionType(TaskHandleActionType iValue) override;
        TaskHandleActionType getHandleActionType() const override;

        bool checkIsUpdateHandle() const override;

        aria2::A2Gid getGid() const override;
        void setGid(const aria2::A2Gid m_value) override;

        QString url() const override;
        QString source() const override;
		
		void setErrorMessage(const QString &strValue) override;
        QString getErrorMessage() const override;

        QString getItemHash() const override;
		

        InfoHash hash() const override;
        QString name() const override;
        QDateTime creationDate() const override;
        QString creator() const override;
        QString comment() const override;
        bool isPrivate() const override;
        qlonglong totalSize() const override;
        qlonglong wantedSize() const override;
        qlonglong completedSize() const override;
        qlonglong pieceLength() const override;
        qlonglong wastedSize() const override;
        QString currentTracker() const override;

        QString savePath(bool actual = false) const override;
        QString rootPath(bool actual = false) const override;
        QString contentPath(bool actual = false) const override;

        bool useTempPath() const override;

        bool isAutoTMMEnabled() const override;
        void setAutoTMMEnabled(bool enabled) override;
#ifdef __ENABLE_CATEGORY__
        QString category() const override;
        bool belongsToCategory(const QString &category) const override;
        bool setCategory(const QString &category) override;

        QSet<QString> tags() const override;
        bool hasTag(const QString &tag) const override;
        bool addTag(const QString &tag) override;
        bool removeTag(const QString &tag) override;
        void removeAllTags() override;
#endif





        int filesCount() const override;
        int piecesCount() const override;
        int piecesHave() const override;
        qreal progress() const override;
        QDateTime addedTime() const override;
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
        void setSequentialDownload(bool enable) override;
        void setFirstLastPiecePriority(bool enabled) override;
        void pause() override;
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
        void clearPeers() override;

        QString createMagnetURI() const override;

        bool needSaveResumeData() const;

        // Session interface
        lt::torrent_handle nativeHandle() const;

        void handleAlert(const lt::alert *a);
        void handleStateUpdate(const lt::torrent_status &nativeStatus);
        void handleTempPathChanged();
#ifdef __ENABLE_CATEGORY__
        void handleCategorySavePathChanged();
#endif
        void handleAppendExtensionToggled();
        void saveResumeData();
        void handleMoveStorageJobFinished(bool hasOutstandingJob);
        void fileSearchFinished(const QString &savePath, const QStringList &fileNames);

        QString actualStorageLocation() const;

    private:
        typedef std::function<void ()> EventTrigger;

        void updateStatus();
        void updateStatus(const lt::torrent_status &nativeStatus);
        void updateState();

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

        bool isMoveInProgress() const;

        void setAutoManaged(bool enable);

        void adjustActualSavePath();
        void adjustActualSavePath_impl();
        void move_impl(QString path, MoveStorageMode mode);
        void moveStorage(const QString &newPath, MoveStorageMode mode);
        void manageIncompleteFiles();
        void applyFirstLastPiecePriority(bool enabled, const QVector<DownloadPriority> &updatedFilePrio = {});

        void endReceivedMetadataHandling(const QString &savePath, const QStringList &fileNames);

        Session *const m_session;
        lt::session *m_nativeSession;
        lt::torrent_handle m_nativeHandle;
        lt::torrent_status m_nativeStatus;
        TorrentState m_state = TorrentState::Unknown;
        TorrentInfo m_torrentInfo;
        SpeedMonitor m_speedMonitor;

        InfoHash m_hash;

        // m_moveFinishedTriggers is activated only when the following conditions are met:
        // all file rename jobs complete, all file move jobs complete
        QQueue<EventTrigger> m_moveFinishedTriggers;
        int m_renameCount = 0;
        bool m_storageIsMoving = false;

        MaintenanceJob m_maintenanceJob = MaintenanceJob::None;

        // Until libtorrent provide an "old_name" field in `file_renamed_alert`
        // we will rely on this workaround to remove empty leftover folders
        QHash<lt::file_index_t, QVector<QString>> m_oldPath;

        QHash<QString, TrackerInfo> m_trackerInfos;


        // handle ����
        TaskHandleType m_handleType;
		
		

        // Persistent data
        QString m_name;
        QString m_savePath;
        QString m_category;
        QSet<QString> m_tags;
        qreal m_ratioLimit;
        int m_seedingTimeLimit;
        TorrentOperatingMode m_operatingMode;
        TorrentContentLayout m_contentLayout;
        bool m_hasSeedStatus;
        bool m_fastresumeDataRejected = false;
        bool m_hasMissingFiles = false;
        bool m_hasFirstLastPiecePriority = false;
        bool m_useAutoTMM;
        bool m_isStopped;

        bool m_unchecked = false;

        lt::add_torrent_params m_ltAddTorrentParams;
    };
}
