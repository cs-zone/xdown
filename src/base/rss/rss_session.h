/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2010  Arnaud Demaiziere <arnaud@qbittorrent.org>
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

/*
 * RSS Session configuration file format (JSON):
 *
 * =============== BEGIN ===============
 *
 {
 *     "folder1":
 {
 *         "subfolder1":
 {
 *             "Feed name 1 (Alias)":
 {
 *                 "uid": "feed unique identifier",
 *                 "url": "http://some-feed-url1"
 *             }
 *             "Feed name 2 (Alias)":
 {
 *                 "uid": "feed unique identifier",
 *                 "url": "http://some-feed-url2"
 *             }
 *         },
 *         "subfolder2": {},
 *         "Feed name 3 (Alias)":
 {
 *             "uid": "feed unique identifier",
 *             "url": "http://some-feed-url3"
 *         }
 *     },
 *     "folder2": {},
 *     "folder3": {}
 * }
 * ================ END ================
 *
 * 1.   Document is JSON object (the same as Folder)
 * 2.   Folder is JSON object (keys are Item names, values are Items)
 * 3.   Feed is JSON object (keys are property names, values are property values; 'uid' and 'url' are required)
 */

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>

class QThread;

class Application;
class AsyncFileStorage;

namespace RSS
{
    class Feed;
    class Folder;
    class Item;

    class Session : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(Session)

        friend class ::Application;

        Session();
        ~Session() override;

    public:
        static Session *instance();

        bool isProcessingEnabled() const;
        void setProcessingEnabled(bool enabled);

        QThread *workingThread() const;
        AsyncFileStorage *confFileStorage() const;
        AsyncFileStorage *dataFileStorage() const;

        int maxArticlesPerFeed() const;
        void setMaxArticlesPerFeed(int n);

        int refreshInterval() const;
        void setRefreshInterval(int refreshInterval);

        bool addFolder(const QString &path, QString *error = nullptr);
        bool addFeed(const QString &url, const QString &path, QString *error = nullptr);
        bool moveItem(const QString &itemPath, const QString &destPath
                         , QString *error = nullptr);
        bool moveItem(Item *item, const QString &destPath, QString *error = nullptr);
        bool removeItem(const QString &itemPath, QString *error = nullptr);

        QList<Item *> items() const;
        Item *itemByPath(const QString &path) const;
        QList<Feed *> feeds() const;
        Feed *feedByURL(const QString &url) const;

        Folder *rootFolder() const;

    public slots:
        void refresh();

    signals:
        void processingStateChanged(bool enabled);
        void maxArticlesPerFeedChanged(int n);
        void itemAdded(Item *item);
        void itemPathChanged(Item *item);
        void itemAboutToBeRemoved(Item *item);
        void feedIconLoaded(Feed *feed);
        void feedStateChanged(Feed *feed);

    private slots:
        void handleItemAboutToBeDestroyed(Item *item);
        void handleFeedTitleChanged(Feed *feed);

    private:
        QUuid generateUID() const;
        void load();
        void loadFolder(const QJsonObject &jsonObj, Folder *folder);
        void loadLegacy();
        void store();
        Folder *prepareItemDest(const QString &path, QString *error);
        Folder *addSubfolder(const QString &name, Folder *parentFolder);
        Feed *addFeedToFolder(const QUuid &uid, const QString &url, const QString &name, Folder *parentFolder);
        void addItem(Item *item, Folder *destFolder);

        static QPointer<Session> m_instance;

        bool m_processingEnabled;
        QThread *m_workingThread;
        AsyncFileStorage *m_confFileStorage;
        AsyncFileStorage *m_dataFileStorage;
        QTimer m_refreshTimer;
        int m_refreshInterval;
        int m_maxArticlesPerFeed;
        QHash<QString, Item *> m_itemsByPath;
        QHash<QUuid, Feed *> m_feedsByUID;
        QHash<QString, Feed *> m_feedsByURL;
    };
}
