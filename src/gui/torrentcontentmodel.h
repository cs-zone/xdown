/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006-2012  Christophe Dumez <chris@qbittorrent.org>
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

#include <QAbstractItemModel>
#include <QVector>

#include "torrentcontentmodelitem.h"

class QFileIconProvider;
class QModelIndex;
class QVariant;

class TorrentContentModelFile;

namespace BitTorrent
{
    class TorrentInfo;
}

class TorrentContentModel final : public QAbstractItemModel
{
    Q_OBJECT
    Q_DISABLE_COPY(TorrentContentModel)

public:
    enum Roles
    {
        UnderlyingDataRole = Qt::UserRole
    };

    TorrentContentModel(QObject *parent = nullptr);
    ~TorrentContentModel() override;

    void updateFilesProgress(const QVector<qreal> &fp);
    void updateFilesPriorities(const QVector<BitTorrent::DownloadPriority> &fprio);
    void updateFilesAvailability(const QVector<qreal> &fa);
    QVector<BitTorrent::DownloadPriority> getFilePriorities() const;
    bool allFiltered() const;
    int columnCount(const QModelIndex &parent = {}) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    TorrentContentModelItem::ItemType itemType(const QModelIndex &index) const;
    int getFileIndex(const QModelIndex &index);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    void clear();
    void setupModelData(const BitTorrent::TorrentInfo &info);

signals:
    void filteredFilesChanged();

public slots:
    void selectAll();
    void selectNone();

private:
    TorrentContentModelFolder *m_rootItem;
    QVector<TorrentContentModelFile *> m_filesIndex;
    QFileIconProvider *m_fileIconProvider;
};
