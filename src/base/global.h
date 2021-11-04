/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Mike Tzou
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

#include <type_traits>
#include <QtGlobal>

#if (QT_POINTER_SIZE == 8)
#define QBT_APP_64BIT
#endif

const char C_TORRENT_FILE_EXTENSION[] = ".torrent";
const int MAX_TORRENT_SIZE = 100 * 1024 * 1024; // 100 MiB

template <typename T>
constexpr typename std::add_const<T>::type &asConst(T &t) noexcept { return t; }

// Forward rvalue as const
template <typename T>
constexpr typename std::add_const<T>::type asConst(T &&t) noexcept { return std::move(t); }

// Prevent const rvalue arguments
template <typename T>
void asConst(const T &&) = delete;

namespace List
{
    // Replacement for the deprecated`QSet<T> QSet::fromList(const QList<T> &list)`
    template <typename T>
    QSet<T> toSet(const QList<T> &list)
    {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        return {list.cbegin(), list.cend()};
#else
        return QSet<T>::fromList(list);
#endif
    }
}

namespace Vector
{
    // Replacement for the deprecated `QVector<T> QVector::fromStdVector(const std::vector<T> &vector)`
    template <typename T>
    QVector<T> fromStdVector(const std::vector<T> &vector)
    {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        return {vector.cbegin(), vector.cend()};
#else
        return QVector<T>::fromStdVector(vector);
#endif
    }
}

#include <qmap.h>

enum E_App_Active_Window{
    E_Main_Window = 0,
    E_Add_New_Torrent_Dialog,
    E_Download_From_Url_Dialog,
    E_Options_Dialog,
};

typedef struct QVersionData_ {
    int iForce;
    QString strUrlLink;
} QVersionData;

typedef struct QTagLinkItem_ {
    QString fileName;
    QString fileSize;
    QString linkTxt;
} QTagLinkItem;

typedef struct QTagDownMsg_{
    qlonglong iMsgId = 0;
    qlonglong iProcessId = 0;
    int iLinkType = 0;
    QMap<QString, QString> reqHeaderMap;
    QVector<QTagLinkItem>  reqLinkList;
    QString reqMagnetLink;
} QTagDownMsg;

typedef struct QBrowserItem_ {
    int iIsInited;
    QString strAppDataLocal;

    QString strBrowserCompany;
    QString strBrowserExeName;
    QString curBrowserExeName;

    // path 
    QString strSecurePreferences;
    QString strPreferences;
    QString strExtensions;

    // regedit path
    QString strRegeditPath;

    // regedit version path
    QString strRegeditVersionPath;
} QBrowserItem;


#define  STR_EXTENSION_ID       "eapmjcdkdlenhkbanlgacimfibbbiinc"





#define  XDOWN_CRX_VERSION              "1.0.5"
#define  XDOWN_FIREFOX_VERSION          "1.0.6"
#define  XDOWN_FIREFOX_XID              "gatebyebye@outlook.com"


#define  XDOWN_TASK_MAX_RETRY           5


