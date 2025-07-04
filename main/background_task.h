#ifndef BACKGROUND_TASK_H
#define BACKGROUND_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>
#include <list>
#include <condition_variable>
#include <atomic>

/**
 * @brief 管理后台任务的类，用于在单独的线程中执行异步任务。
 *
 * 该类提供了一个简单的接口，用于在后台线程中调度和执行任务。用户可以通过调用 `Schedule` 方法来添加新的任务，
 * 并通过 `WaitForCompletion` 方法等待所有任务完成。构造函数允许指定后台线程的堆栈大小。
 *
 * 示例：
 *
 * @param stack_size 后台线程的堆栈大小，默认为 8192 字节。
 */
class BackgroundTask {
public:
    BackgroundTask(uint32_t stack_size = 4096 * 2);
    ~BackgroundTask();

    /**
     * @brief 调度一个任务到后台线程执行。
     * @param callback 需要在后台线程中执行的任务。
     */
    void Schedule(std::function<void()> callback);

    /**
     * @brief 等待所有已调度的任务完成。
     */
    void WaitForCompletion();

private:
    // 互斥锁，用于保护共享资源
    std::mutex mutex_;
    // 主任务列表，存储待执行的任务
    std::list<std::function<void()>> main_tasks_;
    // 条件变量，用于线程同步
    std::condition_variable condition_variable_;
    // 后台任务句柄，用于管理后台任务
    TaskHandle_t background_task_handle_ = nullptr;
    // 原子变量，用于记录当前活跃的任务数
    std::atomic<size_t> active_tasks_{0};

    /**
     * @brief 后台任务循环，用于执行所有已调度的任务。
     */
    void BackgroundTaskLoop();
};

#endif
