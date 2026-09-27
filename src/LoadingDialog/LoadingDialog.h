#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H

#include <QDialog>
#include <QMovie>
#include <QThread>
#include <QTimer>
#include <QtConcurrent>

class QLabel;
class QPushButton;

class LoadingDialog : public QDialog {
    Q_OBJECT

public:
    explicit LoadingDialog(QWidget* parent = nullptr);
    ~LoadingDialog();

    // 全进程只允许一个加载框，所以入口做成静态的：调用方不必持有指针，
    // 谁都能随手唤起。它不是 RAII —— Loading 与 HideLoading 必须成对，
    // 且 HideLoading 会直接 delete 掉实例，下次调用再重新构造。
    static void Loading(QWidget* parent = nullptr);

    static void HideLoading();

private slots:
    void on_Close_Button_clicked();

private:

    void setupUi();

    QPushButton* m_pCloseButton;
    QLabel* m_pGifLabel;
    QMovie* m_pLoadingMovie;
};

#endif

