#ifndef CAMERAERROR_H
#define CAMERAERROR_H

#include <QString>

// 全项目统一的错误码字典：0 为成功，非 0 一律视为失败；
// 各厂商实现的返回码都必须落在本表内，上层才能靠 getErrorInfoEn 还原成可展示文案
namespace CAMERAERROR {
// 这些是宏而非常量，预处理期没有命名空间概念，实际作用域是全局的；
// 调用点必须写裸名，加 CAMERAERROR:: 前缀反而查不到
#define ZYCLEAR_OK 0x0000
#define CONNECT_ERROR 0X0001
#define DISCONNECT_ERROR 0X0002
#define STARTGRAB_ERROR 0x0003
#define STOPGRAB_ERROR 0x0004
#define NOCAMERA_ERROR 0x0005
#define INVALID_INPUT 0x0006
#define INVALID_CAMERA_HANDLE 0x0007
#define WRITE_PARAM_FAILED 0x0008
#define READ_PARAM_FAILED 0x0009
#define CAMERA_NOT_CONNECTED 0x000A   // 门面在"正在采集故拒绝操作"时也复用它，别只按字面理解
#define CAMERA_ACQUIRE_FAILED 0x000B   // 指句柄/设备创建失败，不是"采集启动失败"
#define CAMERA_CONFIG_SAVE_FAILED 0x000C
#define CAMERA_CONFIG_LOAD_FAILED 0x000D
#define GETIAMGE_TIMEOUT 0x000E
#define DEVICE_NOT_ACCESSIBLE 0x000F
}

// static 给出内部链接，每个翻译单元各持一份副本，故放头文件不会重复定义；
// 代价是改动只对当前 TU 生效，跨 TU 调试时留意别被旧副本误导
static QString getErrorInfoEn(unsigned int error)
{
    QString info {};
    switch (error) {
    case ZYCLEAR_OK: {
        info = "Run Success";
        break;
    }
    case CONNECT_ERROR: {
        info = "Camera Connect failed";
        break;
    }
    case DISCONNECT_ERROR: {
        info = "Camera DisConnect failed";
        break;
    }
    case STARTGRAB_ERROR: {
        info = "Camera StartGrabing failed";
        break;
    }
    case STOPGRAB_ERROR: {
        info = "Camera StopGrabing failed";
        break;
    }
    case NOCAMERA_ERROR: {
        info = "Not Find Any Camera";
        break;
    }
    case INVALID_INPUT: {
        info = "Invalid Input Parameter";
        break;
    }
    case INVALID_CAMERA_HANDLE: {
        info = "The camera handle is invalid";
        break;
    }
    case WRITE_PARAM_FAILED: {
        info = "Write Param failed";
        break;
    }
    case READ_PARAM_FAILED: {
        info = "Read Param failed";
        break;
    }
    case CAMERA_NOT_CONNECTED: {
        info = "Camera not connected";
        break;
    }
    case CAMERA_ACQUIRE_FAILED: {
        info = "Camera creation failure";
        break;
    }
    case CAMERA_CONFIG_SAVE_FAILED: {
        info = "Failed to export the camera configuration file";
        break;
    }
    case CAMERA_CONFIG_LOAD_FAILED: {
        info = "Failed to import the camera configuration file";
        break;
    }
    case GETIAMGE_TIMEOUT: {
        info = "Get Image TimeOut";
        break;
    }
    case DEVICE_NOT_ACCESSIBLE: {
        info = "The device is  UnAccessible";
        break;
    }
    default: {
        info = "Unkonw Error";
        break;
    }
    }
    return info;
}

#endif

