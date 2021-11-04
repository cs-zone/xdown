/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include "feedlistwidget.h"

#include <QDragMoveEvent>
#include <QDropEvent>
#include <QTreeWidgetItem>

#include "base/global.h"
#include "base/rss/rss_article.h"
#include "base/rss/rss_feed.h"
#include "base/rss/rss_folder.h"
#include "base/rss/rss_session.h"
#include "gui/uithememanager.h"

FeedListWidget::FeedListWidget(QWidget *parent)
    : QTreeWidget(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setDragDropMode(QAbstractItemView::InternalMove);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setColumnCount(1);
    headerItem()->setText(0, tr("RSS feeds"));

    connect(RSS::Session::instance(), &RSS::Session::itemAdded, this, &FeedListWidget::handleItemAdded);
    connect(RSS::Session::instance(), &RSS::Session::feedStateChanged, this, &FeedListWidget::handleFeedStateChanged);
    connect(RSS::Session::instance(), &RSS::Session::feedIconLoaded, this, &FeedListWidget::handleFeedIconLoaded);
    connect(RSS::Session::instance(), &RSS::Session::itemPathChanged, this, &FeedListWidget::handleItemPathChanged);
    connect(RSS::Session::instance(), &RSS::Session::itemAboutToBeRemoved, this, &FeedListWidget::handleItemAboutToBeRemoved);

    m_rssToTreeItemMapping[RSS::Session::instance()->rootFolder()] = invisibleRootItem();

    m_unreadStickyItem = new QTreeWidgetItem(this);
    m_unreadStickyItem->setData(0, Qt::UserRole, reinterpret_cast<quintptr>(RSS::Session::instance()->rootFolder()));
    m_unreadStickyItem->setText(0, tr("Unread  (%1)").arg(RSS::Session::instance()->rootFolder()->unreadCount()));
    m_unreadStickyItem->setData(0, Qt::DecorationRole, UIThemeManager::instance()->getIcon("mail-folder-inbox"));

    connect(RSS::Session::instance()->rootFolder(), &RSS::Item::unreadCountChanged, this, &FeedListWidget::handleItemUnreadCountChanged);

    setSortingEnabled(false);
    fill(nullptr, RSS::Session::instance()->rootFolder());
    setSortingEnabled(true);

//    setCurrentItem(m_unreadStickyItem);
}

void FeedListWidget::handleItemAdded(RSS::Item *rssItem)
{
    auto parentItem = m_rssToTreeItemMapping.value(
                RSS::Session::instance()->itemByPath(RSS::Item::parentPath(rssItem->path())));
    createItem(rssItem, parentItem);
}

void FeedListWidget::handleFeedStateChanged(RSS::Feed *feed)
{
    QTreeWidgetItem *item = m_rssToTreeItemMapping.value(feed);
    Q_ASSERT(item);

    QIcon icon;
    if (feed->isLoading())
        icon = UIThemeManager::instance()->getIcon(QLatin1String("loading"));
    else if (feed->hasError())
        icon = UIThemeManager::instance()->getIcon(QLatin1String("unavailable"));
    else if (!feed->iconPath().isEmpty())
        icon = QIcon(feed->iconPath());
    else
        icon = UIThemeManager::instance()->getIcon(QLatin1String("application-rss+xml"));
    item->setData(0, Qt::DecorationRole, icon);
}

void FeedListWidget::handleFeedIconLoaded(RSS::Feed *feed)
{
    if (!feed->isLoading() && !feed->hasError())
    {
        QTreeWidgetItem *item = m_rssToTreeItemMapping.value(feed);
        Q_ASSERT(item);

        item->setData(0, Qt::DecorationRole, QIcon(feed->iconPath()));
    }
}

void FeedListWidget::handleItemUnreadCountChanged(RSS::Item *rssItem)
{
    if (rssItem == RSS::Session::instance()->rootFolder())
    {
        m_unreadStickyItem->setText(0, tr("Unread  (%1)").arg(RSS::Session::instance()->rootFolder()->unreadCount()));
    }
    else
    {
        QTreeWidgetItem *item = mapRSSItem(rssItem);
        Q_ASSERT(item);
        item->setData(0, Qt::DisplayRole, QString::fromLatin1("%1  (%2)").arg(rssItem->name(), QString::number(rssItem->unreadCount())));
    }
}

void FeedListWidget::handleItemPathChanged(RSS::Item *rssItem)
{
    QTreeWidgetItem *item = mapRSSItem(rssItem);
    Q_ASSERT(item);

    item->setData(0, Qt::DisplayRole, QString::fromLatin1("%1  (%2)").arg(rssItem->name(), QString::number(rssItem->unreadCount())));

    RSS::Item *parentRssItem = RSS::Session::instance()->itemByPath(RSS::Item::parentPath(rssItem->path()));
    QTreeWidgetItem *parentItem = mapRSSItem(parentRssItem);
    Q_ASSERT(parentItem);

    parentItem->addChild(item);
}

void FeedListWidget::handleItemAboutToBeRemoved(RSS::Item *rssItem)
{
    rssItem->disconnect(this);
    delete m_rssToTreeItemMapping.take(rssItem);

    // RSS Item is still valid in this slot so if it is the last
    // item we should prevent Unread list populating
    if (m_rssToTreeItemMapping.size() == 1)
        setCurrentItem(nullptr);
}

QTreeWidgetItem *FeedListWidget::stickyUnreadItem() const
{
    return m_unreadStickyItem;
}

QList<QTreeWidgetItem *> FeedListWidget::getAllOpenedFolders(QTreeWidgetItem *parent) const
{
    QList<QTreeWidgetItem *> openedFolders;
    int nbChildren = (parent ? parent->childCount() : topLevelItemCount());
    for (int i = 0; i < nbChildren; ++i)
    {
        QTreeWidgetItem *item (parent ? parent->child(i) : topLevelItem(i));
        if (isFolder(item) && item->isExpanded())
        {
            QList<QTreeWidgetItem *> openedSubfolders = getAllOpenedFolders(item);
            if (!openedSubfolders.empty())
                openedFolders << openedSubfolders;
            else
                openedFolders << item;
        }
    }
    return openedFolders;
}

RSS::Item *FeedListWidget::getRSSItem(QTreeWidgetItem *item) const
{
    if (!item) return nullptr;

    return reinterpret_cast<RSS::Item *>(item->data(0, Qt::UserRole).value<quintptr>());
}

QTreeWidgetItem *FeedListWidget::mapRSSItem(RSS::Item *rssItem) const
{
    return m_rssToTreeItemMapping.value(rssItem);
}

QString FeedListWidget::itemPath(QTreeWidgetItem *item) const
{
    return getRSSItem(item)->path();
}

bool FeedListWidget::isFeed(QTreeWidgetItem *item) const
{
    return qobject_cast<RSS::Feed *>(getRSSItem(item));
}

bool FeedListWidget::isFolder(QTreeWidgetItem *item) const
{
    return qobject_cast<RSS::Folder *>(getRSSItem(item));
}

void FeedListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    QTreeWidget::dragMoveEvent(event);

    QTreeWidgetItem *item = itemAt(event->pos());
    // Prohibit dropping onto global unread counter
    if (item == m_unreadStickyItem)
        event->ignore();
    // Prohibit dragging of global unread counter
    else if (selectedItems().contains(m_unreadStickyItem))
        event->ignore();
    // Prohibit dropping onto feeds
    else if (item && isFeed(item))
        event->ignore();
}

void FeedListWidget::dropEvent(QDropEvent *event)
{
    QTreeWidgetItem *destFolderItem = itemAt(event->pos());
    RSS::Folder *destFolder = (destFolderItem
                               ? static_cast<RSS::Folder *>(getRSSItem(destFolderItem))
                               : RSS::Session::instance()->rootFolder());

    // move as much items as possible
    for (QTreeWidgetItem *srcItem : asConst(selectedItems()))
    {
        auto rssItem = getRSSItem(srcItem);
        RSS::Session::instance()->moveItem(rssItem, RSS::Item::joinPath(destFolder->path(), rssItem->name()));
    }

    QTreeWidget::dropEvent(event);
    if (destFolderItem)
        destFolderItem->setExpanded(true);
}

QTreeWidgetItem *FeedListWidget::createItem(RSS::Item *rssItem, QTreeWidgetItem *parentItem)
{
    auto *item = new QTreeWidgetItem;
    item->setData(0, Qt::DisplayRole, QString::fromLatin1("%1  (%2)").arg(rssItem->name(), QString::number(rssItem->unreadCount())));
    item->setData(0, Qt::UserRole, reinterpret_cast<quintptr>(rssItem));
    m_rssToTreeItemMapping[rssItem] = item;

    QIcon icon;
    if (auto feed = qobject_cast<RSS::Feed *>(rssItem))
    {
        if (feed->isLoading())
            icon = UIThemeManager::instance()->getIcon(QLatin1String("loading"));
        else if (feed->hasError())
            icon = UIThemeManager::instance()->getIcon(QLatin1String("unavailable"));
        else if (!feed->iconPath().isEmpty())
            icon = QIcon(feed->iconPath());
        else
            icon = UIThemeManager::instance()->getIcon(QLatin1String("application-rss+xml"));
    }
    else
    {
        icon = UIThemeManager::instance()->getIcon(QLatin1String("inode-directory"));
    }
    item->setData(0, Qt::DecorationRole, icon);

    connect(rssItem, &RSS::Item::unreadCountChanged, this, &FeedListWidget::handleItemUnreadCountChanged);

    if (!parentItem || (parentItem == m_unreadStickyItem))
        addTopLevelItem(item);
    else
        parentItem->addChild(item);

    return item;
}

void FeedListWidget::fill(QTreeWidgetItem *parent, RSS::Folder *rssParent)
{
    for (const auto rssItem : asConst(rssParent->items()))
    {
        QTreeWidgetItem *item = createItem(rssItem, parent);
        // Recursive call if this is a folder.
        if (auto folder = qobject_cast<RSS::Folder *>(rssItem))
            fill(item, folder);
    }
}
