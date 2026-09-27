#ifndef ACQUIREIMAGEPROCESS_H
#define ACQUIREIMAGEPROCESS_H

#include <QImage>
#include <QObject>
#include <QString>
#include <QThread>

// 图像链路的第三级隔离：把"等帧"从界面线程挪到这个辅助线程里。相机回调与
// 缓冲队列在前面两级已经解耦，这里再断一次，界面线程就只剩渲染一件事，
// 满帧时不会因为某次取帧超时而卡住。
class AcquireImageProcess : public QThread {
    Q_OBJECT
public:
    explicit AcquireImageProcess(QObject* parent = nullptr);
    ~AcquireImageProcess();

    void setSerial(QString serial);

    // 请求线程退出并等待结束
    void stop();

signals:
    // 刻意投递 QImage 而不是 cv::Mat：Mat 的缓冲归采集队列的池子所有，出了
    // 取帧作用域就要归还，跨线程拿着它等于拿野指针；QImage 在转换时已深拷贝，
    // 且隐式共享让队列投递只加一次引用计数。
    void sigUpdateImage(const QImage& iamge);

protected:
    void run() override;

private:
    // 只由界面线程写、由 run() 在读之前先落定，故无需加锁；一旦允许运行中
    // 改序列号就必须改成原子量或加保护。
    QString m_serial {};
};

#endif

