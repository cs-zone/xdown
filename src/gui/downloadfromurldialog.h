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

#pragma once

#include <QDialog>


#include <qstandarditemmodel.h>
#include <qlineedit>
#include <qcombobox>
#include "base/global.h"

#include "base/settingvalue.h"

namespace Ui
{
    class DownloadFromURLDialog;
}

class DownloadFromURLDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(DownloadFromURLDialog)

public:
    explicit DownloadFromURLDialog(QWidget *parent, const QTagDownMsg& xDownMsg);
    ~DownloadFromURLDialog();

    void downloadFailedMessage();

signals:
    void urlsReadyToBeDownloaded(const QStringList &torrentURLs,
        const QMap<QString,QString> &headerMap,
        const QMap<QString, QString> &optionMap,
        const QHash<QString, QString> &urlToFileNameMap);

private slots:
    void selectFolderButtonClicked();
    void downloadButtonClicked();
    void downloadDialogReject();
    void textUrlsChanged();

    void addPushButtonClicked();
    void delPushButton1Clicked();
    void delPushButton2Clicked();
    void delPushButton3Clicked();
    void delPushButton4Clicked();
    void delPushButton5Clicked();
    void delPushButton6Clicked();
    void delPushButton7Clicked();

    void closeEvent(QCloseEvent *) override;

private:
    Ui::DownloadFromURLDialog *m_ui;

    QStandardItemModel *m_fileListModel;

    int m_iMaxIndex;
    int m_iHeaders;
    int m_iCurHeaderIndex;

    qlonglong m_iMsgId;
    qlonglong m_iProcessId;

    CachedSettingValue<QSize> m_storeDialogSize;
    CachedSettingValue<int>   m_columnWidth0;
    CachedSettingValue<int>   m_columnWidth1;
    CachedSettingValue<int>   m_columnWidth2;

    void delPushButtonClick(int iValue);

    void copyCtrlValue(int iIndex);
    void copyEditValue(QLineEdit *dstCtrl, QLineEdit *srcCtrl);
    void copyComboxValue(QComboBox *dstCtrl, QComboBox *srcCtrl);

    void OnInitFileList(const QTagDownMsg& xDownMsg);
    void SetHeaderValue(int iIndex, const QString &strHeadKey, const QString &strHeadValue);

    void loadState();
    void saveState();
};
