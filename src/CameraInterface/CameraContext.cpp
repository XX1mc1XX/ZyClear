#include "CameraContext.h"
#include "../CameraFactory/CameraFactory.h"
#include "../Utils/ImageConver.h"
#include "ZCCameraMetaInfo.h"
#include "ZCCameraParam.h"
#include "CameraImageQueue.h"
#include "CameraInterface.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <algorithm>

CameraContext* CameraContext::m_pContext = Q_NULLPTR;

// 懒汉单例、未加锁：约定首次 Instance() 在 GUI 线程、任何工作线程启动之前完成；
// 若日后多线程抢首次调用，这里会造出两个实例并泄漏其一
CameraContext* CameraContext::Instance()
{
    if (Q_NULLPTR == m_pContext) {
        m_pContext = new CameraContext();
    }
    return m_pContext;
}

// 与 Instance 配对，main 退出时调用一次，只销毁门面本体
void CameraContext::Release()
{
    if (Q_NULLPTR != m_pContext) {
        delete m_pContext;
        m_pContext = Q_NULLPTR;
    }
}

CameraContext::CameraContext()
{

}

CameraContext::~CameraContext()
{

    QMap<QString, CameraInterface*>::iterator iter;
    for (iter = m_serialCamMap.begin(); iter != m_serialCamMap.end(); ++iter) {
        // 顺序不能反：设备已关再停采会报错
        iter.value()->stopGrabbing();
        iter.value()->disconnect();
        // release 回收厂商 SDK 里的设备句柄，缺这一步句柄会一直留在系统里；
        // 对象所有权在建表时就已移交门面，这里一并销毁，否则每建一台泄漏一个
        iter.value()->release();
        delete iter.value();
    }
    m_serialCamMap.clear();
}

// 每次枚举先拆掉全部旧设备再重建 map，因此重枚举会丢弃已有连接状态；
// 结果追加进调用方传入的 cameraInfos，靠 operator==（仅比序列号）去重；
// 单个厂商创建失败只告警跳过，函数恒返回 ZYCLEAR_OK，一个相机都没找到也不报错
uint32_t CameraContext::EnumerationCamera(QVector<CameraMetaInfo>& cameraInfos)
{

    QMap<QString, CameraInterface*>::iterator iter;
    for (iter = m_serialCamMap.begin(); iter != m_serialCamMap.end(); ++iter) {
        iter.value()->stopGrabbing();
        iter.value()->disconnect();
        // 重枚举会丢弃全部旧对象，句柄与对象都得回收，否则每枚举一次泄漏一轮
        iter.value()->release();
        delete iter.value();
    }
    m_serialCamMap.clear();

    QVector<CameraMetaInfo> infos;
    CameraFactory::instance()->enumCameras(infos);

    for (const auto& info : infos) {
        // 去重只作用于回填给调用方的列表。相机对象必须无条件重建 —— 本函数开头
        // 刚把整张表清空，若沿用「已在列表里就跳过创建」，第二次枚举时该设备会留在
        // 列表里却不进 map，之后按序列号寻址一律落到 NOCAMERA_ERROR。
        if (std::find(cameraInfos.begin(), cameraInfos.end(), info) == cameraInfos.end()) {
            cameraInfos.push_back(info);
        }

        // 同一台设备可能经不同传输层被枚举出两次，只建一次，
        // 否则后建的对象会覆盖 map 里的前一个，前一个再无人释放
        if (m_serialCamMap.contains(info.Serial)) {
            continue;
        }

        CameraInterface* camera = CameraFactory::instance()->createCamera(info);
        if (camera) {
            m_serialCamMap[info.Serial] = camera;   // 对象所有权自此移交门面
            qDebug() << "创建相机成功:" << info.VenderName << info.Serial;
        } else {
            qWarning() << "创建相机失败，不支持的厂商:" << info.VenderName;
        }
    }
    return ZYCLEAR_OK;
}

// 两趟取参：先让厂商给出参数骨架，再逐个 readParam 回填真实值；
// 单个参数读失败不中断，后续参数照常填充
uint32_t CameraContext::getParamList(const QString serial, QVector<CameraParam>& paramList)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    QVector<CameraParam> paramListTemp;
    auto camera = m_serialCamMap[serial];
    camera->getParamList(paramListTemp);

    for (auto var : paramListTemp) {
        camera->readParam(var);
        paramList.push_back(var);
    }

    return ZYCLEAR_OK;
}

uint32_t CameraContext::isConnect(const QString serial, bool& state)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];
    state = camera->isConnect();

    return ZYCLEAR_OK;
}

uint32_t CameraContext::isGrabbing(const QString serial, bool& state)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];
    state = camera->isGrabbing();

    return ZYCLEAR_OK;
}

// 连接分两步：acquire 建句柄、connect 打开设备并挂回调，任一失败即透传错误码
uint32_t CameraContext::connect(const QString serial)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];

    auto ret = camera->acquire();
    if (ret != ZYCLEAR_OK)
        return ret;

    ret = camera->connect();
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

// 与 connect 对称：先 disconnect 关设备，再 release 销毁句柄
uint32_t CameraContext::disconnect(const QString serial)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];

    auto ret = camera->disconnect();
    if (ret != ZYCLEAR_OK)
        return ret;

    ret = camera->release();
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

// 幂等：未连接返 CAMERA_NOT_CONNECTED，已在采集直接返成功；否则建流再启动采集
uint32_t CameraContext::startGrabbing(const QString serial)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];

    if (camera->isConnect() != true)
        return CAMERA_NOT_CONNECTED;

    if (camera->isGrabbing() == true)
        return ZYCLEAR_OK;

    auto ret = camera->creatStream();
    if (ret != ZYCLEAR_OK)
        return ret;

    ret = camera->startGrabbing();
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

// 未连接、或本就没在采集时视作成功（幂等），避免重复停采报错
uint32_t CameraContext::stopGrabbing(const QString serial)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];

    if (camera->isConnect() == false || camera->isGrabbing() == false)
        return ZYCLEAR_OK;

    auto ret = camera->stopGrabbing();
    if (ret != ZYCLEAR_OK)
        return ret;

    ret = camera->destroyStream();
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

// 采集进行中禁止读配置文件；这里用 CAMERA_NOT_CONNECTED 表示"正在采集"，
// 与字面语义不符，调用方别按"未连接"去理解
uint32_t CameraContext::loadConfig(const QString serial, const QString path)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];

    if (camera->isConnect() == false || camera->isGrabbing() == true)
        return CAMERA_NOT_CONNECTED;

    auto ret = camera->loadConfig(path);
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

uint32_t CameraContext::saveConfig(const QString serial, const QString path)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    auto camera = m_serialCamMap[serial];

    if (camera->isConnect() == false || camera->isGrabbing() == true)
        return CAMERA_NOT_CONNECTED;

    auto ret = camera->saveConfig(path);
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

// 序列号不存在时返回空串，调用方据空串判断失败
QString CameraContext::getConfigFormat(const QString serial)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return "";

    CameraInterface* camera = m_serialCamMap[serial];
    QString format = camera->configFormat();

    return QString(format.data());
}

// 纯粹的转发，不校验也不改名：param 的 name/type 必须与相机参数一致
uint32_t CameraContext::readParam(const QString serial, CameraParam& param)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    CameraInterface* camera = m_serialCamMap[serial];
    auto ret = camera->readParam(param);
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

uint32_t CameraContext::writeParam(const QString serial, CameraParam& param)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    CameraInterface* camera = m_serialCamMap[serial];
    auto ret = camera->writeParam(param);
    if (ret != ZYCLEAR_OK)
        return ret;

    return ZYCLEAR_OK;
}

// 取帧失败统一折算成 GETIAMGE_TIMEOUT，不区分真实原因；
// 成功后转 QImage 会深拷贝一份，故随后可安全 Recycle 归还 cv::Mat 缓冲
uint32_t CameraContext::getImageLast(const QString serial, QImage& image)
{
    if (m_serialCamMap.find(serial) == m_serialCamMap.end())
        return NOCAMERA_ERROR;

    CameraInterface* camera = m_serialCamMap[serial];
    cv::Mat cvImage;
    auto ret = camera->getImageLast(cvImage);
    if (ret != ZYCLEAR_OK)
        return GETIAMGE_TIMEOUT;

    image = ImageConver::cvMat2QImage(cvImage);

    // QImage 已深拷贝，缓冲归还空闲池
    camera->ImageQueue().Recycle(cvImage);

    return ZYCLEAR_OK;
}

// 只记录，不校验 serial 是否已注册，也不切换任何相机状态
uint32_t CameraContext::setCurrentSerial(const QString& serial)
{
    m_currentSerial = serial;
    return ZYCLEAR_OK;
}

QString CameraContext::currentSerial() const
{
    return m_currentSerial;
}

