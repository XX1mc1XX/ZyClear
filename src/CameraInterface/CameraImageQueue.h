#ifndef CAMERAIMAGEQUEUE_H
#define CAMERAIMAGEQUEUE_H

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <queue>
#include <thread>

#define TIME_OUT_MS 5000
#define ImageQueueSize 10

class CameraImageQueue {
public:
    CameraImageQueue();
    CameraImageQueue(int maxSize);

    uint32_t Put(const cv::Mat& m);

    uint32_t Take(cv::Mat& m);

    bool Empty();

    bool Full();

    size_t Size();

private:
    bool isFull() const;
    bool isEmpty() const;
    bool NotFull() const;
    bool NotEmpty() const;

private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::queue<cv::Mat> freeImageQueue;
    std::queue<cv::Mat> workImageQueue;

    uint8_t m_queueSize;
    bool m_needStop;
};

#endif

