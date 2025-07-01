#pragma once
#include <vector>
#include <functional>
#include <numeric>
#include <esp_log.h>

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
    int ticks_ = 0;
    const int kBatteryAdcInterval = 60;
    const int kBatteryAdcDataCount = 3;
    const int kLowBatteryLevel = 20;

    adc_oneshot_unit_handle_t adc_handle_;

    int previous_average_adc_ = -1;

    void CheckBatteryStatus() {
        if (adc_values_.size() < kBatteryAdcDataCount) {
            ReadBatteryAdcData();
            return;
        }

        ticks_++;
        if (ticks_ % kBatteryAdcInterval == 0) {
            ReadBatteryAdcData();
        }
    }

    void ReadBatteryAdcData() {
        int adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, ADC_CHANNEL_2, &adc_value));
        
        adc_values_.push_back(adc_value);
        if (adc_values_.size() > kBatteryAdcDataCount) {
            adc_values_.erase(adc_values_.begin());
        }
        uint32_t average_adc = std::accumulate(adc_values_.begin(), adc_values_.end(), 0) / adc_values_.size();
        
        if (previous_average_adc_ != -1) {
            if (average_adc > previous_average_adc_ + 2) {
                is_charging_ = true;
            } 
            else if (average_adc < previous_average_adc_ - 2) {
                is_charging_ = false;
            }
        }
        previous_average_adc_ = average_adc;

        const struct {
            uint16_t adc;
            uint8_t level;
        } levels[] = {
            {1985, 0}, {2079, 20}, {2141, 40}, {2296, 60}, {2420, 80}, {2606, 100}
        };

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

        if (adc_values_.size() >= kBatteryAdcDataCount) {
            bool new_low_battery_status = battery_level_ <= kLowBatteryLevel;
            if (new_low_battery_status != is_low_battery_) {
                is_low_battery_ = new_low_battery_status;
                if (on_low_battery_status_changed_) { on_low_battery_status_changed_(is_low_battery_); }
            }
        }
        ESP_LOGI("PowerManager", "average: %ld level: %ld%% charging: %s", average_adc, battery_level_, is_charging_ ? "true" : "false");
    }

public:
    PowerManager() {
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) { static_cast<PowerManager*>(arg)->CheckBatteryStatus(); },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "battery_check_timer",
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000));

        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));
        
        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, ADC_CHANNEL_2, &chan_config));
    }

    ~PowerManager() {
        if (timer_handle_) { esp_timer_stop(timer_handle_); esp_timer_delete(timer_handle_); }
        if (adc_handle_) { adc_oneshot_del_unit(adc_handle_); }
    }

    bool IsCharging() {
        return is_charging_;
    }

    bool IsDischarging() { return !is_charging_; }
    uint8_t GetBatteryLevel() { return battery_level_; }
    void OnLowBatteryStatusChanged(std::function<void(bool)> callback) { on_low_battery_status_changed_ = callback; }
    void OnChargingStatusChanged(std::function<void(bool)> callback) { on_charging_status_changed_ = callback; }
};