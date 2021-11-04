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

#include "peerlistwidget.h"

#include <algorithm>

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QHostAddress>
#include <QMenu>
#include <QMessageBox>
#include <QSet>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QVector>
#include <QWheelEvent>

#include "base/bittorrent/peeraddress.h"
#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/geoipmanager.h"
#include "base/net/reverseresolution.h"
#include "base/preferences.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "gui/uithememanager.h"
#include "peerlistsortmodel.h"
#include "peersadditiondialog.h"
#include "propertieswidget.h"

struct PeerEndpoint
{
    BitTorrent::PeerAddress address;
    QString connectionType; // matches return type of `PeerInfo::connectionType()`
};

bool operator==(const PeerEndpoint &left, const PeerEndpoint &right)
{
    return (left.address == right.address) && (left.connectionType == right.connectionType);
}

uint qHash(const PeerEndpoint &peerEndpoint, const uint seed)
{
    return (qHash(peerEndpoint.address, seed) ^ ::qHash(peerEndpoint.connectionType));
}

PeerListWidget::PeerListWidget(PropertiesWidget *parent)
    : QTreeView(parent)
    , m_properties(parent)
{
    // Load settings
    loadSettings();
    // Visual settings
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setItemsExpandable(false);
    setAllColumnsShowFocus(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    header()->setStretchLastSection(false);
    // List Model
    m_listModel = new QStandardItemModel(0, PeerListColumns::COL_COUNT, this);
    m_listModel->setHeaderData(PeerListColumns::COUNTRY, Qt::Horizontal, tr("Country/Region")); // Country flag column
    m_listModel->setHeaderData(PeerListColumns::IP, Qt::Horizontal, tr("IP"));
    m_listModel->setHeaderData(PeerListColumns::PORT, Qt::Horizontal, tr("Port"));
    m_listModel->setHeaderData(PeerListColumns::FLAGS, Qt::Horizontal, tr("Flags"));
    m_listModel->setHeaderData(PeerListColumns::CONNECTION, Qt::Horizontal, tr("Connection"));
    m_listModel->setHeaderData(PeerListColumns::CLIENT, Qt::Horizontal, tr("Client", "i.e.: Client application"));
    m_listModel->setHeaderData(PeerListColumns::PROGRESS, Qt::Horizontal, tr("Progress", "i.e: % downloaded"));
    m_listModel->setHeaderData(PeerListColumns::DOWN_SPEED, Qt::Horizontal, tr("Down Speed", "i.e: Download speed"));
    m_listModel->setHeaderData(PeerListColumns::UP_SPEED, Qt::Horizontal, tr("Up Speed", "i.e: Upload speed"));
    m_listModel->setHeaderData(PeerListColumns::TOT_DOWN, Qt::Horizontal, tr("Downloaded", "i.e: total data downloaded"));
    m_listModel->setHeaderData(PeerListColumns::TOT_UP, Qt::Horizontal, tr("Uploaded", "i.e: total data uploaded"));
    m_listModel->setHeaderData(PeerListColumns::RELEVANCE, Qt::Horizontal, tr("Relevance", "i.e: How relevant this peer is to us. How many pieces it has that we don't."));
    m_listModel->setHeaderData(PeerListColumns::DOWNLOADING_PIECE, Qt::Horizontal, tr("Files", "i.e. files that are being downloaded right now"));
    // Set header text alignment
    m_listModel->setHeaderData(PeerListColumns::PORT, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::PROGRESS, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::DOWN_SPEED, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::UP_SPEED, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::TOT_DOWN, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::TOT_UP, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::RELEVANCE, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    // Proxy model to support sorting without actually altering the underlying model
    m_proxyModel = new PeerListSortModel(this);
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->setSourceModel(m_listModel);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    setModel(m_proxyModel);
    hideColumn(PeerListColumns::IP_HIDDEN);
    hideColumn(PeerListColumns::COL_COUNT);
    m_resolveCountries = Preferences::instance()->resolvePeerCountries();
    if (!m_resolveCountries)
        hideColumn(PeerListColumns::COUNTRY);
    // Ensure that at least one column is visible at all times
    bool atLeastOne = false;
    for (int i = 0; i < PeerListColumns::IP_HIDDEN; ++i)
    {
        if (!isColumnHidden(i))
        {
            atLeastOne = true;
            break;
        }
    }
    if (!atLeastOne)
        setColumnHidden(PeerListColumns::IP, false);
    // To also mitigate the above issue, we have to resize each column when
    // its size is 0, because explicitly 'showing' the column isn't enough
    // in the above scenario.
    for (int i = 0; i < PeerListColumns::IP_HIDDEN; ++i)
    {
        if ((columnWidth(i) <= 0) && !isColumnHidden(i))
            resizeColumnToContents(i);
    }
    // Context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &PeerListWidget::showPeerListMenu);
    // Enable sorting
    setSortingEnabled(true);
    // IP to Hostname resolver
    updatePeerHostNameResolutionState();
    // SIGNAL/SLOT
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &PeerListWidget::displayToggleColumnsMenu);
    connect(header(), &QHeaderView::sectionClicked, this, &PeerListWidget::handleSortColumnChanged);
    connect(header(), &QHeaderView::sectionMoved, this, &PeerListWidget::saveSettings);
    connect(header(), &QHeaderView::sectionResized, this, &PeerListWidget::saveSettings);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &PeerListWidget::saveSettings);
    handleSortColumnChanged(header()->sortIndicatorSection());
    const auto *copyHotkey = new QShortcut(QKeySequence::Copy, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(copyHotkey, &QShortcut::activated, this, &PeerListWidget::copySelectedPeers);

    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(this->header());
    this->header()->setParent(this);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));
}

PeerListWidget::~PeerListWidget()
{
    saveSettings();
}

void PeerListWidget::displayToggleColumnsMenu(const QPoint &)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setTitle(tr("Column visibility"));

    for (int i = 0; i < PeerListColumns::IP_HIDDEN; ++i)
    {
        if ((i == PeerListColumns::COUNTRY) && !Preferences::instance()->resolvePeerCountries())
            continue;

        QAction *myAct = menu->addAction(m_listModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
        myAct->setCheckable(true);
        myAct->setChecked(!isColumnHidden(i));
        myAct->setData(i);
    }

    connect(menu, &QMenu::triggered, this, [this](const QAction *action)
    {
        int visibleCols = 0;
        for (int i = 0; i < PeerListColumns::IP_HIDDEN; ++i)
        {
            if (!isColumnHidden(i))
                ++visibleCols;

            if (visibleCols > 1)
                break;
        }

        const int col = action->data().toInt();

        if (!isColumnHidden(col) && (visibleCols == 1))
            return;

        setColumnHidden(col, !isColumnHidden(col));

        if (!isColumnHidden(col) && (columnWidth(col) <= 5))
            resizeColumnToContents(col);

        saveSettings();
    });

    menu->popup(QCursor::pos());
}

void PeerListWidget::updatePeerHostNameResolutionState()
{
    if (Preferences::instance()->resolvePeerHostNames())
    {
        if (!m_resolver)
        {
            m_resolver = new Net::ReverseResolution(this);
            connect(m_resolver, &Net::ReverseResolution::ipResolved, this, &PeerListWidget::handleResolved);
            loadPeers(m_properties->getCurrentTorrent());
        }
    }
    else
    {
        delete m_resolver;
        m_resolver = nullptr;
    }
}

void PeerListWidget::updatePeerCountryResolutionState()
{
    const bool resolveCountries = Preferences::instance()->resolvePeerCountries();
    if (resolveCountries == m_resolveCountries)
        return;

    m_resolveCountries = resolveCountries;
    if (m_resolveCountries)
    {
        loadPeers(m_properties->getCurrentTorrent());
        showColumn(PeerListColumns::COUNTRY);
        if (columnWidth(PeerListColumns::COUNTRY) <= 0)
            resizeColumnToContents(PeerListColumns::COUNTRY);
    }
    else
    {
        hideColumn(PeerListColumns::COUNTRY);
    }
}

void PeerListWidget::showPeerListMenu(const QPoint &)
{
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // Add Peer Action
    // Do not allow user to add peers in a private torrent
    if (!torrent->isQueued() && !torrent->isChecking() && !torrent->isPrivate())
    {
        const QAction *addPeerAct = menu->addAction(UIThemeManager::instance()->getIcon("user-group-new"), tr("Add a new peer..."));
        connect(addPeerAct, &QAction::triggered, this, [this, torrent]()
        {
            const QVector<BitTorrent::PeerAddress> peersList = PeersAdditionDialog::askForPeers(this);
            const int peerCount = std::count_if(peersList.cbegin(), peersList.cend(), [torrent](const BitTorrent::PeerAddress &peer)
            {
                return torrent->connectPeer(peer);
            });
            if (peerCount < peersList.length())
                QMessageBox::information(this, tr("Adding peers"), tr("Some peers cannot be added. Check the Log for details."));
            else if (peerCount > 0)
                QMessageBox::information(this, tr("Adding peers"), tr("Peers are added to this torrent."));
        });
    }

    if (!selectionModel()->selectedRows().isEmpty())
    {
        const QAction *copyPeerAct = menu->addAction(UIThemeManager::instance()->getIcon("edit-copy"), tr("Copy IP:port"));
        connect(copyPeerAct, &QAction::triggered, this, &PeerListWidget::copySelectedPeers);

        menu->addSeparator();

        const QAction *banAct = menu->addAction(UIThemeManager::instance()->getIcon("user-group-delete"), tr("Ban peer permanently"));
        connect(banAct, &QAction::triggered, this, &PeerListWidget::banSelectedPeers);
    }

    if (menu->isEmpty())
        delete menu;
    else
        menu->popup(QCursor::pos());
}

void PeerListWidget::banSelectedPeers()
{
    // Store selected rows first as selected peers may disconnect
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();

    QVector<QString> selectedIPs;
    selectedIPs.reserve(selectedIndexes.size());

    for (const QModelIndex &index : selectedIndexes)
    {
        const int row = m_proxyModel->mapToSource(index).row();
        const QString ip = m_listModel->item(row, PeerListColumns::IP_HIDDEN)->text();
        selectedIPs += ip;
    }

    // Confirm before banning peer
    const QMessageBox::StandardButton btn = QMessageBox::question(this, tr("Ban peer permanently")
        , tr("Are you sure you want to permanently ban the selected peers?"));
    if (btn != QMessageBox::Yes) return;

    for (const QString &ip : selectedIPs)
    {
        BitTorrent::Session::instance()->banIP(ip);
        LogMsg(tr("Peer \"%1\" is manually banned").arg(ip));
    }
    // Refresh list
    loadPeers(m_properties->getCurrentTorrent());
}

void PeerListWidget::copySelectedPeers()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    QStringList selectedPeers;

    for (const QModelIndex &index : selectedIndexes)
    {
        const int row = m_proxyModel->mapToSource(index).row();
        const QString ip = m_listModel->item(row, PeerListColumns::IP_HIDDEN)->text();
        const QString port = m_listModel->item(row, PeerListColumns::PORT)->text();

        if (!ip.contains('.'))  // IPv6
            selectedPeers << ('[' + ip + "]:" + port);
        else  // IPv4
            selectedPeers << (ip + ':' + port);
    }

    QApplication::clipboard()->setText(selectedPeers.join('\n'));
}

void PeerListWidget::clear()
{
    m_peerItems.clear();
    m_itemsByIP.clear();
    const int nbrows = m_listModel->rowCount();
    if (nbrows > 0)
        m_listModel->removeRows(0, nbrows);
}

void PeerListWidget::loadSettings()
{
    header()->restoreState(Preferences::instance()->getPeerListState());
}

void PeerListWidget::saveSettings() const
{
    Preferences::instance()->setPeerListState(header()->saveState());
}

void PeerListWidget::loadPeers(const BitTorrent::TorrentHandle *torrent)
{
    if (!torrent) return;

    const QVector<BitTorrent::PeerInfo> peers = torrent->peers();
    QSet<PeerEndpoint> existingPeers;
    for (auto i = m_peerItems.cbegin(); i != m_peerItems.cend(); ++i)
        existingPeers << i.key();

    for (const BitTorrent::PeerInfo &peer : peers)
    {
        if (peer.address().ip.isNull()) continue;

        bool isNewPeer = false;
        updatePeer(torrent, peer, isNewPeer);
        if (!isNewPeer)
        {
            const PeerEndpoint peerEndpoint {peer.address(), peer.connectionType()};
            existingPeers.remove(peerEndpoint);
        }
    }

    // Remove peers that are gone
    for (const PeerEndpoint &peerEndpoint : asConst(existingPeers))
    {
        QStandardItem *item = m_peerItems.take(peerEndpoint);

        QSet<QStandardItem *> &items = m_itemsByIP[peerEndpoint.address.ip];
        items.remove(item);
        if (items.isEmpty())
            m_itemsByIP.remove(peerEndpoint.address.ip);

        m_listModel->removeRow(item->row());
    }
}

void PeerListWidget::updatePeer(const BitTorrent::TorrentHandle *torrent, const BitTorrent::PeerInfo &peer, bool &isNewPeer)
{
    const PeerEndpoint peerEndpoint {peer.address(), peer.connectionType()};
    const QString peerIp = peerEndpoint.address.ip.toString();
    const Qt::Alignment intDataTextAlignment = Qt::AlignRight | Qt::AlignVCenter;

    const auto setModelData =
        [this] (const int row, const int column, const QString &displayData
                , const QVariant &underlyingData, const Qt::Alignment textAlignmentData = {}
                , const QString &toolTip = {})
    {
        const QMap<int, QVariant> data =
        {
            {Qt::DisplayRole, displayData},
            {PeerListSortModel::UnderlyingDataRole, underlyingData},
            {Qt::TextAlignmentRole, QVariant {textAlignmentData}},
            {Qt::ToolTipRole, toolTip}
        };
        m_listModel->setItemData(m_listModel->index(row, column), data);
    };

    auto itemIter = m_peerItems.find(peerEndpoint);
    isNewPeer = (itemIter == m_peerItems.end());
    if (isNewPeer)
    {
        // new item
        const int row = m_listModel->rowCount();
        m_listModel->insertRow(row);

        setModelData(row, PeerListColumns::IP, peerIp, peerIp, {}, peerIp);
        setModelData(row, PeerListColumns::PORT, QString::number(peer.address().port), peer.address().port, intDataTextAlignment);
        setModelData(row, PeerListColumns::IP_HIDDEN, peerIp, peerIp);

        itemIter = m_peerItems.insert(peerEndpoint, m_listModel->item(row, PeerListColumns::IP));
        m_itemsByIP[peerEndpoint.address.ip].insert(itemIter.value());
    }

    const int row = (*itemIter)->row();
    const bool hideValues = Preferences::instance()->getHideZeroValues();

    setModelData(row, PeerListColumns::CONNECTION, peer.connectionType(), peer.connectionType());
    setModelData(row, PeerListColumns::FLAGS, peer.flags(), peer.flags(), {}, peer.flagsDescription());
    const QString client = peer.client().toHtmlEscaped();
    setModelData(row, PeerListColumns::CLIENT, client, client);
    setModelData(row, PeerListColumns::PROGRESS, (Utils::String::fromDouble(peer.progress() * 100, 1) + '%'), peer.progress(), intDataTextAlignment);
    const QString downSpeed = (hideValues && (peer.payloadDownSpeed() <= 0)) ? QString {} : Utils::Misc::friendlyUnit(peer.payloadDownSpeed(), true);
    setModelData(row, PeerListColumns::DOWN_SPEED, downSpeed, peer.payloadDownSpeed(), intDataTextAlignment);
    const QString upSpeed = (hideValues && (peer.payloadUpSpeed() <= 0)) ? QString {} : Utils::Misc::friendlyUnit(peer.payloadUpSpeed(), true);
    setModelData(row, PeerListColumns::UP_SPEED, upSpeed, peer.payloadUpSpeed(), intDataTextAlignment);
    const QString totalDown = (hideValues && (peer.totalDownload() <= 0)) ? QString {} : Utils::Misc::friendlyUnit(peer.totalDownload());
    setModelData(row, PeerListColumns::TOT_DOWN, totalDown, peer.totalDownload(), intDataTextAlignment);
    const QString totalUp = (hideValues && (peer.totalUpload() <= 0)) ? QString {} : Utils::Misc::friendlyUnit(peer.totalUpload());
    setModelData(row, PeerListColumns::TOT_UP, totalUp, peer.totalUpload(), intDataTextAlignment);
    setModelData(row, PeerListColumns::RELEVANCE, (Utils::String::fromDouble(peer.relevance() * 100, 1) + '%'), peer.relevance(), intDataTextAlignment);

    const QStringList downloadingFiles {torrent->info().filesForPiece(peer.downloadingPieceIndex())};
    const QString downloadingFilesDisplayValue = downloadingFiles.join(';');
    setModelData(row, PeerListColumns::DOWNLOADING_PIECE, downloadingFilesDisplayValue, downloadingFilesDisplayValue, {}, downloadingFiles.join('\n'));

    if (m_resolver)
        m_resolver->resolve(peerEndpoint.address.ip);

    if (m_resolveCountries)
    {
        const QIcon icon = UIThemeManager::instance()->getFlagIcon(peer.country());
        if (!icon.isNull())
        {
            m_listModel->setData(m_listModel->index(row, PeerListColumns::COUNTRY), icon, Qt::DecorationRole);
            const QString countryName = Net::GeoIPManager::CountryName(peer.country());
            m_listModel->setData(m_listModel->index(row, PeerListColumns::COUNTRY), countryName, Qt::ToolTipRole);
        }
    }
}

void PeerListWidget::handleResolved(const QHostAddress &ip, const QString &hostname) const
{
    if (hostname.isEmpty())
        return;

    const QSet<QStandardItem *> items = m_itemsByIP.value(ip);
    for (QStandardItem *item : items)
        item->setData(hostname, Qt::DisplayRole);
}

void PeerListWidget::handleSortColumnChanged(const int col)
{
    if (col == PeerListColumns::COUNTRY)
        m_proxyModel->setSortRole(Qt::ToolTipRole);
    else
        m_proxyModel->setSortRole(PeerListSortModel::UnderlyingDataRole);
}

void PeerListWidget::wheelEvent(QWheelEvent *event)
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
