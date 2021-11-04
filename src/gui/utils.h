/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Mike Tzou
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

#include <QSize>

class QIcon;
class QPixmap;
class QPoint;
class QWidget;

#include <string>

#define X_KB_SIZE (1024)
#define X_MB_SIZE (1024 * 1024)
#define X_GB_SIZE (1024 * 1024 * 1024 )

namespace Utils
{
    namespace Gui
    {
        void resize(QWidget *widget, const QSize &newSize = {});
        qreal screenScalingFactor(const QWidget *widget);

        template <typename T>
        T scaledSize(const QWidget *widget, const T &size)
        {
            return (size * screenScalingFactor(widget));
        }

        QPixmap scaledPixmap(const QIcon &icon, const QWidget *widget, int height);
        QPixmap scaledPixmap(const QString &path, const QWidget *widget, int height = 0);
        QPixmap scaledPixmapSvg(const QString &path, const QWidget *widget, int baseHeight);
        QSize smallIconSize(const QWidget *widget = nullptr);
        QSize mediumIconSize(const QWidget *widget = nullptr);
        QSize largeIconSize(const QWidget *widget = nullptr);

        QPoint screenCenter(const QWidget *w);

        void openPath(const QString &absolutePath);
        void openFolderSelect(const QString &absolutePath);
		
		
		
		
        void openFolderSelectFile(const QString &absolutePath, const QString &fileName);
        void openSelectFile(const QString &absolutePath, const QString &fileName);

        QString getExtByFileName(const QString &strFileName);
        QString getFileSizeByValue(const qlonglong &lFileSize);

        bool isDirExist(const QString &strFullPath);
        bool isFileExist(const QString &strFullName);

        QString replaceBadFileName(const QString &inputVal);

        QString getFileMd5(const QString &strFullName);
        QString getStringMd5(const QString &strText);
        QString getStringSha1(const QString &strText);

        QString OnEncodeBase64(const QString &strText);
        QString OnDecodeBase64(const QString &strText);


        QString getCurrentPath();
        QString getCurrentDateTime();
        QString getCurrentDateTimeFmt();

        uint getTimestamp();

        int getRandomVal(const int inputVal);
        QString getRandomString(int iMaxLen = 8);

    }
}
