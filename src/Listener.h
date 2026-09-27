#ifndef LISTENER_H
#define LISTENER_H

#include <QMap>
#include <QVector>

// 事件位掩码。六个常量必须各占一个独立的二进制位且互不重叠 ——
// 订阅侧靠按位与把组合值拆成一个个事件位入表，
// 所以新增事件时只能往后取更大的 2 的幂，不能复用已占用的位。
enum MESSAGE {
    CAMERA_ENUMRTION = 0x00000001,
    CAMERA_CONNECT = 0x00000002,
    CAMERA_DISCONNECT = 0x00000004,
    CAMERA_STARTGRAB = 0x00000008,
    CAMERA_STOPTGRAB = 0x00000010,
    CAMERA_CAMERASWICH = 0x00000020
};

// 订阅者接口。故意不继承 QObject：总线走的是普通虚函数调用，
// 不经过信号槽，因此 RespondMessage 是在 notify() 的调用栈上同步执行的，
// 也正因如此它必须跑在与 notify 相同的线程（实际上只有主线程）。
class Listener {
public:
    Listener() { };
    virtual ~Listener() { };
    // 传进来的 message 保证是「单个」事件位，不会是组合值，
    // 实现里直接做等值比较即可，不需要再按位解析。
    // 禁止在回调里注册新订阅（见 registerMessage 的说明）。
    virtual void RespondMessage(int message) = 0;
};

// 进程内唯一的全局总线。它不持有订阅者的所有权，也没有反注册接口 ——
// 一旦注册，Listener* 就永久留在表里，因此订阅者必须活得比所有后续 notify 更久，
// 否则 notify 会在已析构的对象上调虚函数。
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
    // 键是单个事件位，值是订阅该事件的订阅者列表。
    // 同一对象重复订阅同一事件会被追加进列表多次（没有去重），
    // 效果是每次 notify 它都被调用多次。
    QMap<int, QVector<Listener*>> m_messageToLister;
};

#endif

