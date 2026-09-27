#include "Listener.h"

ListenerManger* ListenerManger::m_pListenerManger = new ListenerManger();

// 静态初始化期就建好，Instance() 因而永远返回有效指针，
// 不需要懒加载、也不需要为空指针留分支。
// 代价是这块内存到进程结束都不回收（也不需要回收）——
// 好处是任何静态对象的析构里触发 notify 都还能拿到可用总线。
ListenerManger* ListenerManger::Instance()
{
    return m_pListenerManger;
}

void ListenerManger::notify(int message)
{

    // 精确匹配而不是按位与：这里把 message 当整键查表。
    // 于是「一次 notify 只传单个事件位」成了调用方的硬性契约 ——
    // 传组合值（如 CONNECT|DISCONNECT）在表里找不到对应键，
    // 事件被静默丢弃，且不会有任何报错。这是本总线最容易踩的坑。
    mmap::iterator iter = m_messageToLister.find(message);

    if (iter != m_messageToLister.end()) {
        // 边遍历边投递。RespondMessage 期间若有人在别处 registerMessage，
        // QVector 可能重新分配、令这个迭代器失效；总线约定只在主线程使用，
        // 单线程下不存在这种并发写入。
        QVector<Listener*>::iterator listener = iter.value().begin();
        while (listener != iter.value().end()) {
            (*listener)->RespondMessage(message);
            listener++;
        }
    } else
    {
        // 无人订阅是正常情况（比如某个面板没装配），不是异常，
        // 所以这里连日志都不打，静默吞掉。
        // cout << "no Listener has insterested this meeage" << endl;
    }
}

void ListenerManger::registerMessage(int message, Listener* listener)
{
    // 入表一律用「单个事件位」作键，与 notify 的精确查找口径对齐；
    // 若在这里误把组合值当键写进去，之后永远没有 notify 能匹配到它。
    auto Register = [&](int singleMessage) -> void {
        mmap::iterator iter = m_messageToLister.find(singleMessage);

        if (iter == m_messageToLister.end()) {
            QVector<Listener*> listeners;
            listeners.push_back(listener);
            m_messageToLister[singleMessage] = listeners;
        } else
        {
            iter.value().push_back(listener);
        }
    };

    // 把入参当位掩码逐位拆开，订阅者只会在自己订过的事件上被回调，
    // 不需要在 RespondMessage 里再做过滤。
    //
    // 下面必须覆盖 MESSAGE 枚举里的每一位。漏掉一位不会有任何报错，
    // 订阅它的调用方只是永远收不到该事件（CAMERA_CAMERASWICH 曾漏在这里）。
    // 新增事件时，这里要跟着枚举同步补齐。
    if ((message & MESSAGE::CAMERA_ENUMRTION) == MESSAGE::CAMERA_ENUMRTION) {
        Register(MESSAGE::CAMERA_ENUMRTION);
    }
    if ((message & MESSAGE::CAMERA_CONNECT) == MESSAGE::CAMERA_CONNECT) {
        Register(MESSAGE::CAMERA_CONNECT);
    }
    if ((message & MESSAGE::CAMERA_DISCONNECT) == MESSAGE::CAMERA_DISCONNECT) {
        Register(MESSAGE::CAMERA_DISCONNECT);
    }
    if ((message & MESSAGE::CAMERA_STARTGRAB) == MESSAGE::CAMERA_STARTGRAB) {
        Register(MESSAGE::CAMERA_STARTGRAB);
    }
    if ((message & MESSAGE::CAMERA_STOPTGRAB) == MESSAGE::CAMERA_STOPTGRAB) {
        Register(MESSAGE::CAMERA_STOPTGRAB);
    }
    if ((message & MESSAGE::CAMERA_CAMERASWICH) == MESSAGE::CAMERA_CAMERASWICH) {
        Register(MESSAGE::CAMERA_CAMERASWICH);
    }
}

