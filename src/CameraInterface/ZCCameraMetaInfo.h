#ifndef CMCAMERAMETAINFO_H
#define CMCAMERAMETAINFO_H

#include <QString>

// 相机身份以 Serial 为准；UserDefineID 是用户可改的显示名，不承担主键职责
struct CameraMetaInfo {
    QString Serial {};
    QString UserDefineID {};
    QString VenderName {};

    // 相等性只由 Serial 决定：枚举去重与工厂按序匹配都依赖这一点，
    // 若把 UserDefineID 纳入比较，改名后的相机会被当成新设备重复登记
    bool operator==(const CameraMetaInfo& info)
    {
        if (Serial == info.Serial) {
            return true;
        }
        return false;
    }
};

#endif

