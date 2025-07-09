#pragma once
#include <vector>
#include <functional>

#include <esp_timer.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>


/**
 * @class PowerManager
 * @brief 管理电池电量状态和充电状态的类。
 *
 * PowerManager 类用于监控电池电量状态和充电状态，并提供相应的回调函数以通知状态变化。
 * 该类通过定时器定期检查电池电量，并根据 ADC 值计算电池电量百分比。同时，它还监控充电引脚的状态，
 * 以确定设备是否正在充电。
 *
 * 核心功能包括：
 * - 定期检查电池电量状态
 * - 读取 ADC 值并计算电池电量百分比
 * - 监控充电状态
 * - 提供回调函数以通知电池电量状态和充电状态的变化
 *
 * 使用示例：
 *
 * 构造函数参数：
 * - `gpio_num_t pin`：充电状态的 GPIO 引脚编号。
 *
 * 特殊使用限制或潜在的副作用：
 * - 该类依赖于 ESP-IDF 框架，需要在 ESP32 环境中使用。
 * - 定时器的周期设置为 1 秒，可以根据需要调整。
 * - ADC 读取和电池电量计算依赖于硬件配置，可能需要根据实际硬件调整参数。
 */
class PowerManager {
private:
    // 定时器句柄
    esp_timer_handle_t timer_handle_;
    // 充电状态改变回调函数
    std::function<void(bool)> on_charging_status_changed_;
    // 低电量状态改变回调函数
    std::function<void(bool)> on_low_battery_status_changed_;

    // 充电引脚
    gpio_num_t charging_pin_ = GPIO_NUM_NC;
    // ADC 值队列
    std::vector<uint16_t> adc_values_;
    // 电池电量
    uint32_t battery_level_ = 0;
    // 是否正在充电
    bool is_charging_ = false;
    // 是否低电量
    bool is_low_battery_ = false;
    // tick 计数器
    int ticks_ = 0;
    // ADC 读取间隔
    const int kBatteryAdcInterval = 60;
    // ADC 读取数据数量
    const int kBatteryAdcDataCount = 3;
    // 低电量阈值
    const int kLowBatteryLevel = 20;

    // ADC 单元句柄
    adc_oneshot_unit_handle_t adc_handle_;

    // 检查电池状态
    void CheckBatteryStatus() {
        // Get charging status
        bool new_charging_status = gpio_get_level(charging_pin_) == 1;
        if (new_charging_status != is_charging_) {
            is_charging_ = new_charging_status;
            if (on_charging_status_changed_) {
                on_charging_status_changed_(is_charging_);
            }
            ReadBatteryAdcData();
            return;
        }

        // 如果电池电量数据不足，则读取电池电量数据
        if (adc_values_.size() < kBatteryAdcDataCount) {
            ReadBatteryAdcData();
            return;
        }

        // 如果电池电量数据充足，则每 kBatteryAdcInterval 个 tick 读取一次电池电量数据
        ticks_++;
        if (ticks_ % kBatteryAdcInterval == 0) {
            ReadBatteryAdcData();
        }
    }

    void ReadBatteryAdcData() {
        int adc_value;
        // 读取 ADC 值
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, ADC_CHANNEL_2, &adc_value));
        
        // 将 ADC 值添加到队列中
        adc_values_.push_back(adc_value);
        // 如果队列长度超过最大值，则删除第一个元素
        if (adc_values_.size() > kBatteryAdcDataCount) {
            adc_values_.erase(adc_values_.begin());
        }
        uint32_t average_adc = 0;
        // 计算队列中所有 ADC 值的平均值
        for (auto value : adc_values_) {
            average_adc += value;
        }
        average_adc /= adc_values_.size();

        // 定义电池电量区间
        const struct {
            uint16_t adc;
            uint8_t level;
        } levels[] = {
            {1970, 0},
            {2062, 20},
            {2154, 40},
            {2246, 60},
            {2338, 80},
            {2430, 100}
        };

        // 低于最低值时
        if (average_adc < levels[0].adc) {
            battery_level_ = 0;
        }
        // 高于最高值时
        else if (average_adc >= levels[5].adc) {
            battery_level_ = 100;
        } else {
            // 线性插值计算中间值
            for (int i = 0; i < 5; i++) {
                if (average_adc >= levels[i].adc && average_adc < levels[i+1].adc) {
                    float ratio = static_cast<float>(average_adc - levels[i].adc) / (levels[i+1].adc - levels[i].adc);
                    battery_level_ = levels[i].level + ratio * (levels[i+1].level - levels[i].level);
                    break;
                }
            }
        }

        // Check low battery status
        // 如果队列长度达到最大值，则检查电池电量是否低于最低值
        if (adc_values_.size() >= kBatteryAdcDataCount) {
            bool new_low_battery_status = battery_level_ <= kLowBatteryLevel;
            // 如果电池电量状态发生变化，则调用回调函数
            if (new_low_battery_status != is_low_battery_) {
                is_low_battery_ = new_low_battery_status;
                if (on_low_battery_status_changed_) {
                    on_low_battery_status_changed_(is_low_battery_);
                }
            }
        }

        // 打印 ADC 值、平均值和电池电量
        ESP_LOGI("PowerManager", "ADC value: %d average: %ld level: %ld", adc_value, average_adc, battery_level_);
    }

public:
    PowerManager(gpio_num_t pin) : charging_pin_(pin) {
        // 初始化充电引脚
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << charging_pin_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; 
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     
        gpio_config(&io_conf);

        // 创建电池电量检查定时器
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                PowerManager* self = static_cast<PowerManager*>(arg);
                self->CheckBatteryStatus();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "battery_check_timer",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000));

        // 初始化 ADC
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_2,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));
        
        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, ADC_CHANNEL_6, &chan_config));
    }

    ~PowerManager() {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }
        if (adc_handle_) {
            adc_oneshot_del_unit(adc_handle_);
        }
    }

    bool IsCharging() {
        // 如果电量已经满了，则不再显示充电中
        if (battery_level_ == 100) {
            return false;
        }
        return is_charging_;
    }

    bool IsDischarging() {
        // 没有区分充电和放电，所以直接返回相反状态
        return !is_charging_;
    }

    uint8_t GetBatteryLevel() {
        return battery_level_;
    }

    void OnLowBatteryStatusChanged(std::function<void(bool)> callback) {
        on_low_battery_status_changed_ = callback;
    }

    void OnChargingStatusChanged(std::function<void(bool)> callback) {
        on_charging_status_changed_ = callback;
    }
};
