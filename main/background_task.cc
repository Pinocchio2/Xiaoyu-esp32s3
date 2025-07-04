#include "background_task.h"

#include <esp_log.h>
#include <esp_task_wdt.h>

#define TAG "BackgroundTask"

BackgroundTask::BackgroundTask(uint32_t stack_size) {
    // 创建一个后台任务
    xTaskCreate([](void* arg) {
        // 将参数转换为BackgroundTask指针
        BackgroundTask* task = (BackgroundTask*)arg;
        // 调用BackgroundTaskLoop函数
        task->BackgroundTaskLoop();
    }, "background_task", stack_size, this, 2, &background_task_handle_);
}

BackgroundTask::~BackgroundTask() {
    // 如果background_task_handle_不为空，则删除任务
    if (background_task_handle_ != nullptr) {
        vTaskDelete(background_task_handle_);
    }
}

void BackgroundTask::Schedule(std::function<void()> callback) {
    // 加锁，防止多线程同时访问
    std::lock_guard<std::mutex> lock(mutex_);
    // 如果活跃任务数大于等于30
    if (active_tasks_ >= 30) {
        // 获取内部SRAM的空闲大小
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        // 如果空闲大小小于10000
        if (free_sram < 10000) {
            // 打印警告日志
            ESP_LOGW(TAG, "active_tasks_ == %u, free_sram == %u", active_tasks_.load(), free_sram);
        }
    }
    // 活跃任务数加1
    active_tasks_++;
    // 将回调函数加入主任务队列
    main_tasks_.emplace_back([this, cb = std::move(callback)]() {
        // 执行回调函数
        cb();
        {
            // 加锁，防止多线程同时访问
            std::lock_guard<std::mutex> lock(mutex_);
            // 活跃任务数减1
            active_tasks_--;
            // 如果主任务队列为空且活跃任务数为0
            if (main_tasks_.empty() && active_tasks_ == 0) {
                // 通知所有等待的线程
                condition_variable_.notify_all();
            }
        }
    });
    // 通知所有等待的线程
    condition_variable_.notify_all();
}

void BackgroundTask::WaitForCompletion() {
    // 创建一个std::unique_lock对象，用于锁定mutex_
    std::unique_lock<std::mutex> lock(mutex_);
    // 等待condition_variable_的条件变量满足，即main_tasks_为空且active_tasks_为0
    condition_variable_.wait(lock, [this]() {
        return main_tasks_.empty() && active_tasks_ == 0;
    });
}

void BackgroundTask::BackgroundTaskLoop() {
    // 打印日志，表示后台任务已启动
    ESP_LOGI(TAG, "background_task started");
    while (true) {
        // 创建一个互斥锁，用于保护共享资源
        std::unique_lock<std::mutex> lock(mutex_);
        // 等待条件变量，直到主任务列表不为空
        condition_variable_.wait(lock, [this]() { return !main_tasks_.empty(); });
        
        // 将主任务列表中的任务移动到tasks列表中
        std::list<std::function<void()>> tasks = std::move(main_tasks_);
        // 解锁互斥锁
        lock.unlock();

        // 遍历tasks列表，执行每个任务
        for (auto& task : tasks) {
            task();
        }
    }
}
