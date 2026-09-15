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
    stop();
}

void AcquireImageProcess::setSerial(QString serial)
{
    m_serial = serial;
}

void AcquireImageProcess::stop()
{
    requestInterruption();

    // 取帧最长阻塞 TIME_OUT_MS，等待需覆盖一次完整的取帧超时
    if (!wait(TIME_OUT_MS + 2000)) {
        qWarning() << "AcquireImageProcess: 采集线程未在超时内退出";
    }
}

void AcquireImageProcess::run()
{
    while (!isInterruptionRequested()) {
        QImage image;
        auto ret = CameraContext::Instance()->getImageLast(m_serial, image);

        // 停止请求可能在阻塞取帧期间到达，此时不应再向界面投递图像
        if (isInterruptionRequested())
            break;

        if (ret != ZYCLEAR_OK)
            continue;

        emit sigUpdateImage(image);
    }
}

