#ifndef CAMERAERROR_H
#define CAMERAERROR_H

#include <QString>

namespace CAMERAERROR {
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
#define CAMERA_NOT_CONNECTED 0x000A
#define CAMERA_ACQUIRE_FAILED 0x000B
#define CAMERA_CONFIG_SAVE_FAILED 0x000C
#define CAMERA_CONFIG_LOAD_FAILED 0x000D
#define GETIAMGE_TIMEOUT 0x000E
#define DEVICE_NOT_ACCESSIBLE 0x000F
}

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

