#ifndef CAMERAIMAGEQUEUE_H
#define CAMERAIMAGEQUEUE_H

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <queue>
#include <thread>

// 取帧等待上限。该值同时决定"停止采集"的最坏响应延迟：
// 采集线程阻塞在 Take 上时，最多等这么久就会回到中断标志检查，
// 因此不宜取大值，否则点停止会卡住调用线程
#define TIME_OUT_MS 200
#define ImageQueueSize 10

class CameraImageQueue {
public:
    CameraImageQueue();
    CameraImageQueue(int maxSize);

    uint32_t Put(const cv::Mat& m);

    uint32_t Take(cv::Mat& m);

    // 调用方用完取出的帧后归还缓冲，供生产端循环复用
    void Recycle(const cv::Mat& m);

private:
    bool NotEmpty() const;

private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::queue<cv::Mat> freeImageQueue;
    std::queue<cv::Mat> workImageQueue;

    uint8_t m_queueSize;
};

#endif
