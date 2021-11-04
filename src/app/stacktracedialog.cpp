/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015 The qBittorrent project
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
 *
 */

#include "stacktracedialog.h"

#include <QString>

#include "base/utils/misc.h"
#include "ui_stacktracedialog.h"
#include "base/global.h"
#include "base/xversion.h"

StacktraceDialog::StacktraceDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::StacktraceDialog)
{
    m_ui->setupUi(this);
}

StacktraceDialog::~StacktraceDialog()
{
    delete m_ui;
}

void StacktraceDialog::setStacktraceString(const QString &sigName, const QString &trace)
{
    // try to call Qt function as less as possible
    const QString htmlStr = QString(
        "<p align=center><b><font size=7 color=red>"
        "XDown has crashed"
        "</font></b></p>"
        "<font size=4><p>"
        "Please file a bug report at "
        "<a href=\"https://bugs.xdown.org\">https://bugs.xdown.org</a> "
        "and provide the following information:"
        "</p></font>"
        "<br/><hr><br/>"
        "<p align=center><font size=4>"
        "XDown version: " XDOWN_VERSION " (%1-bit)<br/>"
        "Libtorrent version: %2<br/>"
        "Qt version: " QT_VERSION_STR "<br/>"
        "Boost version: %3<br/>"
        "OpenSSL version: %4<br/>"
        "zlib version: %5<br/>"
        "OS version: %6<br/><br/>"
        "Caught signal: %7"
        "</font></p>"
        "<pre><code>%8</code></pre>"
        "<br/><hr><br/><br/>")
            .arg(QString::number(QT_POINTER_SIZE * 8)
                 , Utils::Misc::libtorrentVersionString()
                 , Utils::Misc::boostVersionString()
                 , Utils::Misc::opensslVersionString()
                 , Utils::Misc::zlibVersionString()
                 , Utils::Misc::osName()
                 , sigName
                 , trace);

    m_ui->errorText->setHtml(htmlStr);
}
