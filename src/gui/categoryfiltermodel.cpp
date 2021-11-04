/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
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
#ifdef __ENABLE_CATEGORY__
#include "categoryfiltermodel.h"

#include <QHash>
#include <QIcon>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/global.h"
#include "uithememanager.h"

class CategoryModelItem
{
public:
    CategoryModelItem()
        : m_parent(nullptr)
        , m_torrentsCount(0)
    {
    }

    CategoryModelItem(CategoryModelItem *parent, QString categoryName, int torrentsCount = 0)
        : m_parent(nullptr)
        , m_name(categoryName)
        , m_torrentsCount(torrentsCount)
    {
        if (parent)
            parent->addChild(m_name, this);
    }

    ~CategoryModelItem()
    {
        clear();
        if (m_parent)
        {
            m_parent->m_torrentsCount -= m_torrentsCount;
            const QString uid = m_parent->m_children.key(this);
            m_parent->m_children.remove(uid);
            m_parent->m_childUids.removeOne(uid);
        }
    }

    QString name() const
    {
        return m_name;
    }

    QString fullName() const
    {
        if (!m_parent || m_parent->name().isEmpty())
            return m_name;

        return QString::fromLatin1("%1/%2").arg(m_parent->fullName(), m_name);
    }

    CategoryModelItem *parent() const
    {
        return m_parent;
    }

    int torrentsCount() const
    {
        return m_torrentsCount;
    }

    void increaseTorrentsCount()
    {
        ++m_torrentsCount;
        if (m_parent)
            m_parent->increaseTorrentsCount();
    }

    void decreaseTorrentsCount()
    {
        --m_torrentsCount;
        if (m_parent)
            m_parent->decreaseTorrentsCount();
    }

    int pos() const
    {
        if (!m_parent) return -1;

        return m_parent->m_childUids.indexOf(m_name);
    }

    bool hasChild(const QString &name) const
    {
        return m_children.contains(name);
    }

    int childCount() const
    {
        return m_children.count();
    }

    CategoryModelItem *child(const QString &uid) const
    {
        return m_children.value(uid);
    }

    CategoryModelItem *childAt(int index) const
    {
        if ((index < 0) || (index >= m_childUids.count()))
            return nullptr;

        return m_children[m_childUids[index]];
    }

    void addChild(const QString &uid, CategoryModelItem *item)
    {
        Q_ASSERT(item);
        Q_ASSERT(!item->parent());
        Q_ASSERT(!m_children.contains(uid));

        item->m_parent = this;
        m_children[uid] = item;
        m_childUids.append(uid);
        m_torrentsCount += item->torrentsCount();
    }

    void clear()
    {
        // use copy of m_children for qDeleteAll
        // to avoid collision when child removes
        // itself from parent children
        qDeleteAll(decltype(m_children)(m_children));
    }

private:
    CategoryModelItem *m_parent;
    QString m_name;
    int m_torrentsCount;
    QHash<QString, CategoryModelItem *> m_children;
    QStringList m_childUids;
};

namespace
{
    QString shortName(const QString &fullName)
    {
        int pos = fullName.lastIndexOf(QLatin1Char('/'));
        if (pos >= 0)
            return fullName.mid(pos + 1);
        return fullName;
    }
}

CategoryFilterModel::CategoryFilterModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_rootItem(new CategoryModelItem)
{
    using namespace BitTorrent;
    const auto *session = Session::instance();

    connect(session, &Session::categoryAdded, this, &CategoryFilterModel::categoryAdded);
    connect(session, &Session::categoryRemoved, this, &CategoryFilterModel::categoryRemoved);
    connect(session, &Session::torrentCategoryChanged, this, &CategoryFilterModel::torrentCategoryChanged);
    connect(session, &Session::subcategoriesSupportChanged, this, &CategoryFilterModel::subcategoriesSupportChanged);
    connect(session, &Session::torrentLoaded, this, &CategoryFilterModel::torrentAdded);
    connect(session, &Session::torrentAboutToBeRemoved, this, &CategoryFilterModel::torrentAboutToBeRemoved);

    populate();
}

CategoryFilterModel::~CategoryFilterModel()
{
    delete m_rootItem;
}

bool CategoryFilterModel::isSpecialItem(const QModelIndex &index)
{
    // the first two items at first level are special items:
    // 'All' and 'Uncategorized'
    return (!index.parent().isValid() && (index.row() <= 1));
}

int CategoryFilterModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant CategoryFilterModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};

    auto item = static_cast<const CategoryModelItem *>(index.internalPointer());

    if ((index.column() == 0) && (role == Qt::DecorationRole))
    {
        return UIThemeManager::instance()->getIcon("inode-directory");
    }

    if ((index.column() == 0) && (role == Qt::DisplayRole))
    {
        return QString(QStringLiteral("%1 (%2)"))
                .arg(item->name()).arg(item->torrentsCount());
    }

    if ((index.column() == 0) && (role == Qt::UserRole))
    {
        return item->torrentsCount();
    }

    return {};
}

Qt::ItemFlags CategoryFilterModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant CategoryFilterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
        if (section == 0)
            return tr("Categories");

    return {};
}

QModelIndex CategoryFilterModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column > 0)
        return {};

    if (parent.isValid() && (parent.column() != 0))
        return {};

    auto parentItem = parent.isValid() ? static_cast<CategoryModelItem *>(parent.internalPointer())
                                       : m_rootItem;
    if (row < parentItem->childCount())
        return createIndex(row, column, parentItem->childAt(row));

    return {};
}

QModelIndex CategoryFilterModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};

    auto item = static_cast<CategoryModelItem *>(index.internalPointer());
    if (!item) return {};

    return this->index(item->parent());
}

int CategoryFilterModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        return m_rootItem->childCount();

    auto item = static_cast<CategoryModelItem *>(parent.internalPointer());
    if (!item) return 0;

    return item->childCount();
}

QModelIndex CategoryFilterModel::index(const QString &categoryName) const
{
    return index(findItem(categoryName));
}

QString CategoryFilterModel::categoryName(const QModelIndex &index) const
{
    if (!index.isValid()) return {};
    return static_cast<CategoryModelItem *>(index.internalPointer())->fullName();
}

QModelIndex CategoryFilterModel::index(CategoryModelItem *item) const
{
    if (!item || !item->parent()) return {};

    return index(item->pos(), 0, index(item->parent()));
}

void CategoryFilterModel::categoryAdded(const QString &categoryName)
{
    CategoryModelItem *parent = m_rootItem;

    if (m_isSubcategoriesEnabled)
    {
        QStringList expanded = BitTorrent::Session::expandCategory(categoryName);
        if (expanded.count() > 1)
            parent = findItem(expanded[expanded.count() - 2]);
    }

    int row = parent->childCount();
    beginInsertRows(index(parent), row, row);
    new CategoryModelItem(
            parent, m_isSubcategoriesEnabled ? shortName(categoryName) : categoryName);
    endInsertRows();
}

void CategoryFilterModel::categoryRemoved(const QString &categoryName)
{
    auto item = findItem(categoryName);
    if (item)
    {
        QModelIndex i = index(item);
        beginRemoveRows(i.parent(), i.row(), i.row());
        delete item;
        endRemoveRows();
    }
}

void CategoryFilterModel::torrentAdded(BitTorrent::TorrentHandle *const torrent)
{
    CategoryModelItem *item = findItem(torrent->category());
    Q_ASSERT(item);

    item->increaseTorrentsCount();
    m_rootItem->childAt(0)->increaseTorrentsCount();
}

void CategoryFilterModel::torrentAboutToBeRemoved(BitTorrent::TorrentHandle *const torrent)
{
    CategoryModelItem *item = findItem(torrent->category());
    Q_ASSERT(item);

    item->decreaseTorrentsCount();
    m_rootItem->childAt(0)->decreaseTorrentsCount();
}

void CategoryFilterModel::torrentCategoryChanged(BitTorrent::TorrentHandle *const torrent, const QString &oldCategory)
{
    QModelIndex i;

    auto item = findItem(oldCategory);
    Q_ASSERT(item);

    item->decreaseTorrentsCount();
    i = index(item);
    while (i.isValid())
    {
        emit dataChanged(i, i);
        i = parent(i);
    }

    item = findItem(torrent->category());
    Q_ASSERT(item);

    item->increaseTorrentsCount();
    i = index(item);
    while (i.isValid())
    {
        emit dataChanged(i, i);
        i = parent(i);
    }
}

void CategoryFilterModel::subcategoriesSupportChanged()
{
    beginResetModel();
    populate();
    endResetModel();
}

void CategoryFilterModel::populate()
{
    m_rootItem->clear();

    const auto *session = BitTorrent::Session::instance();
    const auto torrents = session->torrents();
    m_isSubcategoriesEnabled = session->isSubcategoriesEnabled();

    const QString UID_ALL;
    const QString UID_UNCATEGORIZED(QChar(1));

    // All torrents
    m_rootItem->addChild(UID_ALL, new CategoryModelItem(nullptr, tr("All"), torrents.count()));

    // Uncategorized torrents
    using Torrent = BitTorrent::TorrentHandle;
    m_rootItem->addChild(
                UID_UNCATEGORIZED
                , new CategoryModelItem(
                    nullptr, tr("Uncategorized")
                    , std::count_if(torrents.begin(), torrents.end()
                                    , [](Torrent *torrent) { return torrent->category().isEmpty(); })));

    using Torrent = BitTorrent::TorrentHandle;
    for (auto i = session->categories().cbegin(); i != session->categories().cend(); ++i)
    {
        const QString &category = i.key();
        if (m_isSubcategoriesEnabled)
        {
            CategoryModelItem *parent = m_rootItem;
            for (const QString &subcat : asConst(session->expandCategory(category)))
            {
                const QString subcatName = shortName(subcat);
                if (!parent->hasChild(subcatName))
                {
                    new CategoryModelItem(
                                parent, subcatName
                                , std::count_if(torrents.cbegin(), torrents.cend()
                                                , [subcat](Torrent *torrent) { return torrent->category() == subcat; }));
                }
                parent = parent->child(subcatName);
            }
        }
        else
        {
            new CategoryModelItem(
                        m_rootItem, category
                        , std::count_if(torrents.begin(), torrents.end()
                                        , [category](Torrent *torrent) { return torrent->belongsToCategory(category); }));
        }
    }
}

CategoryModelItem *CategoryFilterModel::findItem(const QString &fullName) const
{
    if (fullName.isEmpty())
        return m_rootItem->childAt(1); // "Uncategorized" item

    if (!m_isSubcategoriesEnabled)
        return m_rootItem->child(fullName);

    CategoryModelItem *item = m_rootItem;
    for (const QString &subcat : asConst(BitTorrent::Session::expandCategory(fullName)))
    {
        const QString subcatName = shortName(subcat);
        if (!item->hasChild(subcatName)) return nullptr;
        item = item->child(subcatName);
    }

    return item;
}
#endif
