#include "AcquireImageProcess.h"
#include "CameraInterface/CameraContext.h"
#include "CameraInterface/CameraError.h"

AcquireImageProcess::AcquireImageProcess(QObject* parent)
    : QThread(parent)
{
}

AcquireImageProcess::~AcquireImageProcess()
{
}

void AcquireImageProcess::setSerial(QString serial)
{
    m_serial = serial;
}

void AcquireImageProcess::run()
{
    while (true) {
        QImage image;
        auto ret = CameraContext::Instance()->getImageLast(m_serial, image);
        if (ret == GETIAMGE_TIMEOUT)
            continue;

        emit sigUpdateImage(image);
    }
}

