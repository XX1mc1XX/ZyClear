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

