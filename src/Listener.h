#ifndef LISTENER_H
#define LISTENER_H

#include <QMap>
#include <QVector>

enum MESSAGE {
    CAMERA_ENUMRTION = 0x00000001,
    CAMERA_CONNECT = 0x00000002,
    CAMERA_DISCONNECT = 0x00000004,
    CAMERA_STARTGRAB = 0x00000008,
    CAMERA_STOPTGRAB = 0x00000010,
    CAMERA_CAMERASWICH = 0x00000020
};

class Listener {
public:
    Listener() { };
    virtual ~Listener() { };
    virtual void RespondMessage(int message) = 0;
};

class ListenerManger {
    typedef QMap<int, QVector<Listener*>> mmap;
public:

    static ListenerManger* Instance();

    void notify(int message);

    void registerMessage(int message, Listener* listener);

private:
    ListenerManger() { };
    ~ListenerManger() { };
    static ListenerManger* m_pListenerManger;
    QMap<int, QVector<Listener*>> m_messageToLister;
};

#endif

