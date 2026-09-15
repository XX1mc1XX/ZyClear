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

    // 空闲缓冲用尽、且工作队列已达上限（说明没有在途帧）时，丢最旧的帧并把它
    // 的缓冲回收进空闲池：既做到保新弃旧，又不让缓冲池随丢帧单向流失
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

    // 尺寸与类型不变时原地写入，复用已分配内存；只有分辨率或像素格式变化
    // 的那一次才重新分配，此后稳态不再有明显的堆分配
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

    // 超时或队列已空都不产出图像，必须回超时码，
    // 否则调用方会把空帧当正常帧送去显示
    if (state == false || workImageQueue.empty()) {
        return GETIAMGE_TIMEOUT;
    }

    // 帧的所有权在此转移给调用方，缓冲要等它深拷贝完成后经 Recycle 归还：
    // 若在此就地归还，生产端可能立刻覆写消费者仍在读取的图像
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
