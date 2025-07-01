#include "../thing.h"
#include "board.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>



#define TAG "BluetoothControl"

namespace iot {

class BluetoothControl : public Thing {
private:
    gpio_num_t en_gpio_ = GPIO_NUM_47;    // EN引脚连接GPIO47
    gpio_num_t rsv_gpio_ = GPIO_NUM_48;   // RSV引脚连接GPIO48
    bool bluetooth_enabled_ = false;      // 蓝牙状态变量
    
    // 初始化GPIO引脚
    void InitializeGpio() {
        // 配置EN引脚 (GPIO47)
        gpio_config_t en_conf = {};
        en_conf.intr_type = GPIO_INTR_DISABLE;      // 禁用中断
        en_conf.mode = GPIO_MODE_OUTPUT;            // 设置为输出模式
        en_conf.pin_bit_mask = (1ULL << en_gpio_);  // 设置引脚位掩码
        en_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // 禁用下拉
        en_conf.pull_up_en = GPIO_PULLUP_DISABLE;   // 禁用上拉
        gpio_config(&en_conf);                      // 配置GPIO
        
        // 配置RSV引脚 (GPIO48)
        gpio_config_t rsv_conf = {};
        rsv_conf.intr_type = GPIO_INTR_DISABLE;      // 禁用中断
        rsv_conf.mode = GPIO_MODE_OUTPUT;            // 设置为输出模式
        rsv_conf.pin_bit_mask = (1ULL << rsv_gpio_); // 设置引脚位掩码
        rsv_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // 禁用下拉
        rsv_conf.pull_up_en = GPIO_PULLUP_DISABLE;   // 禁用上拉
        gpio_config(&rsv_conf);                      // 配置GPIO
        
        // 初始状态：蓝牙休眠 (EN=1, RSV=0)
        gpio_set_level(en_gpio_, 1);
        gpio_set_level(rsv_gpio_, 0);
        bluetooth_enabled_ = false;
        
        ESP_LOGI(TAG, "GPIO %d (EN) and GPIO %d (RSV) initialized for Bluetooth control", en_gpio_, rsv_gpio_);
    }
    
    // 蓝牙休眠函数
    void BluetoothSleep() {
        gpio_set_level(en_gpio_, 1);   // EN = 1
        gpio_set_level(rsv_gpio_, 0);  // RSV = 0
        bluetooth_enabled_ = false;
        ESP_LOGI(TAG, "Bluetooth set to sleep mode (EN=1, RSV=0)");
    }
    
    // 蓝牙重启函数
    void BluetoothRestart() {
        // 重启序列：EN 1->0->1，RSV=1
        gpio_set_level(rsv_gpio_, 1);  // 先设置RSV = 1
        
        gpio_set_level(en_gpio_, 1);   // EN = 1
        vTaskDelay(pdMS_TO_TICKS(10)); // 延时10ms
        
        gpio_set_level(en_gpio_, 0);   // EN = 0
        vTaskDelay(pdMS_TO_TICKS(10)); // 延时10ms
        
        gpio_set_level(en_gpio_, 1);   // EN = 1
        
        bluetooth_enabled_ = true;
        ESP_LOGI(TAG, "Bluetooth restarted (EN=1->0->1, RSV=1)");
    }


public:
    BluetoothControl() : Thing("BluetoothControl", "蓝牙功能，可以打开或者关闭") {
        ESP_LOGI(TAG, "BluetoothControl constructor called");
        InitializeGpio();  // 初始化GPIO
        
        // 属性定义
        properties_.AddBooleanProperty("enabled", "蓝牙是否打开", [this]() -> bool {
            return bluetooth_enabled_;
        });
        
        // 方法定义
        methods_.AddMethod("TurnOnBluetooth", "打开蓝牙", ParameterList(), [this](const ParameterList& parameters) {
            ESP_LOGI(TAG, "TurnOnBluetooth method called!"); // 添加方法调用日志
            BluetoothRestart();
            
            
        });
        
        methods_.AddMethod("TurnOffBluetooth", "关闭蓝牙", ParameterList(), [this](const ParameterList& parameters) {
            ESP_LOGI(TAG, "TurnOffBluetooth method called!"); 
            BluetoothSleep();  // 执行蓝牙休眠
        methods_.AddMethod("ToggleBluetooth", "切换蓝牙状态", ParameterList(), [this](const ParameterList& parameters) {
            ESP_LOGI(TAG, "ToggleBluetooth method called!"); // 添加方法调用日志
            if (bluetooth_enabled_) {
                BluetoothSleep();   // 当前开启，则关闭
                ESP_LOGI(TAG, "Bluetooth toggled to OFF");
            } else {
                BluetoothRestart(); // 当前关闭，则开启
                ESP_LOGI(TAG, "Bluetooth toggled to ON");
            }
        });
        
        ESP_LOGI(TAG, "BluetoothControl initialized successfully"); // 添加初始化完成日志    
           
        });
    }
}; // namespace iot

}
// 注册设备到IoT框架，使其可以被自动发现和管理
DECLARE_THING(BluetoothControl);