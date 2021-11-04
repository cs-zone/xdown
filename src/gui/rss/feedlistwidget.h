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

#pragma once

#include <QHash>
#include <QTreeWidget>

namespace RSS
{
    class Article;
    class Feed;
    class Folder;
    class Item;
}

class FeedListWidget final : public QTreeWidget
{
    Q_OBJECT

public:
    explicit FeedListWidget(QWidget *parent);

    QTreeWidgetItem *stickyUnreadItem() const;
    QList<QTreeWidgetItem *> getAllOpenedFolders(QTreeWidgetItem *parent = nullptr) const;
    RSS::Item *getRSSItem(QTreeWidgetItem *item) const;
    QTreeWidgetItem *mapRSSItem(RSS::Item *rssItem) const;
    QString itemPath(QTreeWidgetItem *item) const;
    bool isFeed(QTreeWidgetItem *item) const;
    bool isFolder(QTreeWidgetItem *item) const;

private slots:
    void handleItemAdded(RSS::Item *rssItem);
    void handleFeedStateChanged(RSS::Feed *feed);
    void handleFeedIconLoaded(RSS::Feed *feed);
    void handleItemUnreadCountChanged(RSS::Item *rssItem);
    void handleItemPathChanged(RSS::Item *rssItem);
    void handleItemAboutToBeRemoved(RSS::Item *rssItem);

private:
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    QTreeWidgetItem *createItem(RSS::Item *rssItem, QTreeWidgetItem *parentItem = nullptr);
    void fill(QTreeWidgetItem *parent, RSS::Folder *rssParent);

    QHash<RSS::Item *, QTreeWidgetItem *> m_rssToTreeItemMapping;
    QTreeWidgetItem *m_unreadStickyItem;
};
