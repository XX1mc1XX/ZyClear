#ifndef ACQUIREIMAGEPROCESS_H
#define ACQUIREIMAGEPROCESS_H

#include <QImage>
#include <QObject>
#include <QString>
#include <QThread>

class AcquireImageProcess : public QThread {
    Q_OBJECT
public:
    explicit AcquireImageProcess(QObject* parent = nullptr);
    ~AcquireImageProcess();

    void setSerial(QString serial);

    // 请求线程退出并等待结束
    void stop();

signals:
    void sigUpdateImage(const QImage& iamge);

protected:
    void run() override;

private:
    QString m_serial {};
};

#endif

