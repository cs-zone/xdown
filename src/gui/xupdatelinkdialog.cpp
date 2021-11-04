

#include "xupdatelinkdialog.h"

#include <QFile>

#include "base/unicodestrings.h"
#include "base/utils/misc.h"
#include "ui_xupdatelinkdialog.h"
#include "utils.h"
#include "base/global.h"
#include <qtimer.h>
#include <qdebug.h>
#include <qmessagebox.h>

XUpdateLinkDialog::XUpdateLinkDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::XUpdateLinkDialog)
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    //g_iActiveWin = (int)E_Options_Dialog;
    setModal(true);

    connect(m_ui->okBtn,    &QPushButton::clicked, this, &XUpdateLinkDialog::okBtnClicked);
    connect(m_ui->exitBtn,  &QPushButton::clicked, this, &XUpdateLinkDialog::exitBtnClicked);

    onInitCtrl();

    Utils::Gui::resize(this);
    show();
}

XUpdateLinkDialog::~XUpdateLinkDialog()
{
    delete m_ui;
}

void XUpdateLinkDialog::onInitCtrl()
{
    // 大文件-更新下载链接
    QString strTitle = tr("Big file - update download link");
    setWindowTitle(strTitle);
    m_btnClose = false;
}

bool XUpdateLinkDialog::OnCheckInputVal(const QString &strVal)
{
    QString strTmp(strVal);
    if (strTmp.startsWith("aria2c ")) {
        int iPosVal = strTmp.indexOf(" ");
        if (iPosVal > 0) {
            strTmp.mid(iPosVal).trimmed();
        }
    }
    if (strTmp.startsWith("http://") || strVal.startsWith("https://") || strVal.startsWith("ftp://")) {
        return true;
    }
    return false;
}

void XUpdateLinkDialog::okBtnClicked()
{
    qDebug() << "====okBtnClicked===";
    QString strCurUrlVal = m_ui->textEdit->toPlainText();
    if (!OnCheckInputVal(strCurUrlVal))
    {
        QMessageBox::warning(nullptr, tr("Error"), tr("invalid url"));
        return;
    }

    emit updateToOK(m_strPrvLinkValue, strCurUrlVal);

    m_btnClose = true;
    close();

}

// ExitProcess
void XUpdateLinkDialog::exitBtnClicked()
{
    qDebug() << "====exitBtnClicked===";
    m_btnClose = true;
    close();
}

void XUpdateLinkDialog::setPrvLinkValue(const QString &strVal)
{
    m_strPrvLinkValue = strVal;
}

void XUpdateLinkDialog::closeEvent(QCloseEvent *e)
{
}
