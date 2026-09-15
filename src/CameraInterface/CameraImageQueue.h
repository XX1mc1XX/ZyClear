#ifndef CAMERAIMAGEQUEUE_H
#define CAMERAIMAGEQUEUE_H

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <queue>
#include <thread>

// 取帧等待上限，同时决定停止采集的最坏响应延迟
#define TIME_OUT_MS 200
#define ImageQueueSize 10

class CameraImageQueue {
public:
    CameraImageQueue();
    CameraImageQueue(int maxSize);

    uint32_t Put(const cv::Mat& m);

    uint32_t Take(cv::Mat& m);

    // 归还取出的帧，供生产端复用缓冲
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
