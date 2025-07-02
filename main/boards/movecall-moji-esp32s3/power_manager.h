#pragma once
#include <vector>
#include <functional>
#include <numeric>
#include <esp_log.h>
#include <cinttypes> // 包含 <cinttypes> 以使用 PRIu32 宏

#include <esp_timer.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>

class PowerManager {
private:
    esp_timer_handle_t timer_handle_;
    std::function<void(bool)> on_charging_status_changed_;
    std::function<void(bool)> on_low_battery_status_changed_;

    std::vector<uint16_t> adc_values_;
    uint32_t battery_level_ = 0;
    bool is_charging_ = false;
    bool is_low_battery_ = false;
    //int ticks_ = 0;
    // [修改] 移除 ticks_，用一个新的计数器专门管理状态报告
    int status_report_counter_ = 0;
    static const int kStatusReportIntervalSeconds = 60; // 60秒报告一次
    static constexpr uint32_t kCheckIntervalUs = 500000; // [新增] 检测周期设置为500ms


    const int kBatteryAdcDataCount = 3;
    const int kLowBatteryLevel = 20;

    adc_oneshot_unit_handle_t adc_handle_;

    int previous_average_adc_ = -1;

    void UpdateBatteryStatus() {
        // 每次调用都直接读取数据
        ReadBatteryAdcData();

        // [修改] 使用新的计数器管理低频的状态报告
        // 计算定时器触发多少次才相当于指定的秒数
        constexpr int report_ticks_threshold = kStatusReportIntervalSeconds * 1000000 / kCheckIntervalUs;
        status_report_counter_++;
        if (status_report_counter_ >= report_ticks_threshold) {
            status_report_counter_ = 0;
            ESP_LOGI("PowerManager", "=== 电池状态报告 ===");
            ESP_LOGI("PowerManager", "电池电量: %" PRIu32 "%%", battery_level_);
            ESP_LOGI("PowerManager", "充电状态: %s", is_charging_ ? "充电中" : "未充电");
            ESP_LOGI("PowerManager", "低电量状态: %s", is_low_battery_ ? "是" : "否");
            ESP_LOGI("PowerManager", "平均ADC值: %d", previous_average_adc_);
            ESP_LOGI("PowerManager", "========================");
        }
    }


private:
    
    int charging_stable_count_ = 0;
    int discharging_stable_count_ = 0;
    const int kChargingStableThreshold = 2;  
    const int kChargingAdcThreshold = 10; 
    void ReadBatteryAdcData() {
        int adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, ADC_CHANNEL_2, &adc_value));
        
        adc_values_.push_back(adc_value);
        if (adc_values_.size() > kBatteryAdcDataCount) {
            adc_values_.erase(adc_values_.begin());
        }
        uint32_t average_adc = std::accumulate(adc_values_.begin(), adc_values_.end(), 0) / adc_values_.size();
        
        // *** 添加调试日志 ***
        // 使用 PRIu32 宏来打印 uint32_t 类型的 average_adc
        ESP_LOGI("PowerManager", "ADC读取: 当前值=%d, 平均值=%" PRIu32 ", 上次平均值=%d", 
                 adc_value, average_adc, previous_average_adc_);
        
        // 保存之前的充电状态用于比较
        bool previous_charging_state = is_charging_;
        
        if (previous_average_adc_ != -1) {
            int adc_diff = average_adc - previous_average_adc_;
            ESP_LOGI("PowerManager", "ADC变化: %+d (阈值: ±%d)", adc_diff, kChargingAdcThreshold);
            
            // 检测到电压上升趋势
            if (average_adc > previous_average_adc_ + kChargingAdcThreshold) {
                charging_stable_count_++;
                discharging_stable_count_ = 0;
                ESP_LOGI("PowerManager", "检测到充电趋势，稳定计数: %d/%d", 
                         charging_stable_count_, kChargingStableThreshold);
                
                // 连续检测到充电趋势才改变状态
                if (charging_stable_count_ >= kChargingStableThreshold) {
                    if (!is_charging_) {
                        is_charging_ = true;
                        ESP_LOGI("PowerManager", "*** 充电状态变更: 开始充电 ***");
                    }
                }
            } 
            // 检测到电压下降趋势
            else if (average_adc < previous_average_adc_ - kChargingAdcThreshold) {
                discharging_stable_count_++;
                charging_stable_count_ = 0;
                ESP_LOGI("PowerManager", "检测到放电趋势，稳定计数: %d/%d", 
                         discharging_stable_count_, kChargingStableThreshold);
                
                // 连续检测到放电趋势才改变状态
                if (discharging_stable_count_ >= kChargingStableThreshold) {
                    if (is_charging_) {
                        is_charging_ = false;
                        ESP_LOGI("PowerManager", "*** 充电状态变更: 停止充电 ***");
                    }
                }
            }
            // ADC值变化不大，重置计数器
            else {
                charging_stable_count_ = 0;
                discharging_stable_count_ = 0;
            }
        }
        previous_average_adc_ = average_adc;
        
        // *** 当充电状态发生变化时调用回调函数 ***
        if (previous_charging_state != is_charging_) {
            ESP_LOGI("PowerManager", "=== 充电状态变化通知: %s -> %s ===", 
                      previous_charging_state ? "充电中" : "未充电",
                      is_charging_ ? "充电中" : "未充电");
            
            if (on_charging_status_changed_) {
                on_charging_status_changed_(is_charging_);
            } else {
                ESP_LOGW("PowerManager", "警告: 充电状态回调函数未设置!");
            }
        }

        const struct {
            uint16_t adc;
            uint8_t level;
        } levels[] = {
            {2500, 0}, 
            {2520, 20}, 
            {2550, 40}, 
            {2590, 60}, 
            {2600, 80}, 
            {2606, 100}
        };

        // 注意: old_battery_level是uint32_t, battery_level_也是uint32_t,
        // 但在日志中你将它们作为百分比打印, 所以类型转换是安全的。
        uint32_t old_battery_level = battery_level_;
        if (average_adc < levels[0].adc) {
            battery_level_ = 0;
        } else if (average_adc >= levels[5].adc) {
            battery_level_ = 100;
        } else {
            for (int i = 0; i < 5; i++) {
                if (average_adc >= levels[i].adc && average_adc < levels[i+1].adc) {
                    float ratio = static_cast<float>(average_adc - levels[i].adc) / (levels[i+1].adc - levels[i].adc);
                    battery_level_ = levels[i].level + ratio * (levels[i+1].level - levels[i].level);
                    break;
                }
            }
        }
        
        // *** 添加电池电量变化日志 ***
        if (old_battery_level != battery_level_) {
            // 使用 PRIu32 宏来打印 uint32_t 类型的 old_battery_level 和 battery_level_
            ESP_LOGI("PowerManager", "电池电量变化: %" PRIu32 "%% -> %" PRIu32 "%% (充电状态: %s)", 
                     old_battery_level, battery_level_, is_charging_ ? "充电中" : "未充电");
        }
    }

public:
    // 构造函数
    PowerManager() {
        // 创建定时器参数
        esp_timer_create_args_t timer_args = {
            // 回调函数
            .callback = [](void* arg) { static_cast<PowerManager*>(arg)->UpdateBatteryStatus(); },
            // 参数
            .arg = this,
            // 分发方法
            .dispatch_method = ESP_TIMER_TASK,
            // 定时器名称
            .name = "battery_update_timer",
        };

        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
        // [修改] 直接将定时器周期设为想要的检测周期，例如500ms
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, kCheckIntervalUs)); 

        // 创建ADC单次测量单元参数
        adc_oneshot_unit_init_cfg_t init_config = {
            // 单元ID
            .unit_id = ADC_UNIT_1,
        };
        // 创建ADC单次测量单元
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));
        
        // 创建ADC单次测量通道参数
        adc_oneshot_chan_cfg_t chan_config = {
            // 电压衰减
            .atten = ADC_ATTEN_DB_12,
            // 位宽
            .bitwidth = ADC_BITWIDTH_12,
        };
        // 配置ADC单次测量通道
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, ADC_CHANNEL_2, &chan_config));
    }

    ~PowerManager() {
        // 如果timer_handle_不为空，则停止定时器并删除定时器
        if (timer_handle_) { esp_timer_stop(timer_handle_); esp_timer_delete(timer_handle_); }
        // 如果adc_handle_不为空，则删除adc单元
        if (adc_handle_) { adc_oneshot_del_unit(adc_handle_); }
    }

    bool IsCharging() {
        return is_charging_;
    }

    // 判断是否在放电
    bool IsDischarging() { return !is_charging_; }
    // 获取电池电量
    uint8_t GetBatteryLevel() { return static_cast<uint8_t>(battery_level_); } // 建议返回时做一次类型转换
    // 当低电量状态改变时调用回调函数
    void OnLowBatteryStatusChanged(std::function<void(bool)> callback) { on_low_battery_status_changed_ = callback; }
    // 当充电状态改变时调用回调函数
    void OnChargingStatusChanged(std::function<void(bool)> callback) { on_charging_status_changed_ = callback; }
};