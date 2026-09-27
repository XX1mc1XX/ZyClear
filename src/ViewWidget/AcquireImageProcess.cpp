#include "AcquireImageProcess.h"
#include "CameraInterface/CameraContext.h"
#include "CameraInterface/CameraError.h"
#include "CameraInterface/CameraImageQueue.h"
#include <QDebug>

AcquireImageProcess::AcquireImageProcess(QObject* parent)
    : QThread(parent)
{
}

AcquireImageProcess::~AcquireImageProcess()
{
    // 兜底停线程：QThread 对象销毁时线程还在跑会被 Qt 强杀，取帧恰好持有
    // 相机缓冲的话，池子里会留下一个半写状态的内存块。
    stop();
}

void AcquireImageProcess::setSerial(QString serial)
{
    m_serial = serial;
}

void AcquireImageProcess::stop()
{
    requestInterruption();

    // 取帧最长阻塞 TIME_OUT_MS，等待需覆盖一次超时
    // 只告警不强杀：等不到说明线程真卡死了，terminate 会把相机 SDK 的内部
    // 状态留在未知状态，比让进程带着一个僵尸线程更危险。
    if (!wait(TIME_OUT_MS + 2000)) {
        qWarning() << "AcquireImageProcess: 采集线程未在超时内退出";
    }
}

void AcquireImageProcess::run()
{
    // 空闲时此处每 TIME_OUT_MS 空转一轮，靠固定超时把单核占用压到可忽略；
    // 也正因为有超时，上面的 stop() 才能在一个确定的时间窗内等到退出。
    while (!isInterruptionRequested()) {
        QImage image;
        // getImageLast 内部已把 cv::Mat 深拷成 QImage 并立刻归还了队列缓冲，
        // 所以 image 与采集侧内存彻底脱钩，可以安全跨线程投递。
        auto ret = CameraContext::Instance()->getImageLast(m_serial, image);

        // 停止请求可能在取帧阻塞期间到达，此时不投递
        if (isInterruptionRequested())
            break;

        // 取不到帧（超时）是空闲时的常态，直接跳过；这里刻意不记日志，
        // 否则相机未出图时日志会被超时刷屏。
        if (ret != ZYCLEAR_OK)
            continue;

        // 接收方在界面线程，Qt 自动走队列连接把 QImage 连引用计数一起递过去
        emit sigUpdateImage(image);
    }
}

