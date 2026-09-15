#ifndef CMCAMERAMETAINFO_H
#define CMCAMERAMETAINFO_H

#include <QString>

struct CameraMetaInfo {
    QString Serial {};
    QString UserDefineID {};
    QString VenderName {};

    bool operator==(const CameraMetaInfo& info)
    {
        if (Serial == info.Serial) {
            return true;
        }
        return false;
    }
};

#endif

