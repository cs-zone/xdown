/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "aboutdialog.h"

#include <QFile>

#include "base/unicodestrings.h"
#include "base/utils/misc.h"
#include "ui_aboutdialog.h"
#include "uithememanager.h"
#include "utils.h"



#include "base/global.h"
#include "base/xversion.h"


#define SETTINGS_KEY(name) "AboutDialog/" name

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , m_storeDialogSize(SETTINGS_KEY("DialogSize"))
    , m_ui(new Ui::AboutDialog)
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    setWindowTitle("XDown");

    // Title
    m_ui->labelName->setText(QString::fromLatin1("<b><h2>XDown " XDOWN_VERSION "</h2></b>"));

    m_ui->logo->setPixmap(Utils::Gui::scaledPixmapSvg(UIThemeManager::instance()->getIconPath(QLatin1String("xdown32")), this, 32));

    // About
    const QString aboutText = QString(
        "<p style=\"white-space: pre-wrap;\">"
        "%1\n\n"
        "<table>"
        "<tr><td>%2</td><td><a href=\"https://xdown.org\">https://xdown.org</a></td></tr>"
        "</table>"
        "</p>")
        .arg(tr("Http/BitTorrent download tool")
            , tr("Home Page:"));
    m_ui->labelAbout->setText(aboutText);

    m_ui->labelMascot->setPixmap(Utils::Gui::scaledPixmap(":/icons/mascot.png", this));

    // Thanks
    QFile thanksfile(":/thanks.html");
    if (thanksfile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_ui->textBrowserThanks->setHtml(QString::fromUtf8(thanksfile.readAll().constData()));
        thanksfile.close();
    }

    // Translation
    QFile translatorsfile(":/translators.html");
    if (translatorsfile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_ui->textBrowserTranslation->setHtml(QString::fromUtf8(translatorsfile.readAll().constData()));
        translatorsfile.close();
    }

    // License
    QFile licensefile(":/gpl.html");
    if (licensefile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_ui->textBrowserLicense->setHtml(QString::fromUtf8(licensefile.readAll().constData()));
        licensefile.close();
    }

    // Software Used
    m_ui->labelQtVer->setText(QT_VERSION_STR);
    m_ui->labelLibtVer->setText(Utils::Misc::libtorrentVersionString());
    m_ui->labelBoostVer->setText(Utils::Misc::boostVersionString());
    m_ui->labelOpensslVer->setText(Utils::Misc::opensslVersionString());
    m_ui->labelZlibVer->setText(Utils::Misc::zlibVersionString());
    m_ui->labelQBitTorrentVer->setText(QBT_VERSION);

    const QString DBIPText = QString(
                                 "<html><head/><body><p>"
                                 "%1"
                                 " (<a href=\"https://db-ip.com/\">https://db-ip.com/</a>)</p></body></html>")
                             .arg(tr("The free IP to Country Lite database by DB-IP is used for resolving the countries of peers. "
                                     "The database is licensed under the Creative Commons Attribution 4.0 International License"));
    m_ui->labelDBIP->setText(DBIPText);

    //Utils::Gui::resize(this);
    loadState();
    show();
}

AboutDialog::~AboutDialog()
{
    saveState();
    delete m_ui;
}


void AboutDialog::loadState()
{
    Utils::Gui::resize(this, m_storeDialogSize);
}

void AboutDialog::saveState()
{
    m_storeDialogSize = size();
}
