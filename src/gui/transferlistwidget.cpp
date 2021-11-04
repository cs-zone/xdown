/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "transferlistwidget.h"

#include <algorithm>

#include <QClipboard>
#include <QDebug>
#include <QFileDialog>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QRegExp>
#include <QRegularExpression>
#include <QSet>
#include <QShortcut>
#include <QTableView>
#include <QVector>
#include <QWheelEvent>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/bittorrent/trackerentry.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/torrentfilter.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "autoexpandabledialog.h"
#include "deletionconfirmationdialog.h"
#include "mainwindow.h"
#include "optionsdialog.h"
#include "previewselectdialog.h"
#include "speedlimitdialog.h"
#include "torrentcategorydialog.h"
#include "trackerentriesdialog.h"
#include "transferlistdelegate.h"
#include "transferlistmodel.h"
#include "transferlistsortmodel.h"
#include "tristateaction.h"
#include "uithememanager.h"
#include "updownratiodialog.h"
#include "utils.h"

#ifdef Q_OS_MACOS
#include "macutilities.h"
#endif

namespace
{
    QVector<BitTorrent::InfoHash> extractHashes(const QVector<BitTorrent::TorrentHandle *> &torrents)
    {
        QVector<BitTorrent::InfoHash> hashes;
        hashes.reserve(torrents.size());
        for (const BitTorrent::TorrentHandle *torrent : torrents)
            hashes << torrent->hash();
        return hashes;
    }

    bool torrentContainsPreviewableFiles(const BitTorrent::TorrentHandle *const torrent)
    {
        if (!torrent->hasMetadata())
            return false;

        for (int i = 0; i < torrent->filesCount(); ++i)
        {
            if (Utils::Misc::isPreviewable(Utils::Fs::fileExtension(torrent->fileName(i))))
                return true;
        }

        return false;
    }

    void openDestinationFolder(const BitTorrent::TorrentHandle *const torrent)
    {
#ifdef Q_OS_MACOS
        bool isHttpTask = false;
        if (torrent->getHandleType() == BitTorrent::TaskHandleType::XDown_Handle) {
            isHttpTask = true;
        }
        MacUtils::openFiles({torrent->contentPath(true)}, isHttpTask);
#else

        // http task
        QString path = torrent->contentPath(true);
        if (torrent->getHandleType() == BitTorrent::TaskHandleType::XDown_Handle) {
            QString fileName = torrent->fileName(0);
            Utils::Gui::openFolderSelectFile(path, fileName);
            return;
        }

        if (torrent->filesCount() == 1)
            Utils::Gui::openFolderSelect(torrent->contentPath(true));
        else
            Utils::Gui::openPath(torrent->contentPath(true));
#endif
    }

    void removeTorrents(const QVector<BitTorrent::TorrentHandle *> &torrents, const bool isDeleteFileSelected)
    {
        auto *session = BitTorrent::Session::instance();
        const DeleteOption deleteOption = isDeleteFileSelected ? TorrentAndFiles : Torrent;

        std::vector<aria2::A2Gid> removeBatchVect;
        for (const BitTorrent::TorrentHandle *torrent : torrents) {
            if (torrent->getHandleType() == BitTorrent::TaskHandleType::XDown_Handle) {
                if (torrent->getGid() > 0) {
                    removeBatchVect.push_back(torrent->getGid());
                }
            }
        }

        int iTotalRemoveSize = removeBatchVect.size();
        int iRemoveIndex = 0;
        while (iRemoveIndex < iTotalRemoveSize) {
            int iCurIndex = 0;
            aria2::A2GidList gidList;
            memset(&gidList, 0, sizeof(gidList));
            for (int idx = iRemoveIndex; idx < iTotalRemoveSize && iCurIndex < (MAX_ACTIVE_GID_SIZE - 1); idx++) {
                aria2::A2Gid tmpGid = removeBatchVect.at(idx);
                gidList.gidList[iCurIndex++] = tmpGid;
                iRemoveIndex++;
            }
            gidList.gidSize = iCurIndex;
            if (iCurIndex > 0) {
                session->OnBatchRemoveDownload(gidList);
            }
        }


        for (const BitTorrent::TorrentHandle *torrent : torrents) {
            if (torrent->getHandleType() != BitTorrent::TaskHandleType::XDown_Handle) {
                session->deleteTorrent(torrent->hash(), deleteOption);
            }
            else {
                session->deleteTorrent(torrent->url(), deleteOption, false);
            }
        }
    }
}

TransferListWidget::TransferListWidget(QWidget *parent, MainWindow *mainWindow)
    : QTreeView(parent)
    , m_mainWindow(mainWindow)
{

    setUniformRowHeights(true);
    // Load settings
    bool columnLoaded = loadSettings();

    // Create and apply delegate
    m_listDelegate = new TransferListDelegate(this);
    setItemDelegate(m_listDelegate);

    // Create transfer list model
    m_listModel = new TransferListModel(this);

    m_sortFilterModel = new TransferListSortModel(this);
    m_sortFilterModel->setDynamicSortFilter(true);
    m_sortFilterModel->setSourceModel(m_listModel);
    m_sortFilterModel->setFilterKeyColumn(TransferListModel::TR_NAME);
    m_sortFilterModel->setFilterRole(Qt::DisplayRole);
    m_sortFilterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_sortFilterModel->setSortRole(TransferListModel::UnderlyingDataRole);

    setModel(m_sortFilterModel);

    // Visual settings
    setRootIsDecorated(false);
    setAllColumnsShowFocus(true);
    setSortingEnabled(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setItemsExpandable(false);
    setAutoScroll(true);
    setDragDropMode(QAbstractItemView::DragOnly);
#if defined(Q_OS_MACOS)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
    header()->setStretchLastSection(false);

    // Default hidden columns
    if (!columnLoaded)
    {
        setColumnHidden(TransferListModel::TR_ADD_DATE, true);
        setColumnHidden(TransferListModel::TR_SEED_DATE, true);
        setColumnHidden(TransferListModel::TR_UPLIMIT, true);
        setColumnHidden(TransferListModel::TR_DLLIMIT, true);
        setColumnHidden(TransferListModel::TR_TRACKER, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_DOWNLOADED, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_UPLOADED, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_DOWNLOADED_SESSION, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_UPLOADED_SESSION, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_LEFT, true);
        setColumnHidden(TransferListModel::TR_TIME_ELAPSED, true);
        setColumnHidden(TransferListModel::TR_SAVE_PATH, true);
        setColumnHidden(TransferListModel::TR_COMPLETED, true);
        setColumnHidden(TransferListModel::TR_RATIO_LIMIT, true);
        setColumnHidden(TransferListModel::TR_SEEN_COMPLETE_DATE, true);
        setColumnHidden(TransferListModel::TR_LAST_ACTIVITY, true);
        setColumnHidden(TransferListModel::TR_TOTAL_SIZE, true);
    }

    //Ensure that at least one column is visible at all times
    bool atLeastOne = false;
    for (int i = 0; i < TransferListModel::NB_COLUMNS; ++i)
    {
        if (!isColumnHidden(i))
        {
            atLeastOne = true;
            break;
        }
    }
    if (!atLeastOne)
        setColumnHidden(TransferListModel::TR_NAME, false);

    //When adding/removing columns between versions some may
    //end up being size 0 when the new version is launched with
    //a conf file from the previous version.
    for (int i = 0; i < TransferListModel::NB_COLUMNS; ++i)
        if ((columnWidth(i) <= 0) && (!isColumnHidden(i)))
            resizeColumnToContents(i);

    setContextMenuPolicy(Qt::CustomContextMenu);

    // Listen for list events
    connect(this, &QAbstractItemView::doubleClicked, this, &TransferListWidget::torrentDoubleClicked);

    // 右键菜单
    connect(this, &QWidget::customContextMenuRequested, this, &TransferListWidget::displayListMenu);
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &TransferListWidget::displayDLHoSMenu);
    connect(header(), &QHeaderView::sectionMoved, this, &TransferListWidget::saveSettings);
    connect(header(), &QHeaderView::sectionResized, this, &TransferListWidget::saveSettings);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &TransferListWidget::saveSettings);

    const auto *editHotkey = new QShortcut(Qt::Key_F2, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(editHotkey, &QShortcut::activated, this, &TransferListWidget::renameSelectedTorrent);
    const auto *deleteHotkey = new QShortcut(QKeySequence::Delete, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(deleteHotkey, &QShortcut::activated, this, &TransferListWidget::softDeleteSelectedTorrents);
    const auto *permDeleteHotkey = new QShortcut(Qt::SHIFT + Qt::Key_Delete, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(permDeleteHotkey, &QShortcut::activated, this, &TransferListWidget::permDeleteSelectedTorrents);
    const auto *doubleClickHotkeyReturn = new QShortcut(Qt::Key_Return, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(doubleClickHotkeyReturn, &QShortcut::activated, this, &TransferListWidget::torrentDoubleClicked);
    const auto *doubleClickHotkeyEnter = new QShortcut(Qt::Key_Enter, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(doubleClickHotkeyEnter, &QShortcut::activated, this, &TransferListWidget::torrentDoubleClicked);
    const auto *recheckHotkey = new QShortcut(Qt::CTRL + Qt::Key_R, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(recheckHotkey, &QShortcut::activated, this, &TransferListWidget::recheckSelectedTorrents);

    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(header());
    header()->setParent(this);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));
}

TransferListWidget::~TransferListWidget()
{
    // Save settings
    saveSettings();
}

TransferListModel *TransferListWidget::getSourceModel() const
{
    return m_listModel;
}

void TransferListWidget::previewFile(const QString &filePath)
{
    Utils::Gui::openPath(filePath);
}

QModelIndex TransferListWidget::mapToSource(const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());
    if (index.model() == m_sortFilterModel)
        return m_sortFilterModel->mapToSource(index);
    return index;
}

QModelIndex TransferListWidget::mapFromSource(const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());
    Q_ASSERT(index.model() == m_sortFilterModel);
    return m_sortFilterModel->mapFromSource(index);
}

// 下载列表，鼠标双击
// TODO 
void TransferListWidget::torrentDoubleClicked()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if ((selectedIndexes.size() != 1) || !selectedIndexes.first().isValid()) return;

    const QModelIndex index = m_listModel->index(mapToSource(selectedIndexes.first()).row());
    BitTorrent::TorrentHandle *const torrent = m_listModel->torrentHandle(index);
    if (!torrent) return;

    bool bIsComplete = false;
    if (torrent->getHandleType() == BitTorrent::TaskHandleType::XDown_Handle) {
        bIsComplete = torrent->isCompleted();
    }
    int action;
    if (torrent->isSeed() || bIsComplete ) {
        action = Preferences::instance()->getActionOnDblClOnTorrentFn();
    }
    else {
        action = Preferences::instance()->getActionOnDblClOnTorrentDl();
    }

    switch (action)
    {
    case TOGGLE_PAUSE:
        if (torrent->isPaused())
            torrent->resume();
        else
            torrent->pause();
        break;
    case PREVIEW_FILE:
        if (torrentContainsPreviewableFiles(torrent))
        {
            auto *dialog = new PreviewSelectDialog(this, torrent);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            connect(dialog, &PreviewSelectDialog::readyToPreviewFile, this, &TransferListWidget::previewFile);
            dialog->show();
        }
        else
        {
            openDestinationFolder(torrent);
        }
        break;
    case OPEN_DEST:
        openDestinationFolder(torrent);
        break;
    }
}

QVector<BitTorrent::TorrentHandle *> TransferListWidget::getSelectedTorrents(BitTorrent::SelectTaskHandleType iValueType) const
{
    const QModelIndexList selectedRows = selectionModel()->selectedRows();

    QVector<BitTorrent::TorrentHandle *> torrents;
    torrents.reserve(selectedRows.size());
    for (const QModelIndex &index : selectedRows) {
        BitTorrent::TorrentHandle *pHandle = m_listModel->torrentHandle(mapToSource(index));
        if (BitTorrent::SelectTaskHandleType::Select_All_Handle == iValueType) {
            torrents << pHandle;
        }
        else if (BitTorrent::SelectTaskHandleType::Select_BitTorrent_Handle == iValueType
            && pHandle->getHandleType() == BitTorrent::TaskHandleType::BitTorrent_Handle) {
            // bt下载任务
            torrents << pHandle;
        }
        else if (BitTorrent::SelectTaskHandleType::Select_XDown_Handle == iValueType && pHandle->getHandleType() == BitTorrent::TaskHandleType::XDown_Handle) {
            torrents << pHandle;
        }
    }
    return torrents;
}

QVector<BitTorrent::TorrentHandle *> TransferListWidget::getVisibleTorrents() const
{
    const int visibleTorrentsCount = m_sortFilterModel->rowCount();

    QVector<BitTorrent::TorrentHandle *> torrents;
    torrents.reserve(visibleTorrentsCount);
    for (int i = 0; i < visibleTorrentsCount; ++i)
        torrents << m_listModel->torrentHandle(mapToSource(m_sortFilterModel->index(i, 0)));
    return torrents;
}

void TransferListWidget::setSelectedTorrentsLocation()
{
    const QVector<BitTorrent::TorrentHandle *> torrents = getSelectedTorrents();
    if (torrents.isEmpty()) return;

    const QString oldLocation = torrents[0]->savePath();
    const QString newLocation = QFileDialog::getExistingDirectory(this, tr("Choose save path"), oldLocation,
                                            QFileDialog::DontConfirmOverwrite | QFileDialog::ShowDirsOnly | QFileDialog::HideNameFilterDetails);
    if (newLocation.isEmpty() || !QDir(newLocation).exists()) return;

    // Actually move storage
    for (BitTorrent::TorrentHandle *const torrent : torrents)
        torrent->move(Utils::Fs::expandPathAbs(newLocation));
}

void TransferListWidget::pauseAllTorrents()
{
    ///////////////////////////////////////////////////////////////
    BitTorrent::Session::instance()->pauseAllTorrents();
    ///////////////////////////////////////////////////////////////
#if 0
    for (BitTorrent::TorrentHandle *const torrent : asConst(BitTorrent::Session::instance()->torrents()))
    {
        torrent->pause();
    }

    for (BitTorrent::TorrentHandle *const torrent : asConst(BitTorrent::Session::instance()->xdowns()))
    {
        torrent->pause();
    }
#endif
}

void TransferListWidget::resumeAllTorrents()
{
    ///////////////////////////////////////////////////////////////
    BitTorrent::Session::instance()->resumeAllTorrents();
    ///////////////////////////////////////////////////////////////
#if 0
    for (BitTorrent::TorrentHandle *const torrent : asConst(BitTorrent::Session::instance()->torrents()))
    {
        torrent->resume();
    }
    for (BitTorrent::TorrentHandle *const torrent : asConst(BitTorrent::Session::instance()->xdowns()))
    {
        torrent->resume();
    }
#endif
}


#include <gui/xupdatelinkdialog.h>
void TransferListWidget::updateSelectedDownloadLink()
{
    BitTorrent::TorrentHandle *pXDown = nullptr;
    for (BitTorrent::TorrentHandle *torrent : getSelectedTorrents(BitTorrent::SelectTaskHandleType::Select_XDown_Handle)) {
        pXDown = torrent;
        break;
    }
    if (!pXDown) {
        return;
    }
    pXDown->pause();
    QString strValue = pXDown->url();
    emit BitTorrent::Session::instance()->startOnShowUpdateLinkDlg(strValue);
}

void TransferListWidget::startSelectedTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->resume();
}

void TransferListWidget::forceStartSelectedTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->resume(BitTorrent::TorrentOperatingMode::Forced);
}

void TransferListWidget::startVisibleTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getVisibleTorrents()))
        torrent->resume();
}

void TransferListWidget::pauseSelectedTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->pause();
}

void TransferListWidget::pauseVisibleTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getVisibleTorrents()))
        torrent->pause();
}

void TransferListWidget::softDeleteSelectedTorrents()
{
    deleteSelectedTorrents(false);
}

void TransferListWidget::permDeleteSelectedTorrents()
{
    deleteSelectedTorrents(true);
}

void TransferListWidget::deleteSelectedTorrents(const bool deleteLocalFiles)
{
    if (m_mainWindow->currentTabWidget() != this) return;

    const QVector<BitTorrent::TorrentHandle *> torrents = getSelectedTorrents();
    if (torrents.empty()) return;

    if (Preferences::instance()->confirmTorrentDeletion())
    {
        auto *dialog = new DeletionConfirmationDialog(this, torrents.size(), torrents[0]->name(), deleteLocalFiles);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &DeletionConfirmationDialog::accepted, this, [this, dialog]()
        {
            // Some torrents might be removed when waiting for user input, so refetch the torrent list
            // NOTE: this will only work when dialog is modal
            removeTorrents(getSelectedTorrents(), dialog->isDeleteFileSelected());
        });
        dialog->open();
    }
    else
    {
        removeTorrents(torrents, deleteLocalFiles);
    }
}

void TransferListWidget::deleteVisibleTorrents()
{
    const QVector<BitTorrent::TorrentHandle *> torrents = getVisibleTorrents();
    if (torrents.empty()) return;

    if (Preferences::instance()->confirmTorrentDeletion())
    {
        auto *dialog = new DeletionConfirmationDialog(this, torrents.size(), torrents[0]->name(), false);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &DeletionConfirmationDialog::accepted, this, [this, dialog]()
        {
            // Some torrents might be removed when waiting for user input, so refetch the torrent list
            // NOTE: this will only work when dialog is modal
            removeTorrents(getVisibleTorrents(), dialog->isDeleteFileSelected());
        });
        dialog->open();
    }
    else
    {
        removeTorrents(torrents, false);
    }
}

void TransferListWidget::increaseQueuePosSelectedTorrents()
{
    qDebug() << Q_FUNC_INFO;
    if (m_mainWindow->currentTabWidget() == this)
        BitTorrent::Session::instance()->increaseTorrentsQueuePos(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::decreaseQueuePosSelectedTorrents()
{
    qDebug() << Q_FUNC_INFO;
    if (m_mainWindow->currentTabWidget() == this)
        BitTorrent::Session::instance()->decreaseTorrentsQueuePos(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::topQueuePosSelectedTorrents()
{
    if (m_mainWindow->currentTabWidget() == this)
        BitTorrent::Session::instance()->topTorrentsQueuePos(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::bottomQueuePosSelectedTorrents()
{
    if (m_mainWindow->currentTabWidget() == this)
        BitTorrent::Session::instance()->bottomTorrentsQueuePos(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::copySelectedMagnetURIs() const
{
    QStringList magnetUris;
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        magnetUris << torrent->createMagnetURI();

    qApp->clipboard()->setText(magnetUris.join('\n'));
}

void TransferListWidget::copySelectedXDownURIs() const
{
    QStringList xdownUris;
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        xdownUris << torrent->source();

    qApp->clipboard()->setText(xdownUris.join('\n'));
}



void TransferListWidget::copySelectedNames() const
{
    QStringList torrentNames;
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrentNames << torrent->name();

    qApp->clipboard()->setText(torrentNames.join('\n'));
}

void TransferListWidget::copySelectedHashes() const
{
    QStringList torrentHashes;
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrentHashes << torrent->hash();

    qApp->clipboard()->setText(torrentHashes.join('\n'));
}

void TransferListWidget::hideQueuePosColumn(bool hide)
{
    setColumnHidden(TransferListModel::TR_QUEUE_POSITION, hide);
    if (!hide && (columnWidth(TransferListModel::TR_QUEUE_POSITION) == 0))
        resizeColumnToContents(TransferListModel::TR_QUEUE_POSITION);
}

void TransferListWidget::openSelectedTorrentsFile() const 
{
#ifdef Q_OS_MACOS
#else
    QSet<QString> pathsList;
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents())) {
        QString path = torrent->contentPath(true);
        // TODO 考虑文件可能会被合并 YouTube下载
        if (torrent->getHandleType() == BitTorrent::TaskHandleType::XDown_Handle) {
            QString fileName = torrent->fileName(0);
            Utils::Gui::openSelectFile(path, fileName);
            continue;
        }
        QString fileName = torrent->fileName(0);
        Utils::Gui::openSelectFile(path, fileName);
    }
#endif
}

void TransferListWidget::openSelectedTorrentsFolder() const
{
    QSet<QString> pathsList;
#ifdef Q_OS_MACOS
    // On macOS you expect both the files and folders to be opened in their parent
    // folders prehilighted for opening, so we use a custom method.
    bool isHttpTask = false;
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
    {
        QString path = torrent->contentPath(true);
        pathsList.insert(path);
        if (torrent->getHandleType() == BitTorrent::TaskHandleType::XDown_Handle) {
            isHttpTask = true;
        }
        break;
    }
    MacUtils::openFiles(pathsList,isHttpTask);
#else
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
    {
        QString path = torrent->contentPath(true);
        if (torrent->getHandleType() == BitTorrent::TaskHandleType::XDown_Handle) {
            QString fileName = torrent->fileName(0);
            Utils::Gui::openFolderSelectFile(path, fileName);
            continue;
        }
        if (!pathsList.contains(path)) {
            if (torrent->filesCount() == 1)
                Utils::Gui::openFolderSelect(path);
            else
                Utils::Gui::openPath(path);
        }
        pathsList.insert(path);
    }
#endif // Q_OS_MACOS
}

void TransferListWidget::previewSelectedTorrents()
{
    for (const BitTorrent::TorrentHandle *torrent : asConst(getSelectedTorrents()))
    {
        if (torrentContainsPreviewableFiles(torrent))
        {
            auto *dialog = new PreviewSelectDialog(this, torrent);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            connect(dialog, &PreviewSelectDialog::readyToPreviewFile, this, &TransferListWidget::previewFile);
            dialog->show();
        }
        else
        {
            QMessageBox::critical(this, tr("Unable to preview"), tr("The selected torrent \"%1\" does not contain previewable files")
                .arg(torrent->name()));
        }
    }
}

void TransferListWidget::setDlLimitSelectedTorrents()
{
    QVector<BitTorrent::TorrentHandle *> torrentsList;
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
    {
        if (torrent->isSeed())
            continue;
        torrentsList += torrent;
    }
    if (torrentsList.empty()) return;

    int oldLimit = torrentsList.first()->downloadLimit();
    for (BitTorrent::TorrentHandle *const torrent : asConst(torrentsList))
    {
        if (torrent->downloadLimit() != oldLimit)
        {
            oldLimit = -1;
            break;
        }
    }

    bool ok = false;
    const long newLimit = SpeedLimitDialog::askSpeedLimit(
                this, &ok, tr("Torrent Download Speed Limiting"), oldLimit
                , BitTorrent::Session::instance()->globalDownloadSpeedLimit());
    if (!ok) return;

    for (BitTorrent::TorrentHandle *const torrent : asConst(torrentsList))
    {
        qDebug("Applying download speed limit of %ld Kb/s to torrent %s", (newLimit / 1024l), qUtf8Printable(torrent->hash()));
        torrent->setDownloadLimit(newLimit);
    }
}

void TransferListWidget::setUpLimitSelectedTorrents()
{
    QVector<BitTorrent::TorrentHandle *> torrentsList = getSelectedTorrents();
    if (torrentsList.empty()) return;

    int oldLimit = torrentsList.first()->uploadLimit();
    for (BitTorrent::TorrentHandle *const torrent : asConst(torrentsList))
    {
        if (torrent->uploadLimit() != oldLimit)
        {
            oldLimit = -1;
            break;
        }
    }

    bool ok = false;
    const long newLimit = SpeedLimitDialog::askSpeedLimit(
                this, &ok, tr("Torrent Upload Speed Limiting"), oldLimit
                , BitTorrent::Session::instance()->globalUploadSpeedLimit());
    if (!ok) return;

    for (BitTorrent::TorrentHandle *const torrent : asConst(torrentsList))
    {
        qDebug("Applying upload speed limit of %ld Kb/s to torrent %s", (newLimit / 1024l), qUtf8Printable(torrent->hash()));
        torrent->setUploadLimit(newLimit);
    }
}

void TransferListWidget::setMaxRatioSelectedTorrents()
{
    const QVector<BitTorrent::TorrentHandle *> torrents = getSelectedTorrents();
    if (torrents.isEmpty()) return;

    qreal currentMaxRatio = BitTorrent::Session::instance()->globalMaxRatio();
    if (torrents.count() == 1)
        currentMaxRatio = torrents[0]->maxRatio();

    int currentMaxSeedingTime = BitTorrent::Session::instance()->globalMaxSeedingMinutes();
    if (torrents.count() == 1)
        currentMaxSeedingTime = torrents[0]->maxSeedingTime();

    bool useGlobalValue = true;
    if (torrents.count() == 1)
        useGlobalValue = (torrents[0]->ratioLimit() == BitTorrent::TorrentHandle::USE_GLOBAL_RATIO)
                && (torrents[0]->seedingTimeLimit() == BitTorrent::TorrentHandle::USE_GLOBAL_SEEDING_TIME);

    auto dialog = new UpDownRatioDialog(useGlobalValue, currentMaxRatio, BitTorrent::TorrentHandle::MAX_RATIO,
                       currentMaxSeedingTime, BitTorrent::TorrentHandle::MAX_SEEDING_TIME, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [dialog, torrents]()
    {
        for (BitTorrent::TorrentHandle *const torrent : torrents)
        {
            const qreal ratio = (dialog->useDefault()
                ? BitTorrent::TorrentHandle::USE_GLOBAL_RATIO : dialog->ratio());
            torrent->setRatioLimit(ratio);

            const int seedingTime = (dialog->useDefault()
                ? BitTorrent::TorrentHandle::USE_GLOBAL_SEEDING_TIME : dialog->seedingTime());
            torrent->setSeedingTimeLimit(seedingTime);
        }
    });
    dialog->open();
}

void TransferListWidget::recheckSelectedTorrents()
{
    if (Preferences::instance()->confirmTorrentRecheck())
    {
        QMessageBox::StandardButton ret = QMessageBox::question(this, tr("Recheck confirmation"), tr("Are you sure you want to recheck the selected torrent(s)?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (ret != QMessageBox::Yes) return;
    }

    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->forceRecheck();
}

void TransferListWidget::reannounceSelectedTorrents()
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->forceReannounce();
}

// hide/show columns menu
void TransferListWidget::displayDLHoSMenu(const QPoint&)
{
    auto menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setTitle(tr("Column visibility"));

    for (int i = 0; i < m_listModel->columnCount(); ++i)
    {
        if (!BitTorrent::Session::instance()->isQueueingSystemEnabled() && (i == TransferListModel::TR_QUEUE_POSITION))
            continue;

        QAction *myAct = menu->addAction(m_listModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
        myAct->setCheckable(true);
        myAct->setChecked(!isColumnHidden(i));
        myAct->setData(i);
    }

    connect(menu, &QMenu::triggered, this, [this](const QAction *action)
    {
        int visibleCols = 0;
        for (int i = 0; i < TransferListModel::NB_COLUMNS; ++i)
        {
            if (!isColumnHidden(i))
                ++visibleCols;

            if (visibleCols > 1)
                break;
        }

        const int col = action->data().toInt();

        if (!isColumnHidden(col) && visibleCols == 1)
            return;

        setColumnHidden(col, !isColumnHidden(col));

        if (!isColumnHidden(col) && columnWidth(col) <= 5)
            resizeColumnToContents(col);

        saveSettings();
    });

    menu->popup(QCursor::pos());
}

void TransferListWidget::setSelectedTorrentsSuperSeeding(const bool enabled) const
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
    {
        if (torrent->hasMetadata())
            torrent->setSuperSeeding(enabled);
    }
}

void TransferListWidget::setSelectedTorrentsSequentialDownload(const bool enabled) const
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->setSequentialDownload(enabled);
}

void TransferListWidget::setSelectedFirstLastPiecePrio(const bool enabled) const
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->setFirstLastPiecePriority(enabled);
}

void TransferListWidget::setSelectedAutoTMMEnabled(const bool enabled) const
{
    for (BitTorrent::TorrentHandle *const torrent : asConst(getSelectedTorrents()))
        torrent->setAutoTMMEnabled(enabled);
}

#ifdef __ENABLE_CATEGORY__
void TransferListWidget::askNewCategoryForSelection()
{
    const QString newCategoryName = TorrentCategoryDialog::createCategory(this);
    if (!newCategoryName.isEmpty())
        setSelectionCategory(newCategoryName);
}

void TransferListWidget::askAddTagsForSelection()
{
    const QStringList tags = askTagsForSelection(tr("Add Tags"));
    for (const QString &tag : tags)
        addSelectionTag(tag);
}
#endif

void TransferListWidget::editTorrentTrackers()
{
    const QVector<BitTorrent::TorrentHandle *> torrents = getSelectedTorrents();
    QVector<BitTorrent::TrackerEntry> commonTrackers;

    if (!torrents.empty())
    {
        commonTrackers = torrents[0]->trackers();

        for (const BitTorrent::TorrentHandle *torrent : torrents)
        {
            QSet<BitTorrent::TrackerEntry> trackerSet;

            for (const BitTorrent::TrackerEntry &entry : asConst(torrent->trackers()))
                trackerSet.insert(entry);

            commonTrackers.erase(std::remove_if(commonTrackers.begin(), commonTrackers.end()
                , [&trackerSet](const BitTorrent::TrackerEntry &entry) { return !trackerSet.contains(entry); })
                , commonTrackers.end());
        }
    }

    auto trackerDialog = new TrackerEntriesDialog(this);
    trackerDialog->setAttribute(Qt::WA_DeleteOnClose);
    trackerDialog->setTrackers(commonTrackers);

    connect(trackerDialog, &QDialog::accepted, this, [torrents, trackerDialog]()
    {
        for (BitTorrent::TorrentHandle *torrent : torrents)
            torrent->replaceTrackers(trackerDialog->trackers());
    });

    trackerDialog->open();
}

#ifdef __ENABLE_CATEGORY__

void TransferListWidget::confirmRemoveAllTagsForSelection()
{
    QMessageBox::StandardButton response = QMessageBox::question(
        this, tr("Remove All Tags"), tr("Remove all tags from selected torrents?"),
        QMessageBox::Yes | QMessageBox::No);
    if (response == QMessageBox::Yes)
        clearSelectionTags();
}

QStringList TransferListWidget::askTagsForSelection(const QString &dialogTitle)
{
    QStringList tags;
    bool invalid = true;
    while (invalid)
    {
        bool ok = false;
        invalid = false;
        const QString tagsInput = AutoExpandableDialog::getText(
            this, dialogTitle, tr("Comma-separated tags:"), QLineEdit::Normal, "", &ok).trimmed();
        if (!ok || tagsInput.isEmpty())
            return {};
        tags = tagsInput.split(',', QString::SkipEmptyParts);
        for (QString &tag : tags)
        {
            tag = tag.trimmed();
            if (!BitTorrent::Session::isValidTag(tag))
            {
                QMessageBox::warning(this, tr("Invalid tag")
                                     , tr("Tag name: '%1' is invalid").arg(tag));
                invalid = true;
            }
        }
    }
    return tags;
}
#endif

void TransferListWidget::applyToSelectedTorrents(const std::function<void (BitTorrent::TorrentHandle *const)> &fn)
{
    for (const QModelIndex &index : asConst(selectionModel()->selectedRows()))
    {
        BitTorrent::TorrentHandle *const torrent = m_listModel->torrentHandle(mapToSource(index));
        Q_ASSERT(torrent);
        fn(torrent);
    }
}

void TransferListWidget::renameSelectedTorrent()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if ((selectedIndexes.size() != 1) || !selectedIndexes.first().isValid()) return;

    const QModelIndex mi = m_listModel->index(mapToSource(selectedIndexes.first()).row(), TransferListModel::TR_NAME);
    BitTorrent::TorrentHandle *const torrent = m_listModel->torrentHandle(mi);
    if (!torrent) return;

    // Ask for a new Name
    bool ok = false;
    QString name = AutoExpandableDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, torrent->name(), &ok);
    if (ok && !name.isEmpty())
    {
        name.replace(QRegularExpression("\r?\n|\r"), " ");
        // Rename the torrent
        m_listModel->setData(mi, name, Qt::DisplayRole);
    }
}

#ifdef __ENABLE_CATEGORY__
void TransferListWidget::setSelectionCategory(const QString &category)
{
    for (const QModelIndex &index : asConst(selectionModel()->selectedRows()))
        m_listModel->setData(m_listModel->index(mapToSource(index).row(), TransferListModel::TR_CATEGORY), category, Qt::DisplayRole);
}

void TransferListWidget::addSelectionTag(const QString &tag)
{
    applyToSelectedTorrents([&tag](BitTorrent::TorrentHandle *const torrent) { torrent->addTag(tag); });
}

void TransferListWidget::removeSelectionTag(const QString &tag)
{
    applyToSelectedTorrents([&tag](BitTorrent::TorrentHandle *const torrent) { torrent->removeTag(tag); });
}

void TransferListWidget::clearSelectionTags()
{
    applyToSelectedTorrents([](BitTorrent::TorrentHandle *const torrent) { torrent->removeAllTags(); });
}
#endif

void TransferListWidget::displayListMenu(const QPoint &)
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty()) return;

    const QVector<BitTorrent::TorrentHandle *> torrents = getSelectedTorrents(BitTorrent::SelectTaskHandleType::Select_All_Handle);
    if (torrents.isEmpty()) {
        return;
    }

    BitTorrent::TaskHandleType xHandleType = torrents[0]->getHandleType();


    auto *listMenu = new QMenu(this);
    listMenu->setAttribute(Qt::WA_DeleteOnClose);

    auto *actionStart = new QAction(UIThemeManager::instance()->getIcon("media-playback-start"), tr("Resume", "Resume/start the torrent"), listMenu);
    connect(actionStart, &QAction::triggered, this, &TransferListWidget::startSelectedTorrents);
    auto *actionPause = new QAction(UIThemeManager::instance()->getIcon("media-playback-pause"), tr("Pause", "Pause the torrent"), listMenu);
    connect(actionPause, &QAction::triggered, this, &TransferListWidget::pauseSelectedTorrents);
    auto *actionForceStart = new QAction(UIThemeManager::instance()->getIcon("media-seek-forward"), tr("Force Resume", "Force Resume/start the torrent"), listMenu);
    connect(actionForceStart, &QAction::triggered, this, &TransferListWidget::forceStartSelectedTorrents);
    auto *actionDelete = new QAction(UIThemeManager::instance()->getIcon("list-remove"), tr("Delete", "Delete the torrent"), listMenu);
    connect(actionDelete, &QAction::triggered, this, &TransferListWidget::softDeleteSelectedTorrents);
    auto *actionPreviewFile = new QAction(UIThemeManager::instance()->getIcon("view-preview"), tr("Preview file..."), listMenu);
    connect(actionPreviewFile, &QAction::triggered, this, &TransferListWidget::previewSelectedTorrents);
    auto *actionSetMaxRatio = new QAction(UIThemeManager::instance()->getIcon(QLatin1String("ratio")), tr("Limit share ratio..."), listMenu);
    connect(actionSetMaxRatio, &QAction::triggered, this, &TransferListWidget::setMaxRatioSelectedTorrents);
    auto *actionSetUploadLimit = new QAction(UIThemeManager::instance()->getIcon("kt-set-max-upload-speed"), tr("Limit upload rate..."), listMenu);
    connect(actionSetUploadLimit, &QAction::triggered, this, &TransferListWidget::setUpLimitSelectedTorrents);
    auto *actionSetDownloadLimit = new QAction(UIThemeManager::instance()->getIcon("kt-set-max-download-speed"), tr("Limit download rate..."), listMenu);
    connect(actionSetDownloadLimit, &QAction::triggered, this, &TransferListWidget::setDlLimitSelectedTorrents);

    auto *actionOpenDestinationFolder = new QAction(UIThemeManager::instance()->getIcon("inode-directory"), tr("Open destination folder"), listMenu);
    connect(actionOpenDestinationFolder, &QAction::triggered, this, &TransferListWidget::openSelectedTorrentsFolder);

    auto *actionIncreaseQueuePos = new QAction(UIThemeManager::instance()->getIcon("go-up"), tr("Move up", "i.e. move up in the queue"), listMenu);
    connect(actionIncreaseQueuePos, &QAction::triggered, this, &TransferListWidget::increaseQueuePosSelectedTorrents);
    auto *actionDecreaseQueuePos = new QAction(UIThemeManager::instance()->getIcon("go-down"), tr("Move down", "i.e. Move down in the queue"), listMenu);
    connect(actionDecreaseQueuePos, &QAction::triggered, this, &TransferListWidget::decreaseQueuePosSelectedTorrents);
    auto *actionTopQueuePos = new QAction(UIThemeManager::instance()->getIcon("go-top"), tr("Move to top", "i.e. Move to top of the queue"), listMenu);
    connect(actionTopQueuePos, &QAction::triggered, this, &TransferListWidget::topQueuePosSelectedTorrents);
    auto *actionBottomQueuePos = new QAction(UIThemeManager::instance()->getIcon("go-bottom"), tr("Move to bottom", "i.e. Move to bottom of the queue"), listMenu);
    connect(actionBottomQueuePos, &QAction::triggered, this, &TransferListWidget::bottomQueuePosSelectedTorrents);
    //更改保存位置...
    auto *actionSetTorrentPath = new QAction(UIThemeManager::instance()->getIcon("inode-directory"), tr("Set location..."), listMenu);
    connect(actionSetTorrentPath, &QAction::triggered, this, &TransferListWidget::setSelectedTorrentsLocation);
    auto *actionForceRecheck = new QAction(UIThemeManager::instance()->getIcon("document-edit-verify"), tr("Force recheck"), listMenu);
    connect(actionForceRecheck, &QAction::triggered, this, &TransferListWidget::recheckSelectedTorrents);
    auto *actionForceReannounce = new QAction(UIThemeManager::instance()->getIcon("document-edit-verify"), tr("Force reannounce"), listMenu);
    connect(actionForceReannounce, &QAction::triggered, this, &TransferListWidget::reannounceSelectedTorrents);
    auto *actionCopyMagnetLink = new QAction(UIThemeManager::instance()->getIcon("kt-magnet"), tr("Magnet link"), listMenu);
    connect(actionCopyMagnetLink, &QAction::triggered, this, &TransferListWidget::copySelectedMagnetURIs);

    auto *actionCopyXDownLink = new QAction(UIThemeManager::instance()->getIcon("edit-copy"), tr("Copy Link"), listMenu);
    connect(actionCopyXDownLink, &QAction::triggered, this, &TransferListWidget::copySelectedXDownURIs);

    auto *actionUpdateLink = new QAction(UIThemeManager::instance()->getIcon("document-edit"), tr("Update download link", "Update download link"), listMenu);
    connect(actionUpdateLink, &QAction::triggered, this, &TransferListWidget::updateSelectedDownloadLink);

    auto *actionCopyName = new QAction(UIThemeManager::instance()->getIcon("edit-copy"), tr("Name"), listMenu);
    connect(actionCopyName, &QAction::triggered, this, &TransferListWidget::copySelectedNames);
    auto *actionCopyHash = new QAction(UIThemeManager::instance()->getIcon("edit-copy"), tr("Hash"), listMenu);
    connect(actionCopyHash, &QAction::triggered, this, &TransferListWidget::copySelectedHashes);
    auto *actionSuperSeedingMode = new TriStateAction(tr("Super seeding mode"), listMenu);
    connect(actionSuperSeedingMode, &QAction::triggered, this, &TransferListWidget::setSelectedTorrentsSuperSeeding);
    // 重命名...
    auto *actionRename = new QAction(UIThemeManager::instance()->getIcon("edit-rename"), tr("Rename..."), listMenu);
    connect(actionRename, &QAction::triggered, this, &TransferListWidget::renameSelectedTorrent);
    auto *actionSequentialDownload = new TriStateAction(tr("Download in sequential order"), listMenu);
    connect(actionSequentialDownload, &QAction::triggered, this, &TransferListWidget::setSelectedTorrentsSequentialDownload);
    auto *actionFirstLastPiecePrio = new TriStateAction(tr("Download first and last pieces first"), listMenu);
    connect(actionFirstLastPiecePrio, &QAction::triggered, this, &TransferListWidget::setSelectedFirstLastPiecePrio);

    auto *actionAutoTMM = new TriStateAction(tr("Automatic Torrent Management"), listMenu);
    actionAutoTMM->setToolTip(tr("Automatic mode means that various torrent properties(eg save path) will be decided by the associated category"));
    connect(actionAutoTMM, &QAction::triggered, this, &TransferListWidget::setSelectedAutoTMMEnabled);

    QAction *actionEditTracker = new QAction(UIThemeManager::instance()->getIcon("edit-rename"), tr("Edit trackers..."), listMenu);
    connect(actionEditTracker, &QAction::triggered, this, &TransferListWidget::editTorrentTrackers);

    bool bIsTorrentTask = true;
    if (xHandleType == BitTorrent::TaskHandleType::XDown_Handle) {
        bIsTorrentTask = false;
    }

    // End of actions

    // Enable/disable pause/start action given the DL state
    bool needsPause = false, needsStart = false, needsForce = false, needsPreview = false;
    bool allSameSuperSeeding = true;
    bool superSeedingMode = false;
    bool allSameSequentialDownloadMode = true, allSamePrioFirstlast = true;
    bool sequentialDownloadMode = false, prioritizeFirstLast = false;
    bool oneHasMetadata = false, oneNotSeed = false;
    bool allSameAutoTMM = true;
    bool firstAutoTMM = false;

    bool first = true;

    for (const QModelIndex &index : selectedIndexes)
    {
        // Get the file name
        // Get handle and pause the torrent
        const BitTorrent::TorrentHandle *torrent = m_listModel->torrentHandle(mapToSource(index));
        if (!torrent) continue;

        if (first)
        {
            firstAutoTMM = torrent->isAutoTMMEnabled();
#ifdef __ENABLE_CATEGORY__
            tagsInAll = torrent->tags();
#endif
        }
        else {
#ifdef __ENABLE_CATEGORY__
            tagsInAll.intersect(torrent->tags());
#endif
        }

        if (firstAutoTMM != torrent->isAutoTMMEnabled())
            allSameAutoTMM = false;

        if (torrent->hasMetadata())
            oneHasMetadata = true;
        if (!torrent->isSeed())
        {
            oneNotSeed = true;
            if (first)
            {
                sequentialDownloadMode = torrent->isSequentialDownload();
                prioritizeFirstLast = torrent->hasFirstLastPiecePriority();
            }
            else
            {
                if (sequentialDownloadMode != torrent->isSequentialDownload())
                    allSameSequentialDownloadMode = false;
                if (prioritizeFirstLast != torrent->hasFirstLastPiecePriority())
                    allSamePrioFirstlast = false;
            }
        }
        else
        {
            if (!oneNotSeed && allSameSuperSeeding && torrent->hasMetadata())
            {
                if (first)
                    superSeedingMode = torrent->superSeeding();
                else if (superSeedingMode != torrent->superSeeding())
                    allSameSuperSeeding = false;
            }
        }

        if (!torrent->isForced())
            needsForce = true;
        else
            needsStart = true;

        if (torrent->isPaused())
            needsStart = true;
        else
            needsPause = true;

        if (torrent->isErrored() || torrent->hasMissingFiles())
        {
            // If torrent is in "errored" or "missing files" state
            // it cannot keep further processing until you restart it.
            needsStart = true;
            needsForce = true;
        }

        if (torrent->hasMetadata())
            needsPreview = true;

        first = false;

        if (oneHasMetadata && oneNotSeed && !allSameSequentialDownloadMode
            && !allSamePrioFirstlast && !allSameSuperSeeding
#ifdef __ENABLE_CATEGORY__
            && !allSameCategory
#endif
            && needsStart && needsForce && needsPause && needsPreview && !allSameAutoTMM) {
            break;
        }
    }

    if (needsStart)
        listMenu->addAction(actionStart);
    if (needsPause)
        listMenu->addAction(actionPause);
    if (needsForce)
        listMenu->addAction(actionForceStart);
    listMenu->addSeparator();
    listMenu->addAction(actionDelete);
    listMenu->addSeparator();
    if (bIsTorrentTask) {
        // 更改保存位置
        listMenu->addAction(actionSetTorrentPath);
        if (selectedIndexes.size() == 1) {
            listMenu->addAction(actionRename);
        }
        listMenu->addAction(actionEditTracker);
    }



    if (bIsTorrentTask) {
        actionAutoTMM->setCheckState(allSameAutoTMM
            ? (firstAutoTMM ? Qt::Checked : Qt::Unchecked)
            : Qt::PartiallyChecked);
        listMenu->addAction(actionAutoTMM);

        // 分割线
        listMenu->addSeparator();
        // 限制下载速度
        if (oneNotSeed) {
            listMenu->addAction(actionSetDownloadLimit);
        }
    
        // 显示上传速度
        listMenu->addAction(actionSetUploadLimit);
        // 显示分享速度
        listMenu->addAction(actionSetMaxRatio);
    }
    if (!oneNotSeed && oneHasMetadata) {
        actionSuperSeedingMode->setCheckState(allSameSuperSeeding
            ? (superSeedingMode ? Qt::Checked : Qt::Unchecked)
            : Qt::PartiallyChecked);
        listMenu->addAction(actionSuperSeedingMode);
    }
    // 分割线
    listMenu->addSeparator();
    // 预览文件
    bool addedPreviewAction = false;
    if (needsPreview) {
        listMenu->addAction(actionPreviewFile);
        addedPreviewAction = true;
    }
    // 是否做种状态
    if (bIsTorrentTask) {
        if (oneNotSeed) {
            // 按顺序下载
            actionSequentialDownload->setCheckState(allSameSequentialDownloadMode
                ? (sequentialDownloadMode ? Qt::Checked : Qt::Unchecked)
                : Qt::PartiallyChecked);
            listMenu->addAction(actionSequentialDownload);

            // 先按首位块下载
            actionFirstLastPiecePrio->setCheckState(allSamePrioFirstlast
                ? (prioritizeFirstLast ? Qt::Checked : Qt::Unchecked)
                : Qt::PartiallyChecked);
            listMenu->addAction(actionFirstLastPiecePrio);

            addedPreviewAction = true;
        }
    }

    if (addedPreviewAction)
        listMenu->addSeparator();
    if (oneHasMetadata)
    {
        listMenu->addAction(actionForceRecheck);
        listMenu->addAction(actionForceReannounce);
        listMenu->addSeparator();
    }

    // 打开目标文件夹
    listMenu->addAction(actionOpenDestinationFolder);
    if (BitTorrent::Session::instance()->isQueueingSystemEnabled() && oneNotSeed)
    {
        listMenu->addSeparator();
        QMenu *queueMenu = listMenu->addMenu(tr("Queue"));
        queueMenu->addAction(actionTopQueuePos);
        queueMenu->addAction(actionIncreaseQueuePos);
        queueMenu->addAction(actionDecreaseQueuePos);
        queueMenu->addAction(actionBottomQueuePos);
    }

    if (bIsTorrentTask) {
        QMenu *copySubMenu = listMenu->addMenu(
            UIThemeManager::instance()->getIcon("edit-copy"), tr("Copy"));
        copySubMenu->addAction(actionCopyName);
        copySubMenu->addAction(actionCopyHash);
        copySubMenu->addAction(actionCopyMagnetLink);
    }
    else {
        listMenu->addSeparator();
        // actionCopyXDownLink
        listMenu->addAction(actionCopyXDownLink);
        BitTorrent::TorrentHandle *pXDownItem = nullptr;
        for (BitTorrent::TorrentHandle *torrent : getSelectedTorrents()) {
            pXDownItem = torrent;
            break;
        }
        if (pXDownItem != nullptr) {
            listMenu->addAction(actionUpdateLink);
            actionUpdateLink->setEnabled(true);
        }
    }
    listMenu->popup(QCursor::pos());
}

void TransferListWidget::currentChanged(const QModelIndex &current, const QModelIndex&)
{
    qDebug("CURRENT CHANGED");
    BitTorrent::TorrentHandle *torrent = nullptr;
    if (current.isValid())
    {
        torrent = m_listModel->torrentHandle(mapToSource(current));
        // Scroll Fix
        scrollTo(current);
    }
    emit currentTorrentChanged(torrent);
}

void TransferListWidget::applyNameFilter(const QString &name)
{
    const QRegExp::PatternSyntax patternSyntax = Preferences::instance()->getRegexAsFilteringPatternForTransferList()
                ? QRegExp::RegExp : QRegExp::WildcardUnix;
    m_sortFilterModel->setFilterRegExp(QRegExp(name, Qt::CaseInsensitive, patternSyntax));
}

void TransferListWidget::applyStatusFilter(int f)
{
    m_sortFilterModel->setStatusFilter(static_cast<TorrentFilter::Type>(f));
    // Select first item if nothing is selected
    if (selectionModel()->selectedRows(0).empty() && (m_sortFilterModel->rowCount() > 0))
    {
        qDebug("Nothing is selected, selecting first row: %s", qUtf8Printable(m_sortFilterModel->index(0, TransferListModel::TR_NAME).data().toString()));
        selectionModel()->setCurrentIndex(m_sortFilterModel->index(0, TransferListModel::TR_NAME), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    }
}

void TransferListWidget::saveSettings()
{
    Preferences::instance()->setTransHeaderState(header()->saveState());
}

bool TransferListWidget::loadSettings()
{
    return header()->restoreState(Preferences::instance()->getTransHeaderState());
}

void TransferListWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ShiftModifier)
    {
        // Shift + scroll = horizontal scroll
        event->accept();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        QWheelEvent scrollHEvent(event->position(), event->globalPosition()
            , event->pixelDelta(), event->angleDelta().transposed(), event->buttons()
            , event->modifiers(), event->phase(), event->inverted(), event->source());
#else
        QWheelEvent scrollHEvent(event->pos(), event->globalPos()
            , event->delta(), event->buttons(), event->modifiers(), Qt::Horizontal);
#endif
        QTreeView::wheelEvent(&scrollHEvent);
        return;
    }

    QTreeView::wheelEvent(event);  // event delegated to base class
}
