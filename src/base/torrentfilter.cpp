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

#include "torrentfilter.h"

#include "bittorrent/infohash.h"
#include "bittorrent/torrenthandle.h"

const QString TorrentFilter::AnyCategory;
const InfoHashSet TorrentFilter::AnyHash {{}};
const QString TorrentFilter::AnyTag;

const TorrentFilter TorrentFilter::DownloadingTorrent(TorrentFilter::Downloading);
#ifdef __ENABLE_ALL_STATUS__
const TorrentFilter TorrentFilter::SeedingTorrent(TorrentFilter::Seeding);
#endif
const TorrentFilter TorrentFilter::CompletedTorrent(TorrentFilter::Completed);
#ifdef __ENABLE_ALL_STATUS__
const TorrentFilter TorrentFilter::PausedTorrent(TorrentFilter::Paused);
const TorrentFilter TorrentFilter::ResumedTorrent(TorrentFilter::Resumed);
const TorrentFilter TorrentFilter::StalledTorrent(TorrentFilter::Stalled);
const TorrentFilter TorrentFilter::StalledUploadingTorrent(TorrentFilter::StalledUploading);
const TorrentFilter TorrentFilter::StalledDownloadingTorrent(TorrentFilter::StalledDownloading);
const TorrentFilter TorrentFilter::ErroredTorrent(TorrentFilter::Errored);
#endif

const TorrentFilter TorrentFilter::ActiveTorrent(TorrentFilter::Active);
const TorrentFilter TorrentFilter::InactiveTorrent(TorrentFilter::Inactive);

using BitTorrent::TorrentHandle;

TorrentFilter::TorrentFilter(const Type type, const InfoHashSet &hashSet)
    : m_type(type)
#ifdef __ENABLE_CATEGORY__
    , m_category(category)
    , m_tag(tag)
#endif
    , m_hashSet(hashSet)
{
}

TorrentFilter::TorrentFilter(const QString &filter, const InfoHashSet &hashSet)
    : m_type(All)
#ifdef __ENABLE_CATEGORY__
    , m_category(category)
    , m_tag(tag)
#endif
    , m_hashSet(hashSet)
{
    setTypeByName(filter);
}

bool TorrentFilter::setType(Type type)
{
    if (m_type != type)
    {
        m_type = type;
        return true;
    }

    return false;
}

bool TorrentFilter::setTypeByName(const QString &filter)
{
    Type type = All;

    if (filter == "downloading")
        type = Downloading;
#ifdef __ENABLE_ALL_STATUS__
    else if (filter == "seeding")
        type = Seeding;
#endif
    else if (filter == "completed")
        type = Completed;
#ifdef __ENABLE_ALL_STATUS__
    else if (filter == "paused")
        type = Paused;
    else if (filter == "resumed")
        type = Resumed;
    else if (filter == "active")
        type = Active;
    else if (filter == "inactive")
        type = Inactive;
    else if (filter == "stalled")
        type = Stalled;
    else if (filter == "stalled_uploading")
        type = StalledUploading;
    else if (filter == "stalled_downloading")
        type = StalledDownloading;
    else if (filter == "errored")
        type = Errored;
#endif

    return setType(type);
}

#ifdef __ENABLE_TRACKER_TREE__
bool TorrentFilter::setHashSet(const QStringSet &hashSet)
{
    if (m_hashSet != hashSet)
    {
        m_hashSet = hashSet;
        return true;
    }

    return false;
}
#endif

#ifdef __ENABLE_CATEGORY__
bool TorrentFilter::setCategory(const QString &category)
{
    // QString::operator==() doesn't distinguish between empty and null strings.
    if ((m_category != category)
            || (m_category.isNull() && !category.isNull())
            || (!m_category.isNull() && category.isNull()))
            {
        m_category = category;
        return true;
    }

    return false;
}

bool TorrentFilter::setTag(const QString &tag)
{
    // QString::operator==() doesn't distinguish between empty and null strings.
    if ((m_tag != tag)
        || (m_tag.isNull() && !tag.isNull())
        || (!m_tag.isNull() && tag.isNull()))
        {
        m_tag = tag;
        return true;
    }

    return false;
}
#endif

bool TorrentFilter::match(const TorrentHandle *const torrent) const
{
    if (!torrent) return false;

    return (matchState(torrent)
#ifdef __ENABLE_TRACKER_TREE__
        && matchHash(torrent)
#endif
#ifdef __ENABLE_CATEGORY__
        && matchCategory(torrent) && matchTag(torrent)
#endif
        );
}

// ×´Ì¬¹ýÂËÌõ¼þ
bool TorrentFilter::matchState(const BitTorrent::TorrentHandle *const torrent) const
{
    switch (m_type)
    {
    case All:
        return true;
    case Downloading:
        return torrent->isDownloading();
#ifdef __ENABLE_ALL_STATUS__
    case Seeding:
        return torrent->isUploading();
#endif
    case Completed:
        return torrent->isCompleted();
#ifdef __ENABLE_ALL_STATUS__
    case Paused:
        return torrent->isPaused();
    case Resumed:
        return torrent->isResumed();
    case Active:
        if (torrent->getHandleType() == BitTorrent::TaskHandleType::XDown_Handle) {
            return false;
        }
        else {
            return torrent->isActive();
        }
    case Inactive:
        return torrent->isInactive();
    case Stalled:
        return (torrent->state() ==  BitTorrent::TorrentState::StalledUploading)
                || (torrent->state() ==  BitTorrent::TorrentState::StalledDownloading);
    case StalledUploading:
        return torrent->state() ==  BitTorrent::TorrentState::StalledUploading;
    case StalledDownloading:
        return torrent->state() ==  BitTorrent::TorrentState::StalledDownloading;
    case Errored:
        return torrent->isErrored();
#endif
    default: // All
        return true;
    }
}

bool TorrentFilter::matchHash(const BitTorrent::TorrentHandle *const torrent) const
{
    if (m_hashSet == AnyHash) return true;

    return m_hashSet.contains(torrent->hash());
}

#ifdef __ENABLE_CATEGORY__
bool TorrentFilter::matchCategory(const BitTorrent::TorrentHandle *const torrent) const
{
    if (m_category.isNull()) return true;

    return (torrent->belongsToCategory(m_category));
}

bool TorrentFilter::matchTag(const BitTorrent::TorrentHandle *const torrent) const
{
    // Empty tag is a special value to indicate we're filtering for untagged torrents.
    if (m_tag.isNull()) return true;
    if (m_tag.isEmpty()) return torrent->tags().isEmpty();

    return (torrent->hasTag(m_tag));
}
#endif
