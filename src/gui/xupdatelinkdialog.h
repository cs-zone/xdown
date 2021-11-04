#ifndef __XUpdateLinkDialog_H__
#define __XUpdateLinkDialog_H__

#include <QDialog>

namespace Ui
{
    class XUpdateLinkDialog;
}

class XUpdateLinkDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(XUpdateLinkDialog)

public:
    explicit XUpdateLinkDialog(QWidget *parent);
    ~XUpdateLinkDialog() override;

    void onInitCtrl();
    void setPrvLinkValue(const QString &strVal);

    bool OnCheckInputVal(const QString &strVal);

private slots:
    void closeEvent(QCloseEvent *) override;
    void okBtnClicked();
    void exitBtnClicked();

signals:
    void updateToOK(const QString &strVal, const QString &strUpdateLinkVal);
    void updateToExit();

private:
    Ui::XUpdateLinkDialog *m_ui;
    bool    m_btnClose = false;

    QString m_strPrvLinkValue;
};

#endif // ABOUTDIALOG_H
