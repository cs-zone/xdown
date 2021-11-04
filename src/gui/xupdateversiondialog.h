#ifndef XUpdateVersionDialog_H
#define XUpdateVersionDialog_H

#include <QDialog>
#include <base/settingvalue.h>

namespace Ui
{
    class XUpdateVersionDialog;
}

class XUpdateVersionDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(XUpdateVersionDialog)

public:
    explicit XUpdateVersionDialog(QWidget *parent);
    ~XUpdateVersionDialog() override;

    void onInitCtrl();

    void OnInitHttpProxyCtrl();

    void loadState();
    void saveState();
    void onIncreaseTimerVal();

private slots:
    void refreshTimer();
    void closeEvent(QCloseEvent *) override;
    void retryBtnClicked();
    void exitBtnClicked();
    void applyBtnClicked();

    void enableApplyButton();
    void enableProxyCheckChanged();
    void OnSaveProxySetting();

signals:

private:
    Ui::XUpdateVersionDialog *m_ui;

    QTimer *m_refreshTimer = nullptr;

    int m_refreshValue = 30;

    bool    m_btnClose = false;

    CachedSettingValue<QSize> m_storeDialogSize;
};

#endif // ABOUTDIALOG_H
