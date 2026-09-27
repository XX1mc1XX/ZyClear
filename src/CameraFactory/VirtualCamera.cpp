#include "VirtualCamera.h"
#include "../ParseUiJson/ParseUiJson.h"
#include <chrono>
#include <ctime>
#include <thread>

// 三个标识与工厂里注册用的厂商名绑定；序列号是常量，保证多次枚举结果一致，
// 上层才能按序列号稳定索引同一台虚拟相机。
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

// 恒产出这一台虚拟设备，不依赖任何硬件；签名与真机枚举器一致，
// 工厂因此能把它和真实品牌一视同仁地注册与调用。
uint32_t VirtualCamera::EnumCamera(QVector<CameraMetaInfo>& cameraInfos)
{

    // 字段顺序必须对齐 CameraMetaInfo 的声明 { Serial, UserDefineID, VenderName }：
    // 两个常量名字相近，顺序写反了照样编译通过，序列号与显示名却就此互换
    cameraInfos.push_back(CameraMetaInfo { VIRTUAL_CAMERA_SERIAL, VIRTUAL_CAMERA_NAME, VIRTUAL_CAMERA_VENDER });
    return ZYCLEAR_OK;
}

// 参数骨架来自 Qt 资源里的 JSON，由 ParseUiJson 单例解析，而该单例是全局共享的：
// 别的相机也可能往它里面塞过数据，所以这里每调一次都重载一遍，保证本次结果正确。
// 返回的只是 UI 骨架，具体值要等 readParam 逐项填充。
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

// 直接回状态位：真机这里要问 SDK 是否在线，仿真层没有底层可问。
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

// 只翻状态位，不做任何资源操作：真机的 acquire/release、独占检查在这里退化为空。
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

// 起采即拉起一条 detach 的造帧线程，线程以 isGrabbing() 为唯一退出条件：
// 必须由 stopGrabbing() 收尾，对象销毁前若忘了调它，线程会继续访问已析构的 this。
// 线程每 300ms 造一帧 512x512 纯色图投进与真机共用的有界队列，队列满时按队列既定
// 策略丢旧帧，正好模拟真机受带宽/缓冲限制的丢帧行为。
uint32_t VirtualCamera::startGrabbing()
{
    auto CreateImage = [this]() -> void {
        while (this->isGrabbing()) {

            cv::Mat canvas = cv::Mat::zeros(cv::Size(512, 512), CV_8UC3);
            // 每轮用 time(NULL) 重播种子，同一秒内的多帧会是同一种颜色，
            // 只为让画面动起来，不追求真随机。
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

// 置位后造帧线程最多再走完一轮睡眠才退出，期间可能仍多进一帧，
// 所以停采后队列里残留最后一帧是允许的，取帧侧不应把它当异常。
uint32_t VirtualCamera::stopGrabbing()
{
    m_starGrabbing = false;
    return ZYCLEAR_OK;
}

// 配置导入导出是空实现：真机写的是厂商私有参数文件，仿真层没有可存的状态，
// 直接报成功以免上层为虚拟相机加分支。
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

// 不查设备也不查权限，按声明类型直接编一份合法的假值返回，三个权限位一律置真：
// 目的就是让 UI 把各类控件都画出来并允许操作，覆盖取值/枚举/字符串等所有控件形态。
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

// 写入不落地：虚拟相机没有可写的寄存器，接受即成功，
// 免得 UI 的“应用参数”流程在这里断掉。
uint32_t VirtualCamera::writeParam(CameraParam& param)
{
    return ZYCLEAR_OK;
}

uint32_t VirtualCamera::getImageLast(cv::Mat& image)
{
    // 直接取进输出参数，省去一次全图拷贝
    // 与真机同一语义：超时/空队列回非零码，取出的 Mat 由调用方经 Recycle 归还。
    return m_imageQueue.Take(image);
}

