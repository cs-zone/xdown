

#include "xupdateversiondialog.h"

#include <QFile>

#include "app/application.h"
#include "base/bittorrent/session.h"
#include "base/unicodestrings.h"
#include "base/utils/misc.h"
#include "ui_xupdateversiondialog.h"
#include "utils.h"
#include "base/global.h"
#include <qtimer.h>
#include <qspinbox.h>
#include <qdebug.h>


#define SETTINGS_KEY(name) "UpdateVersionDialog/" name

XUpdateVersionDialog::XUpdateVersionDialog(QWidget *parent)
    : QDialog(parent)
    , m_storeDialogSize(SETTINGS_KEY("DialogSize"))
    , m_ui(new Ui::XUpdateVersionDialog)
    , m_refreshTimer{ new QTimer {this} }
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    int iValue = 1000;
    m_refreshTimer->setInterval(iValue);
    connect(m_refreshTimer, &QTimer::timeout, this, &XUpdateVersionDialog::refreshTimer);

    connect(m_ui->retryBtn, &QPushButton::clicked, this, &XUpdateVersionDialog::retryBtnClicked);
    connect(m_ui->exitBtn,  &QPushButton::clicked, this, &XUpdateVersionDialog::exitBtnClicked);

    connect(m_ui->applyBtn, &QPushButton::clicked, this, &XUpdateVersionDialog::applyBtnClicked);

    onInitCtrl();

    loadState();
    show();
}

XUpdateVersionDialog::~XUpdateVersionDialog()
{
    saveState();
    delete m_ui;
}


void XUpdateVersionDialog::loadState()
{
    Utils::Gui::resize(this, m_storeDialogSize);
}

void XUpdateVersionDialog::saveState()
{
    m_storeDialogSize = size();
}

void XUpdateVersionDialog::onInitCtrl()
{
    QString strTitle = tr("Version detection failed");
    setWindowTitle(strTitle);

    QString strMessage1 = tr("Please ensure connect to the Internet when starting the program");
    QString strMessage2 = tr("Check if the set HTTP proxy is valid");

    QString strText = QString("%1\r\n%2\r\n").arg(strMessage1, strMessage2);
    m_ui->textEdit->setText(strText);

    QString strExitText = tr("Exit");
    m_ui->exitBtn->setText(strExitText);

    

    {
        m_ui->enableProxyCheckBox->setText(tr("Enable Http Proxy"));
        m_ui->hostLabel->setText(tr("Host:"));
        m_ui->portLabel->setText(tr("Port:"));
        m_ui->enableAuthenCheckBox->setText(tr("Authentication"));
        m_ui->userNameLabel->setText(tr("User Name:"));
        m_ui->passwordLabel->setText(tr("Password:"));

        m_ui->applyBtn->setText(tr("Apply"));

        void (QSpinBox::*qSpinBoxValueChanged)(int) = &QSpinBox::valueChanged;

        connect(m_ui->proxyHostLineEdit, &QLineEdit::textChanged, this, &XUpdateVersionDialog::enableApplyButton);
        connect(m_ui->proxyPortSpinBox, qSpinBoxValueChanged, this, &XUpdateVersionDialog::enableApplyButton);
        connect(m_ui->proxyUserLineEdit, &QLineEdit::textChanged, this, &XUpdateVersionDialog::enableApplyButton);
        connect(m_ui->proxyPasswordLineEdit, &QLineEdit::textChanged, this, &XUpdateVersionDialog::enableApplyButton);

        connect(m_ui->enableProxyCheckBox, &QAbstractButton::toggled, this, &XUpdateVersionDialog::enableApplyButton);
        connect(m_ui->enableProxyCheckBox, &QAbstractButton::toggled, this, &XUpdateVersionDialog::enableProxyCheckChanged);

        connect(m_ui->enableAuthenCheckBox, &QAbstractButton::toggled, this, &XUpdateVersionDialog::enableApplyButton);
        connect(m_ui->enableAuthenCheckBox, &QAbstractButton::toggled, this, &XUpdateVersionDialog::enableProxyCheckChanged);

        const auto *session = BitTorrent::Session::instance();

        m_ui->enableProxyCheckBox->setChecked(session->getHttpEnableProxy());
        m_ui->proxyHostLineEdit->setText(session->getHttpProxyHost());
        int proxyPort = session->getHttpProxyPort();
        m_ui->proxyPortSpinBox->setValue(session->getHttpProxyPort());
        m_ui->enableAuthenCheckBox->setChecked(session->getHttpEnableAuth());
        m_ui->proxyUserLineEdit->setText(session->getHttpProxyUser());
        m_ui->proxyPasswordLineEdit->setText(session->getHttpProxyPassword());
    }

    m_refreshValue = 20;
    QString strRetryText = tr("Retry");
    strRetryText.append(QString("(%1)").arg(QString::number(m_refreshValue)));
    m_ui->retryBtn->setText(strRetryText);
    m_refreshTimer->start();
    m_ui->applyBtn->setEnabled(false);
    m_btnClose = false;
}

void XUpdateVersionDialog::onIncreaseTimerVal()
{
    if (m_refreshValue + 30 <= 120) {
        m_refreshValue = m_refreshValue + 30;
    }
    else {
        m_refreshValue = 120;
    }
}

void XUpdateVersionDialog::enableApplyButton()
{
    onIncreaseTimerVal();
    m_ui->applyBtn->setEnabled(true);
}


void XUpdateVersionDialog::enableProxyCheckChanged()
{
}

void XUpdateVersionDialog::OnInitHttpProxyCtrl()
{
    
}

void XUpdateVersionDialog::OnSaveProxySetting()
{
}

void XUpdateVersionDialog::applyBtnClicked()
{
    OnSaveProxySetting();
    m_ui->applyBtn->setEnabled(false);
}

// m_refreshValue
void XUpdateVersionDialog::refreshTimer()
{
    --m_refreshValue;
    if (m_refreshValue > 0) {
        QString strRetryText = tr("Retry");
        strRetryText.append(QString("(%1)").arg(QString::number(m_refreshValue)));
        m_ui->retryBtn->setText(strRetryText);
    }
    else {
        m_refreshTimer->stop();
        retryBtnClicked();
    }
}


void XUpdateVersionDialog::retryBtnClicked()
{
    m_btnClose = true;
    close();
}

// ExitProcess
void XUpdateVersionDialog::exitBtnClicked()
{
    m_btnClose = true;
    close();
}

void XUpdateVersionDialog::closeEvent(QCloseEvent *e)
{
    m_refreshTimer->stop();
}
