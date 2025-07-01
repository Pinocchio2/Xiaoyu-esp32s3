#pragma once

#include <functional>

#include <esp_timer.h>
#include <esp_pm.h>

/**
 * @brief PowerSaveTimer 类用于管理设备的电源节能模式。
 *
 * 该类允许用户设置CPU的最大频率，并定义进入睡眠模式和关机前的延迟时间。
 * 用户可以通过设置回调函数来定义进入睡眠模式、退出睡眠模式和关机请求时的行为。
 * 该类还提供了一个方法来唤醒设备。
 *
 * 使用示例：
 *
 * @param cpu_max_freq 构造函数参数，表示CPU的最大频率。
 * @param seconds_to_sleep 构造函数参数，表示进入睡眠模式前的延迟时间，默认为20秒。
 * @param seconds_to_shutdown 构造函数参数，表示关机前的延迟时间，默认为-1（表示不关机）。
 */
class PowerSaveTimer {
public:
    PowerSaveTimer(int cpu_max_freq, int seconds_to_sleep = 20, int seconds_to_shutdown = -1);
    ~PowerSaveTimer();

    void SetEnabled(bool enabled);
    void OnEnterSleepMode(std::function<void()> callback);
    void OnExitSleepMode(std::function<void()> callback);
    void OnShutdownRequest(std::function<void()> callback);
    void WakeUp();

private:
    void PowerSaveCheck();

    esp_timer_handle_t power_save_timer_ = nullptr;
    bool enabled_ = false;
    bool in_sleep_mode_ = false;
    int ticks_ = 0;
    int cpu_max_freq_;
    int seconds_to_sleep_;
    int seconds_to_shutdown_;

    std::function<void()> on_enter_sleep_mode_;
    std::function<void()> on_exit_sleep_mode_;
    std::function<void()> on_shutdown_request_;
};
