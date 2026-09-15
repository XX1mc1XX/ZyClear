#include "CameraImageQueue.h"
#include "CameraError.h"

CameraImageQueue::CameraImageQueue()
{
    m_queueSize = ImageQueueSize;
    for (int i = 0; i < m_queueSize; i++) {
        freeImageQueue.push(cv::Mat());
    }
}

CameraImageQueue::CameraImageQueue(int maxSize)
{
    m_queueSize = maxSize;
    for (int i = 0; i < maxSize; i++) {
        freeImageQueue.push(cv::Mat());
    }
}

uint32_t CameraImageQueue::Put(const cv::Mat& m)
{
    std::unique_lock<std::mutex> locker(m_mutex);

    // 队列满时丢最旧帧，并把它占用的缓冲收回空闲池
    if (freeImageQueue.empty() && workImageQueue.size() >= m_queueSize
        && !workImageQueue.empty()) {
        freeImageQueue.push(workImageQueue.front());
        workImageQueue.pop();
    }

    cv::Mat buffer;
    if (!freeImageQueue.empty()) {
        buffer = freeImageQueue.front();
        freeImageQueue.pop();
    }

    // 尺寸与类型不变时复用已有内存，仅在分辨率或格式变化时重新分配
    if (buffer.size() == m.size() && buffer.type() == m.type()) {
        m.copyTo(buffer);
    } else {
        buffer = m.clone();
    }

    workImageQueue.push(buffer);
    m_condition.notify_one();
    return ZYCLEAR_OK;
}

uint32_t CameraImageQueue::Take(cv::Mat& m)
{
    std::unique_lock<std::mutex> locker(m_mutex);
    std::chrono::milliseconds dura(TIME_OUT_MS);

    auto state = m_condition.wait_for(locker, dura, [this] { return NotEmpty(); });

    // 取不到帧时回超时码，不产出空帧
    if (state == false || workImageQueue.empty()) {
        return GETIAMGE_TIMEOUT;
    }

    // 所有权转移给调用方，缓冲由它经 Recycle 归还
    m = workImageQueue.front();
    workImageQueue.pop();
    return ZYCLEAR_OK;
}

void CameraImageQueue::Recycle(const cv::Mat& m)
{
    if (m.empty())
        return;

    std::lock_guard<std::mutex> locker(m_mutex);
    freeImageQueue.push(m);
}

bool CameraImageQueue::NotEmpty() const
{
    return !workImageQueue.empty();
}
