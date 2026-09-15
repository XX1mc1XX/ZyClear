#include "Listener.h"

ListenerManger* ListenerManger::m_pListenerManger = new ListenerManger();

ListenerManger* ListenerManger::Instance()
{
    return m_pListenerManger;
}

void ListenerManger::notify(int message)
{

    mmap::iterator iter = m_messageToLister.find(message);

    if (iter != m_messageToLister.end()) {
        QVector<Listener*>::iterator listener = iter.value().begin();
        while (listener != iter.value().end()) {
            (*listener)->RespondMessage(message);
            listener++;
        }
    } else
    {
        // cout << "no Listener has insterested this meeage" << endl;
    }
}

void ListenerManger::registerMessage(int message, Listener* listener)
{
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
}

