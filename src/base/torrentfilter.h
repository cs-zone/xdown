/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QSet>
#include <QString>

#include "base/bittorrent/infohash.h"

namespace BitTorrent
{
    class TorrentHandle;
}

using InfoHashSet = QSet<BitTorrent::InfoHash>;

class TorrentFilter
{
public:
    enum Type
    {
        All,
        Downloading,
#ifdef __ENABLE_ALL_STATUS__
        Seeding,
#endif
        Completed,
#ifdef __ENABLE_ALL_STATUS__
        Resumed,
        Paused,
#endif
        Active,
        Inactive,
#ifdef __ENABLE_ALL_STATUS__
        Stalled,
        StalledUploading,
        StalledDownloading,
        Errored
#endif
    };

    // These mean any permutation, including no category / tag.
    static const QString AnyCategory;
    static const InfoHashSet AnyHash;
    static const QString AnyTag;

    static const TorrentFilter DownloadingTorrent;
    static const TorrentFilter SeedingTorrent;
    static const TorrentFilter CompletedTorrent;
    static const TorrentFilter PausedTorrent;
    static const TorrentFilter ResumedTorrent;
    static const TorrentFilter ActiveTorrent;
    static const TorrentFilter InactiveTorrent;
    static const TorrentFilter StalledTorrent;
    static const TorrentFilter StalledUploadingTorrent;
    static const TorrentFilter StalledDownloadingTorrent;
    static const TorrentFilter ErroredTorrent;

    TorrentFilter() = default;
    // category & tags: pass empty string for uncategorized / untagged torrents.
    // Pass null string (QString()) to disable filtering (i.e. all torrents).
    TorrentFilter(Type type, const InfoHashSet &hashSet = AnyHash
#ifdef __ENABLE_CATEGORY__
        ,const QString &category = AnyCategory, const QString &tag = AnyTag
#endif
    );
    TorrentFilter(const QString &filter, const InfoHashSet &hashSet = AnyHash
#ifdef __ENABLE_CATEGORY__
        , const QString &category = AnyCategory, const QString &tags = AnyTag
#endif
    );

    bool setType(Type type);
    bool setTypeByName(const QString &filter);
    bool setHashSet(const InfoHashSet &hashSet);
#ifdef __ENABLE_TRACKER_TREE__
    bool setCategory(const QString &category);
    bool setTag(const QString &tag);
#endif
    bool match(const BitTorrent::TorrentHandle *torrent) const;

private:
    bool matchState(const BitTorrent::TorrentHandle *torrent) const;
    bool matchHash(const BitTorrent::TorrentHandle *torrent) const;
#ifdef __ENABLE_CATEGORY__
    bool matchCategory(const BitTorrent::TorrentHandle *torrent) const;
    bool matchTag(const BitTorrent::TorrentHandle *torrent) const;
#endif

    Type m_type {All};
#ifdef __ENABLE_CATEGORY__
    QString m_category;
    QString m_tag;
#endif
    InfoHashSet m_hashSet;
};
