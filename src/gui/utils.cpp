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

#include "utils.h"

#ifdef Q_OS_WIN
#include <Objbase.h>
#include <Shlobj.h>
#endif

#include <QApplication>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>
#include <QPixmapCache>
#include <QPoint>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QStyle>
#include <QUrl>
#include <QWidget>
#include <QWindow>


#include <qdir.h>
#include <qdatetime.h>
#include <qcryptographichash.h>




#include "base/utils/fs.h"
#include "base/utils/version.h"

void Utils::Gui::resize(QWidget *widget, const QSize &newSize)
{
    if (newSize.isValid())
        widget->resize(newSize);
    else  // depends on screen DPI
        widget->resize(widget->size() * screenScalingFactor(widget));
}

qreal Utils::Gui::screenScalingFactor(const QWidget *widget)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    Q_UNUSED(widget);
    return 1;
#else
    if (!widget)
        return 1;

#ifdef Q_OS_WIN
    const int screenNumber = qApp->desktop()->screenNumber(widget);
    const QScreen *screen = QApplication::screens()[screenNumber];
    // Workaround for QScreen::physicalDotsPerInch() that could return
    // values that are smaller than the normal 96 DPI on Windows
    const qreal physicalDPI = qMax<qreal>(screen->physicalDotsPerInch(), 96);
    return (screen->logicalDotsPerInch() / physicalDPI);
#elif defined(Q_OS_MACOS)
    return 1;
#else
    return widget->devicePixelRatioF();
#endif // Q_OS_WIN
#endif // QT_VERSION
}

QPixmap Utils::Gui::scaledPixmap(const QIcon &icon, const QWidget *widget, const int height)
{
    Q_ASSERT(height > 0);
    const int scaledHeight = height * Utils::Gui::screenScalingFactor(widget);
    return icon.pixmap(scaledHeight);
}

QPixmap Utils::Gui::scaledPixmap(const QString &path, const QWidget *widget, const int height)
{
    const QPixmap pixmap(path);
    const int scaledHeight = ((height > 0) ? height : pixmap.height()) * Utils::Gui::screenScalingFactor(widget);
    return pixmap.scaledToHeight(scaledHeight, Qt::SmoothTransformation);
}

QPixmap Utils::Gui::scaledPixmapSvg(const QString &path, const QWidget *widget, const int baseHeight)
{
    const int scaledHeight = baseHeight * Utils::Gui::screenScalingFactor(widget);
    const QString normalizedKey = path + '@' + QString::number(scaledHeight);

    QPixmap pm;
    QPixmapCache cache;
    if (!cache.find(normalizedKey, &pm))
    {
        pm = QIcon(path).pixmap(scaledHeight);
        cache.insert(normalizedKey, pm);
    }
    return pm;
}

QSize Utils::Gui::smallIconSize(const QWidget *widget)
{
    // Get DPI scaled icon size (device-dependent), see QT source
    // under a 1080p screen is usually 16x16
    const int s = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, widget);
    return {s, s};
}

QSize Utils::Gui::mediumIconSize(const QWidget *widget)
{
    // under a 1080p screen is usually 24x24
    return ((smallIconSize(widget) + largeIconSize(widget)) / 2);
}

QSize Utils::Gui::largeIconSize(const QWidget *widget)
{
    // Get DPI scaled icon size (device-dependent), see QT source
    // under a 1080p screen is usually 32x32
    const int s = QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize, nullptr, widget);
    return {s, s};
}

QPoint Utils::Gui::screenCenter(const QWidget *w)
{
    // Returns the QPoint which the widget will be placed center on screen (where parent resides)

    if (!w)
        return {};

    QRect r = QGuiApplication::primaryScreen()->availableGeometry();
    const QPoint primaryScreenCenter {(r.x() + (r.width() - w->frameSize().width()) / 2), (r.y() + (r.height() - w->frameSize().height()) / 2)};

    const QWidget *parent = w->parentWidget();
    if (!parent)
        return primaryScreenCenter;

    const QWindow *window = parent->window()->windowHandle();
    if (!window)
        return primaryScreenCenter;

    const QScreen *screen = window->screen();
    if (!screen)
        return primaryScreenCenter;

    r = screen->availableGeometry();
    return {(r.x() + (r.width() - w->frameSize().width()) / 2), (r.y() + (r.height() - w->frameSize().height()) / 2)};
}

// Open the given path with an appropriate application
void Utils::Gui::openPath(const QString &absolutePath)
{
    const QString path = Utils::Fs::toUniformPath(absolutePath);
    // Hack to access samba shares with QDesktopServices::openUrl
    if (path.startsWith("//"))
        QDesktopServices::openUrl(Utils::Fs::toNativePath("file:" + path));
    else
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

// Open the parent directory of the given path with a file manager and select
// (if possible) the item at the given path
void Utils::Gui::openFolderSelect(const QString &absolutePath)
{
    QString path {Utils::Fs::toUniformPath(absolutePath)};
    const QFileInfo pathInfo {path};
    // If the item to select doesn't exist, try to open its parent
    if (!pathInfo.exists(path))
    {
        openPath(path.left(path.lastIndexOf('/')));
        return;
    }

#ifdef Q_OS_WIN
    HRESULT hresult = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    PIDLIST_ABSOLUTE pidl = ::ILCreateFromPathW(reinterpret_cast<PCTSTR>(Utils::Fs::toNativePath(path).utf16()));
    if (pidl)
    {
        ::SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        ::ILFree(pidl);
    }
    if ((hresult == S_OK) || (hresult == S_FALSE))
        ::CoUninitialize();
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QProcess proc;
    proc.start("xdg-mime", {"query", "default", "inode/directory"});
    proc.waitForFinished();
    const QString output = proc.readLine().simplified();
    if ((output == "dolphin.desktop") || (output == "org.kde.dolphin.desktop"))
    {
        proc.startDetached("dolphin", {"--select", Utils::Fs::toNativePath(path)});
    }
    else if ((output == "nautilus.desktop") || (output == "org.gnome.Nautilus.desktop")
                 || (output == "nautilus-folder-handler.desktop"))
                 {
        if (pathInfo.isDir())
            path = path.left(path.lastIndexOf('/'));
        proc.start("nautilus", {"--version"});
        proc.waitForFinished();
        const QString nautilusVerStr = QString(proc.readLine()).remove(QRegularExpression("[^0-9.]"));
        using NautilusVersion = Utils::Version<int, 3>;
        if (NautilusVersion::tryParse(nautilusVerStr, {1, 0, 0}) > NautilusVersion {3, 28})
            proc.startDetached("nautilus", {Utils::Fs::toNativePath(path)});
        else
            proc.startDetached("nautilus", {"--no-desktop", Utils::Fs::toNativePath(path)});
    }
    else if (output == "nemo.desktop")
    {
        if (pathInfo.isDir())
            path = path.left(path.lastIndexOf('/'));
        proc.startDetached("nemo", {"--no-desktop", Utils::Fs::toNativePath(path)});
    }
    else if ((output == "konqueror.desktop") || (output == "kfmclient_dir.desktop")) {
        proc.startDetached("konqueror", {"--select", Utils::Fs::toNativePath(path)});
    }
    else {
        // "caja" manager can't pinpoint the file, see: https://github.com/qbittorrent/qBittorrent/issues/5003
        openPath(path.left(path.lastIndexOf('/')));
    }
#else
    openPath(path.left(path.lastIndexOf('/')));
#endif
}






#ifdef Q_OS_WIN
#include <Windows.h>
#include <shellapi.h>
#endif

// ���ļ��У���ѡ���ļ�
void Utils::Gui::openFolderSelectFile(const QString &absolutePath, const QString &fileName)
{
    const QString path = Utils::Fs::toUniformPath(absolutePath);
    // If the item to select doesn't exist, try to open its parent
    if (!QFileInfo::exists(path)) {
        openPath(path.left(path.lastIndexOf('/')));
        return;
    }
#ifdef Q_OS_WIN
    QString fullFileName;
    fullFileName = absolutePath + "//" + fileName;
    fullFileName = fullFileName.replace("//", "/");
    fullFileName = fullFileName.replace("//", "/");
    fullFileName = fullFileName.replace("//", "/");
    fullFileName = fullFileName.replace("/", "\\");
    if (QFileInfo::exists(fullFileName) && fileName.length() > 0 ) {
        QString strParam = "/select, " + fullFileName;
        PCTSTR pTmp = reinterpret_cast<PCTSTR>(strParam.utf16());
        ShellExecute(NULL, L"open", L"explorer.exe",pTmp, NULL, SW_SHOWNORMAL);
    }
    else {
        ShellExecute(NULL, L"open", L"explorer.exe", reinterpret_cast<PCTSTR>(Utils::Fs::toNativePath(path).utf16()), NULL, SW_SHOWNORMAL);
    }
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QProcess proc;
    proc.start("xdg-mime", { "query", "default", "inode/directory" });
    proc.waitForFinished();
    const QString output = proc.readLine().simplified();
    if ((output == "dolphin.desktop") || (output == "org.kde.dolphin.desktop")) {
        proc.startDetached("dolphin", { "--select", Utils::Fs::toNativePath(path) });
    }
    else
    {
        // "caja" manager can't pinpoint the file, see: https://github.com/qbittorrent/qBittorrent/issues/5003
        openPath(path.left(path.lastIndexOf('/')));
    }
#else
    openPath(path.left(path.lastIndexOf('/')));
#endif
}



void Utils::Gui::openSelectFile(const QString &absolutePath, const QString &fileName)
{
    QString strFullPath;
    strFullPath = QString("%1/%2").arg(absolutePath, fileName);
    if (isDirExist(strFullPath)) {
        openPath(strFullPath);
    }
    else if (isFileExist(strFullPath)) {
#ifdef Q_OS_WIN
        PCTSTR pDiskDir = reinterpret_cast<PCTSTR>(absolutePath.utf16());
        PCTSTR pDiskFileName = reinterpret_cast<PCTSTR>(fileName.utf16());
        int nRes = (int)ShellExecute(NULL, L"open", pDiskFileName, L"", pDiskDir, SW_SHOWDEFAULT);
        if (SE_ERR_NOASSOC == nRes)
        {
            QString strExecCmd = QString("rundll32 shell32, OpenAs_RunDLL  %1").arg(fileName);
            PCSTR pStrExecCmd = reinterpret_cast<PCSTR>(strExecCmd.utf16());
            WinExec(pStrExecCmd, SW_SHOWNORMAL);
        }
#endif
    }

}

QString Utils::Gui::getExtByFileName(const QString &strFileName) {
    if (strFileName.trimmed().length() < 1) {
        return "";
    }
    QString strTmp(strFileName.trimmed());
    int iPosValue = strTmp.lastIndexOf(".");
    if (iPosValue == -1) {
        return "";
    }
    return strTmp.mid(iPosValue + 1);
}


QString Utils::Gui::getFileSizeByValue(const qlonglong &fileSize) {
    if (fileSize <= 0) {
        return "-";
    }
    QString strFmtSize = "";
    if (fileSize < X_KB_SIZE) {
        strFmtSize = QString("%d B").arg(fileSize);
    }
    else if (fileSize < X_MB_SIZE)
    {
        strFmtSize = QString("%0.2 KB").arg((double)(fileSize / X_KB_SIZE));
    }
    else if (fileSize < X_GB_SIZE)
    {
        strFmtSize = QString("%0.2 MB").arg((double)(fileSize / X_MB_SIZE));
    }
    else
    {
        strFmtSize = QString("%0.2 GB").arg((double)(fileSize / X_GB_SIZE));
    }
    return strFmtSize;
}


bool Utils::Gui::isDirExist(const QString &strFullPath)
{
    QDir dir(strFullPath);
    if (dir.exists())
    {
        return true;
    }
    return false;
}

bool Utils::Gui::isFileExist(const QString &strFullName)
{
    if (QFile::exists(strFullName)) {
        return true;
    }
    return false;
}

QString Utils::Gui::replaceBadFileName(const QString &inputVal)
{
    QString strTmpVal(inputVal);
    strTmpVal = strTmpVal.replace("\\", "_");
    strTmpVal = strTmpVal.replace("/", "_");
    strTmpVal = strTmpVal.replace("||", "_");
    strTmpVal = strTmpVal.replace(":", "-");
    strTmpVal = strTmpVal.replace("?", "-");
    strTmpVal = strTmpVal.replace("'", "-");
    strTmpVal = strTmpVal.replace("<", "-");
    strTmpVal = strTmpVal.replace(">", "-");
    strTmpVal = strTmpVal.replace("*", "-");
    strTmpVal = strTmpVal.replace(" ", "-");
    return strTmpVal;
}

QString Utils::Gui::getCurrentDateTime()
{
    QDateTime current_date_time = QDateTime::currentDateTime();
    QString current_date = current_date_time.toString("yyyy-MM-dd");
    QString current_time = current_date_time.toString("hh:mm:ss.zzz");
    return QString("%1 %2").arg(current_date, current_time);
}

QString Utils::Gui::getCurrentDateTimeFmt()
{
    QDateTime current_date_time = QDateTime::currentDateTime();
    QString current_date = current_date_time.toString("yyyyMMdd");
    QString current_time = current_date_time.toString("hhmmsszzz");
    return QString("%1_%2").arg(current_date, current_time);
}

QString Utils::Gui::getFileMd5(const QString &strFullName) {
    if (isFileExist(strFullName)) {
        QFile theFile(strFullName);
        theFile.open(QIODevice::ReadOnly);
        QByteArray ba = QCryptographicHash::hash(theFile.readAll(), QCryptographicHash::Md5);
        theFile.close();
        QString tmpStr(ba.toHex());
        return tmpStr;
    }
    else {
        return "";
    }
}

QString Utils::Gui::getStringMd5(const QString &strText)
{
    QString md5;
    QByteArray bRetArray;
    QCryptographicHash md(QCryptographicHash::Md5);
    md.addData(strText.toUtf8());
    bRetArray = md.result();
    md5.append(bRetArray.toHex());
    return md5;
}

QString Utils::Gui::getStringSha1(const QString &strText)
{
    QString md5;
    QByteArray bRetArray;
    QCryptographicHash md(QCryptographicHash::Sha1);
    md.addData(strText.toUtf8());
    bRetArray = md.result();
    md5.append(bRetArray.toHex());
    return md5;
}

// ��׼base64
QString Utils::Gui::OnEncodeBase64(const QString &strText)
{
    QByteArray text(strText.toUtf8());
    return QString(text.toBase64());
}
QString Utils::Gui::OnDecodeBase64(const QString &strText)
{
    return QString(QByteArray::fromBase64(strText.toUtf8()));
}


QString Utils::Gui::getCurrentPath()
{
    QString curExePath = QApplication::applicationFilePath();
    int iPosValue = curExePath.lastIndexOf("/");
    if (iPosValue == -1) {
        iPosValue = curExePath.lastIndexOf("\\");
    }
    return curExePath.mid(0, iPosValue);
}


uint Utils::Gui::getTimestamp()
{
    QDateTime time = QDateTime::currentDateTime();
    return time.toTime_t();
}

int Utils::Gui::getRandomVal(const int inputVal)
{
    QTime time;
    time = QTime::currentTime();
    qsrand(time.msec() + time.second() * 1000);
    int nValue = qrand() % inputVal;
    return nValue;
}

QString Utils::Gui::getRandomString(int iMaxLen)
{
    QString randString;
    QString tmp = QString("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWZYZ");
    QString str;
    QTime t;
    t = QTime::currentTime();
    qsrand(t.msec() + t.second() * 1000);
    for (int i = 0; i < iMaxLen; i++)
    {
        int len = qrand() % tmp.length();
        str[i] = tmp.at(len);
    }
    randString = str;
    return randString;
}

