#include "CameraImageQueue.h"
#include "CameraError.h"

CameraImageQueue::CameraImageQueue()
{
    m_queueSize = 10;
    m_needStop = false;
    for (int i = 0; i < m_queueSize; i++) {
        freeImageQueue.push(cv::Mat());
    }
}

CameraImageQueue::CameraImageQueue(int maxSize)
{
    m_queueSize = maxSize;
    m_needStop = false;
    for (int i = 0; i < maxSize; i++) {
        freeImageQueue.push(cv::Mat());
    }
}

uint32_t CameraImageQueue::Put(const cv::Mat& m)
{
    std::unique_lock<std::mutex> locker(m_mutex);
    if (freeImageQueue.size() != 0) {

        cv::Mat temp = freeImageQueue.front();
        freeImageQueue.pop();
        temp = m;
        workImageQueue.push(temp);
    } else {

        workImageQueue.pop();
        workImageQueue.push(m);
    }
    m_condition.notify_one();
    return ZYCLEAR_OK;
}

uint32_t CameraImageQueue::Take(cv::Mat& m)
{
    std::unique_lock<std::mutex> locker(m_mutex);
    std::chrono::milliseconds dura(TIME_OUT_MS);

    auto state = m_condition.wait_for(locker, dura, [this] { return m_needStop || NotEmpty(); });

    if (state == false) {
        return ZYCLEAR_OK;
    }

    m = workImageQueue.front();
    workImageQueue.pop();
    freeImageQueue.push(m);
    return ZYCLEAR_OK;
}

bool CameraImageQueue::Empty()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return workImageQueue.empty();
}

bool CameraImageQueue::Full()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return workImageQueue.size() == m_queueSize;
}

size_t CameraImageQueue::Size()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return workImageQueue.size();
}

bool CameraImageQueue::isFull() const
{
    bool full = workImageQueue.size() >= m_queueSize;
    return full;
}

bool CameraImageQueue::isEmpty() const
{
    bool empty = workImageQueue.empty();
    return empty;
}

bool CameraImageQueue::NotFull() const
{
    bool full = workImageQueue.size() >= m_queueSize;
    return !full;
}

bool CameraImageQueue::NotEmpty() const
{
    bool empty = workImageQueue.empty();
    return !empty;
}

