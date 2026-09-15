#include "VirtualCamera.h"
#include "../ParseUiJson/ParseUiJson.h"
#include <chrono>
#include <ctime>
#include <thread>

const QString VirtualCamera::VIRTUAL_CAMERA_NAME = "VirtualCamera";
const QString VirtualCamera::VIRTUAL_CAMERA_SERIAL = "Vir123456";
const QString VirtualCamera::VIRTUAL_CAMERA_VENDER = "Virtual";

VirtualCamera::VirtualCamera(const CameraMetaInfo& info)
    : CameraInterface(info)
{
}

VirtualCamera::~VirtualCamera()
{
}

uint32_t VirtualCamera::EnumCamera(QVector<CameraMetaInfo>& cameraInfos)
{

    cameraInfos.push_back(CameraMetaInfo { VIRTUAL_CAMERA_NAME, VIRTUAL_CAMERA_SERIAL, VIRTUAL_CAMERA_VENDER });
    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::getParamList(QVector<CameraParam>& paramList)
{
    ParseUiJson* parser = ParseUiJson::instance();
    parser->loadFromFile(":/VirtualCameraParam.json");
    QList<CameraParamMetaInfo> paramMetaInfoList = parser->getParamList();

    for (auto var : paramMetaInfoList) {
        paramList.push_back(CameraParam(var));
    }

    return ZYCLEAR_OK;
}

bool VirtualCamera::isConnect()
{
    return m_connect;
}

bool VirtualCamera::isGrabbing()
{
    return m_starGrabbing;
}

uint32_t VirtualCamera::acquire()
{
    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::release()
{
    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::connect()
{
    m_connect = true;
    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::disconnect()
{
    m_connect = false;
    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::creatStream()
{
    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::destroyStream()
{
    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::startGrabbing()
{
    auto CreateImage = [this]() -> void {
        while (this->isGrabbing()) {

            cv::Mat canvas = cv::Mat::zeros(cv::Size(512, 512), CV_8UC3);
            cv::RNG rng(time(NULL));
            int b = rng.uniform(0, 255);
            int g = rng.uniform(0, 255);
            int r = rng.uniform(0, 255);
            canvas.setTo(cv::Scalar(b, g, r));

            this->getImageQueue().Put(canvas);

            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    };

    m_starGrabbing = true;

    std::thread image_callBack(CreateImage);
    image_callBack.detach();

    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::stopGrabbing()
{
    m_starGrabbing = false;
    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::loadConfig(const QString path)
{
    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::saveConfig(const QString path)
{
    return ZYCLEAR_OK;
}

QString VirtualCamera::configFormat()
{
    return "xml";
}

uint32_t VirtualCamera::readParam(CameraParam& param)
{
    param.setValid(true);
    param.setReadable(true);
    param.setWriteable(true);

    if (param.type() == INT) {
        IntParam varParam = param.GetValue().value<IntParam>();
        varParam.value = 30;
        varParam.increment = 1;
        varParam.min = 0;
        varParam.max = 100;
        param.SetValue(QVariant::fromValue(varParam));
    } else if (param.type() == DOUBLE) {
        DoubleParam varParam = param.GetValue().value<DoubleParam>();
        varParam.value = 55.6;
        varParam.min = 0;
        varParam.max = 1000;
        param.SetValue(QVariant::fromValue(varParam));
    } else if (param.type() == ENUM) {

        EnumParam varParam = param.GetValue().value<EnumParam>();
        varParam.value = "item1";
        varParam.valueInt = 0;
        varParam.availableInt = QVector<int> { 0, 1, 2 };
        varParam.availableValue = QVector<QString> { "item1", "item2", "item3" };
        param.SetValue(QVariant::fromValue(varParam));
    } else if (param.type() == BOOL) {
        BoolParam varParam = param.GetValue().value<BoolParam>();
        varParam.value = false;
        param.SetValue(QVariant::fromValue(varParam));
    } else if (param.type() == CMD) {

    } else if (param.type() == STRING) {
        StringParam varParam = param.GetValue().value<StringParam>();
        varParam.value = "string_value";
        varParam.nMaxLength = 16;
        param.SetValue(QVariant::fromValue(varParam));
    }

    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::writeParam(CameraParam& param)
{
    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::getImageLast(cv::Mat& image)
{
    // 直接取进输出参数，省去一次全图拷贝
    return m_imageQueue.Take(image);
}

