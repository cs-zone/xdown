/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include "magneturi.h"

#include <libtorrent/bencode.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/sha1_hash.hpp>

#include <QRegularExpression>
#include <QUrl>

#include "infohash.h"

namespace
{
    bool isBitTorrentInfoHash(const QString &string)
    {
        // There are 2 representations for BitTorrent info hash:
        // 1. 40 chars hex-encoded string
        //      == 20 (SHA-1 length in bytes) * 2 (each byte maps to 2 hex characters)
        // 2. 32 chars Base32 encoded string
        //      == 20 (SHA-1 length in bytes) * 1.6 (the efficiency of Base32 encoding)
        const int SHA1_HEX_SIZE = BitTorrent::InfoHash::length() * 2;
        const int SHA1_BASE32_SIZE = BitTorrent::InfoHash::length() * 1.6;

        return ((((string.size() == SHA1_HEX_SIZE))
                && !string.contains(QRegularExpression(QLatin1String("[^0-9A-Fa-f]"))))
            || ((string.size() == SHA1_BASE32_SIZE)
                && !string.contains(QRegularExpression(QLatin1String("[^2-7A-Za-z]")))));
    }
}

using namespace BitTorrent;

MagnetUri::MagnetUri(const QString &source)
    : m_valid(false)
    , m_url(source)
{
    if (source.isEmpty()) return;

    if (isBitTorrentInfoHash(source))
        m_url = QLatin1String("magnet:?xt=urn:btih:") + source;

    lt::error_code ec;
    lt::parse_magnet_uri(m_url.toStdString(), m_addTorrentParams, ec);
    if (ec) return;

    m_valid = true;
    m_hash = m_addTorrentParams.info_hash;
    m_name = QString::fromStdString(m_addTorrentParams.name);

    m_trackers.reserve(m_addTorrentParams.trackers.size());
    for (const std::string &tracker : m_addTorrentParams.trackers)
        m_trackers.append(lt::announce_entry {tracker});

    m_urlSeeds.reserve(m_addTorrentParams.url_seeds.size());
    for (const std::string &urlSeed : m_addTorrentParams.url_seeds)
        m_urlSeeds.append(QUrl(QString::fromStdString(urlSeed)));
}

bool MagnetUri::isValid() const
{
    return m_valid;
}

InfoHash MagnetUri::hash() const
{
    return m_hash;
}

QString MagnetUri::name() const
{
    return m_name;
}

QVector<TrackerEntry> MagnetUri::trackers() const
{
    return m_trackers;
}

QVector<QUrl> MagnetUri::urlSeeds() const
{
    return m_urlSeeds;
}

QString MagnetUri::url() const
{
    return m_url;
}

lt::add_torrent_params MagnetUri::addTorrentParams() const
{
    return m_addTorrentParams;
}
