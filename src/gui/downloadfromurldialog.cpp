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

#include "downloadfromurldialog.h"

#include <QClipboard>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

#include "ui_downloadfromurldialog.h"
#include "utils.h"


#include "base/bittorrent/xdownhandleimpl.h"
#include "downloaditemdelegate.h"

#include "base/bittorrent/session.h"
#include <qfiledialog.h>
#include "base/global.h"
#include "gui/utils.h"

extern int g_iActiveWin;

namespace
{
    bool isDownloadable(const QString &str)
    {
        return (str.startsWith("http://", Qt::CaseInsensitive)
            || str.startsWith("https://", Qt::CaseInsensitive)
            || str.startsWith("ftp://", Qt::CaseInsensitive)
            || str.startsWith("magnet:", Qt::CaseInsensitive)
            || ((str.size() == 40) && !str.contains(QRegularExpression("[^0-9A-Fa-f]")))
            || ((str.size() == 32) && !str.contains(QRegularExpression("[^2-7A-Za-z]"))));
    }

    bool isXDownable(const QString &str) {
        return !str.endsWith(".torrent") && (str.startsWith("http://") || str.startsWith("https://") || str.startsWith("ftp://"));
    }

    
}

#define SETTINGS_KEY(name) "DownloadFromUrlDialog/" name
DownloadFromURLDialog::DownloadFromURLDialog(QWidget *parent, const QTagDownMsg& xDownMsg)
    : QDialog(parent)
    , m_storeDialogSize(SETTINGS_KEY("DialogSize"))
    , m_columnWidth0(SETTINGS_KEY("ColumnWdith0"))
    , m_columnWidth1(SETTINGS_KEY("ColumnWdith1"))
    , m_columnWidth2(SETTINGS_KEY("ColumnWdith2"))
    , m_ui(new Ui::DownloadFromURLDialog)
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    g_iActiveWin = (int)E_Download_From_Url_Dialog;

    setModal(true);

    m_iMsgId = 0;
    m_iProcessId = 0;

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Download"));
    m_ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &DownloadFromURLDialog::downloadButtonClicked);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &DownloadFromURLDialog::downloadDialogReject);

    m_ui->textUrls->setWordWrapMode(QTextOption::NoWrap);

    BitTorrent::Session *session = BitTorrent::Session::instance();
    // XDown Add Control
    {

        m_ui->Concurrent->setText(tr("Concurrent"));
        m_ui->enableRRcheckBox->setText(tr("Enable RR"));
        m_ui->tabWidget->setTabText(0, tr("File List"));
        m_ui->tabWidget->setTabText(1, tr("Http Header"));
        m_ui->label->setText(tr("Save Path"));

        int iMaxConcurrent = session->getHttpMaxConcurrent();
        if (iMaxConcurrent > 0) {
            m_ui->concurrentSpinBox->setValue(iMaxConcurrent);
            bool bValue = session->getHttpEnableRR();
            m_ui->enableRRcheckBox->setChecked(bValue);
        }
        else {
            m_ui->concurrentSpinBox->setValue(16);
        }

        const QString defSavePath{ BitTorrent::Session::instance()->defaultSavePath() };
        m_ui->savePathEdit->setText(defSavePath);

       
        m_iMaxIndex = 7;
        
        m_iHeaders = 1;
        
        m_iCurHeaderIndex = 0;

        m_fileListModel = new QStandardItemModel;
        m_ui->fileListTableView->setModel(m_fileListModel);
        m_fileListModel->setColumnCount(3);
        m_fileListModel->setHorizontalHeaderItem(0, new QStandardItem(tr("File Name")));
        m_fileListModel->setHorizontalHeaderItem(1, new QStandardItem(tr("File Type")));
        m_fileListModel->setHorizontalHeaderItem(2, new QStandardItem(tr("File Size")));

        if (m_columnWidth0 <= 0
            || m_columnWidth1 <= 0
            || m_columnWidth2 <= 0) {
            m_ui->fileListTableView->setColumnWidth(0, 400);
            m_ui->fileListTableView->setColumnWidth(1, 100);
            m_ui->fileListTableView->setColumnWidth(2, 100);
        }

        m_ui->fileListTableView->horizontalHeader()->setStyleSheet("QHeaderView::section {"
            "color: black;padding-left: 4px;border: 0px solid #6c6c6c;}");

        m_ui->fileListTableView->setStyleSheet("QTableView::item:selected{background-color: black; padding-left: 4px;border: 0px solid #6c6c6c;};");

        m_ui->fileListTableView->verticalHeader()->hide();
        m_ui->fileListTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_ui->fileListTableView->setItemDelegate(new DefaultItemDelegate);
    }

    QStringList headList = { QObject::tr("User-Agent"),
        QObject::tr("Cookie"),
        QObject::tr("Referer"),
        QObject::tr("Accept"),
        QObject::tr("Accept-Encoding"),
        QObject::tr("Accept-Language") };
    {
        int idx = 0;
        for (idx = 0; idx < headList.size(); idx++) {
            m_ui->headerCombox1->addItem(headList.at(idx));
            m_ui->headerCombox2->addItem(headList.at(idx));
            m_ui->headerCombox3->addItem(headList.at(idx));
            m_ui->headerCombox4->addItem(headList.at(idx));
            m_ui->headerCombox5->addItem(headList.at(idx));
            m_ui->headerCombox6->addItem(headList.at(idx));
            m_ui->headerCombox7->addItem(headList.at(idx));
        }

        m_ui->headerCombox1->setCurrentIndex(0);
        m_ui->headerCombox2->setCurrentIndex(1);
        m_ui->headerCombox3->setCurrentIndex(2);
        m_ui->headerCombox4->setCurrentIndex(3);
        m_ui->headerCombox5->setCurrentIndex(4);
        m_ui->headerCombox6->setCurrentIndex(5);
        m_ui->headerCombox7->setCurrentText(tr(""));

#if 1
        m_ui->headerCombox2->setVisible(false);
        m_ui->headerCombox3->setVisible(false);
        m_ui->headerCombox4->setVisible(false);
        m_ui->headerCombox5->setVisible(false);
        m_ui->headerCombox6->setVisible(false);
        m_ui->headerCombox7->setVisible(false);

        m_ui->headerEdit2->setVisible(false);
        m_ui->headerEdit3->setVisible(false);
        m_ui->headerEdit4->setVisible(false);
        m_ui->headerEdit5->setVisible(false);
        m_ui->headerEdit6->setVisible(false);
        m_ui->headerEdit7->setVisible(false);

        m_ui->delPushButton2->setVisible(false);
        m_ui->delPushButton3->setVisible(false);
        m_ui->delPushButton4->setVisible(false);
        m_ui->delPushButton5->setVisible(false);
        m_ui->delPushButton6->setVisible(false);
        m_ui->delPushButton7->setVisible(false);
#endif
    }

    bool bFindUserAgent = false;
    bool bFromXDownMsg = false;
    {

        if (xDownMsg.iLinkType > 0)
        {

            m_iMsgId = xDownMsg.iMsgId;
            m_iProcessId = xDownMsg.iProcessId;

            if (xDownMsg.iLinkType == 1) {
                //
                QString strTmp;
                for (int idx = 0; idx < xDownMsg.reqLinkList.size(); idx++) {
                    strTmp.append(xDownMsg.reqLinkList[idx].linkTxt);
                    strTmp.append("\r\n");
                }
                m_ui->textUrls->setText(strTmp);
                int idx = 0;
                
                for(auto iter = xDownMsg.reqHeaderMap.begin(); iter != xDownMsg.reqHeaderMap.end(); iter++) {
                    QString strHeadKey = iter.key();
                    QString strHeadValue = iter.value();
                    if (idx > 0) {
                        addPushButtonClicked();
                    }
                    if (strHeadKey.toLower() == "user-agent") {
                        bFindUserAgent = true;
                    }
                    SetHeaderValue(idx + 1, strHeadKey, strHeadValue);
                    ++idx;
                }
                OnInitFileList(xDownMsg);
                bFromXDownMsg = true;
            }
            else if (xDownMsg.iLinkType == 2) {
                //
                m_ui->textUrls->setText(xDownMsg.reqMagnetLink);
                bFromXDownMsg = true;
            }
        }

        if (!bFindUserAgent) {
            if (m_iHeaders < 7) {
                if (bFromXDownMsg) {
                    addPushButtonClicked();
                }
                QString strUserAgent = session->getHttpUserAgent().trimmed();
                if (strUserAgent.trimmed().length() > 0) {
                    SetHeaderValue(m_iHeaders, "User-Agent", strUserAgent);
                }
            }
        }

        connect(m_ui->textUrls, &QTextEdit::textChanged, this, &DownloadFromURLDialog::textUrlsChanged);
        connect(m_ui->selectFolderButton, &QPushButton::clicked, this, &DownloadFromURLDialog::selectFolderButtonClicked);

        connect(m_ui->addPushButton, &QPushButton::clicked, this, &DownloadFromURLDialog::addPushButtonClicked);
        connect(m_ui->delPushButton1, &QPushButton::clicked, this, &DownloadFromURLDialog::delPushButton1Clicked);
        connect(m_ui->delPushButton2, &QPushButton::clicked, this, &DownloadFromURLDialog::delPushButton2Clicked);
        connect(m_ui->delPushButton3, &QPushButton::clicked, this, &DownloadFromURLDialog::delPushButton3Clicked);
        connect(m_ui->delPushButton4, &QPushButton::clicked, this, &DownloadFromURLDialog::delPushButton4Clicked);
        connect(m_ui->delPushButton5, &QPushButton::clicked, this, &DownloadFromURLDialog::delPushButton5Clicked);
        connect(m_ui->delPushButton6, &QPushButton::clicked, this, &DownloadFromURLDialog::delPushButton6Clicked);
        connect(m_ui->delPushButton7, &QPushButton::clicked, this, &DownloadFromURLDialog::delPushButton7Clicked);
    }

    if (!bFromXDownMsg) {
        // Paste clipboard if there is an URL in it
        const QString clipboardText = qApp->clipboard()->text();
        const QVector<QStringRef> clipboardList = clipboardText.splitRef('\n');

        QSet<QString> uniqueURLs;
        for (QStringRef strRef : clipboardList) {
            strRef = strRef.trimmed();
            if (strRef.isEmpty()) continue;

            const QString str = strRef.toString();
            if (isDownloadable(str))
                uniqueURLs << str;
        }
        m_ui->textUrls->setText(uniqueURLs.values().join('\n'));
    }
    
    //Utils::Gui::resize(this);
    loadState();
    show();
}

DownloadFromURLDialog::~DownloadFromURLDialog()
{
    saveState();
    g_iActiveWin = (int)E_Main_Window;
    delete m_ui;
}


void DownloadFromURLDialog::loadState()
{
    Utils::Gui::resize(this, m_storeDialogSize);
    if(m_columnWidth0 > 0) {
        m_ui->fileListTableView->setColumnWidth(0, m_columnWidth0);
    }
    if (m_columnWidth1 > 0) {
        m_ui->fileListTableView->setColumnWidth(1, m_columnWidth1);
    }
    if (m_columnWidth2 > 0) {
        m_ui->fileListTableView->setColumnWidth(2, m_columnWidth2);
    }
}

void DownloadFromURLDialog::saveState()
{
    m_storeDialogSize = size();

    int iMaxIndex = m_fileListModel->rowCount();

    m_columnWidth0 = m_ui->fileListTableView->columnWidth(0);
    m_columnWidth1 = m_ui->fileListTableView->columnWidth(1);
    m_columnWidth2 = m_ui->fileListTableView->columnWidth(2);
}

void DownloadFromURLDialog::SetHeaderValue(int iIndex, const QString &strHeadKey, const QString &strHeadValue)
{
    switch (iIndex)
    {
    case 1:
        m_ui->headerCombox1->setCurrentText(strHeadKey);
        m_ui->headerEdit1->setText(strHeadValue);
        break;
    case 2:
        m_ui->headerCombox2->setCurrentText(strHeadKey);
        m_ui->headerEdit2->setText(strHeadValue);
        break;
    case 3:
        m_ui->headerCombox3->setCurrentText(strHeadKey);
        m_ui->headerEdit3->setText(strHeadValue);
        break;
    case 4:
        m_ui->headerCombox4->setCurrentText(strHeadKey);
        m_ui->headerEdit4->setText(strHeadValue);
        break;
    case 5:
        m_ui->headerCombox5->setCurrentText(strHeadKey);
        m_ui->headerEdit5->setText(strHeadValue);
        break;
    case 6:
        m_ui->headerCombox6->setCurrentText(strHeadKey);
        m_ui->headerEdit6->setText(strHeadValue);
        break;
    case 7:
        m_ui->headerCombox7->setCurrentText(strHeadKey);
        m_ui->headerEdit7->setText(strHeadValue);
        break;
    default:
        break;
    }
}

void DownloadFromURLDialog::selectFolderButtonClicked()
{
    QString beforePath = m_ui->savePathEdit->text();
    QString currDirectory = QFileInfo(beforePath).absoluteDir().absolutePath();
    QString directory = currDirectory.isEmpty() ? QDir::homePath() : currDirectory;
    QString strFileFolderTitle = tr("Choose a folder");
    QString srrSelectFolder = QFileDialog::getExistingDirectory(this, strFileFolderTitle, directory, QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly);
    if (srrSelectFolder.length() > 0) {
        m_ui->savePathEdit->setText(srrSelectFolder);
    }
}

void DownloadFromURLDialog::delPushButton1Clicked()
{
    delPushButtonClick(1);
}

void DownloadFromURLDialog::delPushButton2Clicked()
{
    delPushButtonClick(2);
}

void DownloadFromURLDialog::delPushButton3Clicked()
{
    delPushButtonClick(3);
}

void DownloadFromURLDialog::delPushButton4Clicked()
{
    delPushButtonClick(4);
}

void DownloadFromURLDialog::delPushButton5Clicked()
{
    delPushButtonClick(5);
}

void DownloadFromURLDialog::delPushButton6Clicked()
{
    delPushButtonClick(6);
}

void DownloadFromURLDialog::delPushButton7Clicked()
{
    delPushButtonClick(7);
}


void DownloadFromURLDialog::addPushButtonClicked()
{
    if (m_iHeaders >= 7) {
        return;
    }
    switch (m_iHeaders)
    {
    case 1:
        m_ui->headerCombox2->setVisible(true);
        m_ui->headerEdit2->setVisible(true);
        m_ui->headerEdit2->setText(tr(""));
        m_ui->delPushButton2->setVisible(true);
        break;
    case 2:
        m_ui->headerCombox3->setVisible(true);
        m_ui->headerEdit3->setVisible(true);
        m_ui->headerEdit3->setText(tr(""));
        m_ui->delPushButton3->setVisible(true);
        break;
    case 3:
        m_ui->headerCombox4->setVisible(true);
        m_ui->headerEdit4->setVisible(true);
        m_ui->headerEdit4->setText(tr(""));
        m_ui->delPushButton4->setVisible(true);
        break;
    case 4:
        m_ui->headerCombox5->setVisible(true);
        m_ui->headerEdit5->setVisible(true);
        m_ui->headerEdit5->setText(tr(""));
        m_ui->delPushButton5->setVisible(true);
        break;
    case 5:
        m_ui->headerCombox6->setVisible(true);
        m_ui->headerEdit6->setVisible(true);
        m_ui->headerEdit6->setText(tr(""));
        m_ui->delPushButton6->setVisible(true);
        break;
    case 6:
        m_ui->headerCombox7->setVisible(true);
        m_ui->headerEdit7->setVisible(true);
        m_ui->headerEdit7->setText(tr(""));
        m_ui->delPushButton7->setVisible(true);
        break;
    default:
        break;
    }
    ++m_iHeaders;
    qDebug("===add m_iHeaders=== %d", m_iHeaders);
}


void DownloadFromURLDialog::delPushButtonClick(int iValue)
{
    if (m_iHeaders == 1) {
        m_ui->headerCombox1->setCurrentIndex(0);
        m_ui->headerEdit1->setText(tr(""));
        return;
    }
#if 1
    switch (m_iHeaders)
    {
    case 2:
        m_ui->headerCombox2->setVisible(false);
        m_ui->headerEdit2->setVisible(false);
        m_ui->delPushButton2->setVisible(false);
        break;
    case 3:
        m_ui->headerCombox3->setVisible(false);
        m_ui->headerEdit3->setVisible(false);
        m_ui->delPushButton3->setVisible(false);
        break;
    case 4:
        m_ui->headerCombox4->setVisible(false);
        m_ui->headerEdit4->setVisible(false);
        m_ui->delPushButton4->setVisible(false);
        break;
    case 5:
        m_ui->headerCombox5->setVisible(false);
        m_ui->headerEdit5->setVisible(false);
        m_ui->delPushButton5->setVisible(false);
        break;
    case 6:
        m_ui->headerCombox6->setVisible(false);
        m_ui->headerEdit6->setVisible(false);
        m_ui->delPushButton6->setVisible(false);
        break;
    case 7:
        m_ui->headerCombox7->setVisible(false);
        m_ui->headerEdit7->setVisible(false);
        m_ui->delPushButton7->setVisible(false);
        break;
    default:
        break;
    }
#endif
    
    copyCtrlValue(iValue);

    --m_iHeaders;
    qDebug("===del m_iHeaders=== %d", m_iHeaders);
}

void DownloadFromURLDialog::copyCtrlValue(int iIndex)
{

    for (int idx = iIndex; idx <= m_iHeaders; idx++) {
        if (idx == 1) {
            copyComboxValue(m_ui->headerCombox1, m_ui->headerCombox2);
            copyEditValue(m_ui->headerEdit1, m_ui->headerEdit2);
        }

        if (idx == 2) {
            copyComboxValue(m_ui->headerCombox2, m_ui->headerCombox3);
            copyEditValue(m_ui->headerEdit2, m_ui->headerEdit3);
        }

        if (idx == 3) {
            copyComboxValue(m_ui->headerCombox3, m_ui->headerCombox4);
            copyEditValue(m_ui->headerEdit3, m_ui->headerEdit4);
        }

        if (idx == 4) {
            copyComboxValue(m_ui->headerCombox4, m_ui->headerCombox5);
            copyEditValue(m_ui->headerEdit4, m_ui->headerEdit5);
        }

        if (idx == 5) {
            copyComboxValue(m_ui->headerCombox5, m_ui->headerCombox6);
            copyEditValue(m_ui->headerEdit5, m_ui->headerEdit6);
        }

        if (idx == 6) {
            copyComboxValue(m_ui->headerCombox6, m_ui->headerCombox7);
            copyEditValue(m_ui->headerEdit6, m_ui->headerEdit7);
        }
    }
}

void DownloadFromURLDialog::copyComboxValue(QComboBox *dstCtrl, QComboBox *srcCtrl)
{
    dstCtrl->setCurrentText(srcCtrl->currentText());
}

void DownloadFromURLDialog::copyEditValue(QLineEdit *dstCtrl, QLineEdit *srcCtrl)
{
    dstCtrl->setText(srcCtrl->text());
}

void DownloadFromURLDialog::downloadFailedMessage()
{
    if (m_iMsgId > 0 && m_iProcessId > 0) {
        BitTorrent::Session *session = BitTorrent::Session::instance();
        session->sendFailedMessageToHosts(m_iMsgId, m_iProcessId);
    }
}

void DownloadFromURLDialog::downloadDialogReject()
{
    downloadFailedMessage();
    reject();
}

void DownloadFromURLDialog::closeEvent(QCloseEvent *e)
{
    downloadFailedMessage();
    qDebug("===DownloadFromURLDialog closeEvent");
}


void DownloadFromURLDialog::downloadButtonClicked()
{
    const QString plainText = m_ui->textUrls->toPlainText();
    const QVector<QStringRef> urls = plainText.splitRef('\n');

    // http header
    QMap<QString, QString> headerMap;
    QMap<QString, QString> optionMap;
    QString strSavePath = m_ui->savePathEdit->text().trimmed();
    {
        // 
        {
            QString strKey = m_ui->headerCombox1->currentText().trimmed();
            QString strValue = m_ui->headerEdit1->text().trimmed();
            if(strKey.length() > 0 && strValue.length() > 0){
                headerMap.insert(strKey, strValue);
            }
        }
        
        {
            QString strKey = m_ui->headerCombox2->currentText().trimmed();
            QString strValue = m_ui->headerEdit2->text().trimmed();
            if (strKey.length() > 0 && strValue.length() > 0) {
                headerMap.insert(strKey, strValue);
            }
        }
        
        {
            QString strKey = m_ui->headerCombox3->currentText().trimmed();
            QString strValue = m_ui->headerEdit3->text().trimmed();
            if (strKey.length() > 0 && strValue.length() > 0) {
                headerMap.insert(strKey, strValue);
            }
        }
        
        {
            QString strKey = m_ui->headerCombox4->currentText().trimmed();
            QString strValue = m_ui->headerEdit4->text().trimmed();
            if (strKey.length() > 0 && strValue.length() > 0) {
                headerMap.insert(strKey, strValue);
            }
        }

        {
            QString strKey = m_ui->headerCombox5->currentText().trimmed();
            QString strValue = m_ui->headerEdit5->text().trimmed();
            if (strKey.length() > 0 && strValue.length() > 0) {
                headerMap.insert(strKey, strValue);
            }
        }

        {
            QString strKey = m_ui->headerCombox6->currentText().trimmed();
            QString strValue = m_ui->headerEdit6->text().trimmed();
            if (strKey.length() > 0 && strValue.length() > 0) {
                headerMap.insert(strKey, strValue);
            }
        }
        
        {
            QString strKey = m_ui->headerCombox7->currentText().trimmed();
            QString strValue = m_ui->headerEdit7->text().trimmed();
            if (strKey.length() > 0 && strValue.length() > 0) {
                headerMap.insert(strKey, strValue);
            }
        }

        // 
        // concurrentSpinBox
        // enableRRcheckBox
        // savePath

        int iConcurrent = m_ui->concurrentSpinBox->value();
        int iEnableRR = m_ui->enableRRcheckBox->isChecked();
        

        QString sValue1 = QString::number(iConcurrent, 10);
        QString sValue2 = QString::number(iEnableRR, 10);
        optionMap.insert(QT_UI_CONCURRENT_CTRL, sValue1);
        optionMap.insert(QT_UI_ENABLE_RR_CTRL, sValue2);
        optionMap.insert(QT_UI_SAVE_PATH_CTRL, strSavePath);

        int iEnableIPv4 = m_ui->enableIPv4->isChecked();
        int iEnableIPv6 = m_ui->enableIPv6->isChecked();
        QString sValueV4 = QString::number(iEnableIPv4, 10);
        QString sValueV6 = QString::number(iEnableIPv6, 10);
        optionMap.insert(OPT_URI_ENABLE_IPV4, sValueV4);
        optionMap.insert(OPT_URI_ENABLE_IPV6, sValueV6);
    }



    QSet<QString> uniqueURLs;
    for (QStringRef url : urls)
    {
        url = url.trimmed();
        if (url.isEmpty()) continue;

        uniqueURLs << url.toString();
    }

    if (uniqueURLs.isEmpty())
    {
        QMessageBox::warning(this, tr("No URL entered"), tr("Please type at least one URL."));
        return;
    }

    BitTorrent::Session *session = BitTorrent::Session::instance();
    if (Utils::Gui::isDirExist(strSavePath)) {
        session->setDefaultSavePath(strSavePath);
    }

    if (m_iMsgId > 0 && m_iProcessId > 0) {
        session->sendSuccessMessageToHosts(m_iMsgId, m_iProcessId);
    }

    QHash<QString, QString> urlToFileNameMap;
    int iMaxIndex = m_fileListModel->rowCount();
    for (int iCurIndex = 0; iCurIndex < iMaxIndex; iCurIndex++) {
        QVariant curNameVar = m_fileListModel->data(m_fileListModel->index(iCurIndex, 0));
        QString  curStrFileName = curNameVar.toString();
        QStandardItem *curItem = m_fileListModel->item(iCurIndex, 0);
        if (curItem != nullptr && curStrFileName.length() > 0) {
            QVariant curUrlVar = curItem->data();
            QString curUrlValue = curUrlVar.toString();
            BitTorrent::CreateXDownParams params = BitTorrent::CreateXDownParams(curUrlValue);
            QString strReallyUrl = params.url;
            if (params.uriFileName != curStrFileName) {
                urlToFileNameMap.insert(strReallyUrl.toLower().trimmed(), curStrFileName);
            }
        }
    }
    emit urlsReadyToBeDownloaded(uniqueURLs.values(), headerMap, optionMap, urlToFileNameMap);

    accept();
}

void DownloadFromURLDialog::OnInitFileList(const QTagDownMsg& xDownMsg)
{
    bool bMatchLink = false;
    if (xDownMsg.iLinkType == 1) {
        bMatchLink = true;
    }

    if (!bMatchLink) return;
    int iIndex = 0;
    for (int idx = 0; idx < xDownMsg.reqLinkList.size(); idx++) {
        QTagLinkItem tmpItem = xDownMsg.reqLinkList.at(idx);
        QString strUrlVal = tmpItem.linkTxt.trimmed();
        QString strFileName = tmpItem.fileName.trimmed();
        QString strFileType = Utils::Gui::getExtByFileName(strFileName);
        QString strFileSize = tmpItem.fileSize.trimmed();
        if (strFileName.length() > 0) {
            // file name
            QStandardItem *standItem1 = new QStandardItem(tr("%1").arg(strFileName));
            standItem1->setEditable(true);
            standItem1->setData(strUrlVal);
            m_fileListModel->setItem(iIndex, 0, standItem1);

            // file ext
            QStandardItem *standItem2 = new QStandardItem(strFileType);
            standItem2->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            standItem2->setEditable(false);
            m_fileListModel->setItem(iIndex, 1, standItem2);

            if (strFileSize.trimmed().length() > 0) {
                QString strSizeFmt = Utils::Gui::getFileSizeByValue(strFileSize.toLongLong());
                if (strSizeFmt.length() > 1) {
                    // file size
                    QStandardItem *standItem3 = new QStandardItem(strSizeFmt);
                    standItem3->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    standItem3->setEditable(false);
                    m_fileListModel->setItem(iIndex, 2, standItem3);
                }
            }

            iIndex++;
        }
    }
}

void DownloadFromURLDialog::textUrlsChanged()
{
    const QString plainText = m_ui->textUrls->toPlainText();
    const QVector<QStringRef> urls = plainText.splitRef('\n');

    // clear
    m_fileListModel->removeRows(0, m_fileListModel->rowCount());

    int iIndex = 0;
    for (QStringRef url : urls) {
        url = url.trimmed();
        if (url.isEmpty()) continue;
        QString source = url.toString();
        if (isXDownable(source)) {
           // aria2 task
            BitTorrent::CreateXDownParams params = BitTorrent::CreateXDownParams(source);
            QString strName = params.uriFileName;
            QString strExt = Utils::Gui::getExtByFileName(strName);
            if (strExt.length() > 5) {
                strExt = "";
            }

            QStandardItem *standItem1 = new QStandardItem(tr("%1").arg(strName));
            standItem1->setEditable(true);

            QString strUrlVal = params.url;
            standItem1->setData(strUrlVal);
            m_fileListModel->setItem(iIndex, 0, standItem1);

            QStandardItem *standItem2 = new QStandardItem(strExt);
            standItem2->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            standItem2->setEditable(false);
            m_fileListModel->setItem(iIndex, 1, standItem2);

            QStandardItem *standItem3 = new QStandardItem("-");
            standItem3->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            standItem3->setEditable(false);
            m_fileListModel->setItem(iIndex, 2, standItem3);

            iIndex++;
        }
    }
}
