#include "wifi_board.h"
#include "ml307_board.h"
#include "dual_network_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include "display/oled_display.h"
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include "dual_display_manager.h"
#include <cJSON.h>

// LVGL 图片声明
LV_IMG_DECLARE(biyan_img);
LV_IMG_DECLARE(zhenyan_img);

#define TAG "yuwell-xiaoyu-esp32s3-double-lcd"

// --- 类的声明部分 ---
class YuwellXiaoyuEsp32S3BoardDoubleLcd : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Button internal_button_;
    Button wifi_switch_button_;
    DualDisplayManager dual_display_manager_;

    // 私有初始化函数的声明
    void InitUart();
    void InitializeI2cBus();
    void InitializeButtons();
    void InitializeIot();

public:
    // 构造函数
    YuwellXiaoyuEsp32S3BoardDoubleLcd();

    // 动画测试函数的声明
    void TestEyeAnimation();

    // 重写的虚函数声明
    virtual Display* GetDisplay() override;
    virtual AudioCodec* GetAudioCodec() override;

    // 获取显示管理器的函数声明
    DualDisplayManager* GetDualDisplayManager();
};

// --- 类的成员函数定义部分 ---

// 构造函数实现
YuwellXiaoyuEsp32S3BoardDoubleLcd::YuwellXiaoyuEsp32S3BoardDoubleLcd() : 
    DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, 4096),
    boot_button_(BOOT_BUTTON_GPIO),
    internal_button_(INTERNAL_BUTTON_GPIO),
    wifi_switch_button_(NETWORK_SWITCH_BUTTON_GPIO) 
{  
    InitializeI2cBus();
    dual_display_manager_.Initialize();
    InitUart();
    InitializeButtons();
    InitializeIot();

    ESP_LOGI(TAG, "Board initialization complete. Starting eye animation test...");
    TestEyeAnimation();
}

// 初始化串口
void YuwellXiaoyuEsp32S3BoardDoubleLcd::InitUart() {
    ESP_LOGI(TAG, "初始化串口，用于血压数据接收");
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, BT_TX_PIN, BT_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    const int uart_buffer_size = 1024;
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, uart_buffer_size * 2, 0, 0, NULL, 0));
    ESP_LOGI(TAG, "串口初始化完成 - TX: GPIO%d, RX: GPIO%d, 波特率: %d", BT_TX_PIN, BT_RX_PIN, uart_config.baud_rate);
}

// 初始化I2C总线
void YuwellXiaoyuEsp32S3BoardDoubleLcd::InitializeI2cBus() {
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
        .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {.enable_internal_pullup = 1},
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    ESP_LOGI(TAG, "I2C总线初始化完成");
}

// 初始化按钮
void YuwellXiaoyuEsp32S3BoardDoubleLcd::InitializeButtons() {
    boot_button_.OnClick([this]() {
        auto& app = Application::GetInstance();
        app.ToggleChatState();
    });

    internal_button_.OnClick([this]() {
        auto& app = Application::GetInstance();
        app.ChangeChatState();
    });
    
    wifi_switch_button_.OnClick([this]() {
        ESP_LOGI(TAG, "key1 (wifi_switch_button) clicked, toggling Bluetooth.");
        cJSON *command = cJSON_CreateObject();
        cJSON_AddStringToObject(command, "name", "BluetoothControl");
        cJSON_AddStringToObject(command, "method", "ToggleBluetooth");
        cJSON_AddItemToObject(command, "parameters", cJSON_CreateObject());
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.Invoke(command);
        cJSON_Delete(command);
    });

    wifi_switch_button_.OnLongPress([this]() {
        ESP_LOGI(TAG, "WiFi切换按键长按");
        SwitchNetworkType();
    });
}

// 初始化物联网设备
void YuwellXiaoyuEsp32S3BoardDoubleLcd::InitializeIot() {
    auto& thing_manager = iot::ThingManager::GetInstance();
    thing_manager.AddThing(iot::CreateThing("Speaker"));
    thing_manager.AddThing(iot::CreateThing("Screen"));
    thing_manager.AddThing(iot::CreateThing("BluetoothControl"));
}

// 动画测试函数
void YuwellXiaoyuEsp32S3BoardDoubleLcd::TestEyeAnimation() {
    ESP_LOGI(TAG, "开始双屏眼睛动画测试...");
    
    DualDisplayManager* display_manager = GetDualDisplayManager();
    if (!display_manager) {
        ESP_LOGE(TAG, "Display manager not initialized!");
        return;
    }

    for (int i = 0; i < 5; ++i) {
        ESP_LOGI(TAG, "动画循环: %d - 左闭右睁", i + 1);
        display_manager->SetImage(true, &biyan_img);
        display_manager->SetImage(false, &zhenyan_img);
        vTaskDelay(pdMS_TO_TICKS(1500));

        ESP_LOGI(TAG, "动画循环: %d - 左睁右闭", i + 1);
        display_manager->SetImage(true, &zhenyan_img);
        display_manager->SetImage(false, &biyan_img);
        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    ESP_LOGI(TAG, "双屏眼睛动画测试完成!");
}

// 重写的虚函数
Display* YuwellXiaoyuEsp32S3BoardDoubleLcd::GetDisplay() {
    return dual_display_manager_.GetPrimaryDisplay();
}
    
DualDisplayManager* YuwellXiaoyuEsp32S3BoardDoubleLcd::GetDualDisplayManager() {
    return &dual_display_manager_;
}

AudioCodec* YuwellXiaoyuEsp32S3BoardDoubleLcd::GetAudioCodec() {
    static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
        AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
        AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
    return &audio_codec;
}

// --- DECLARE_BOARD 宏 ---
DECLARE_BOARD(YuwellXiaoyuEsp32S3BoardDoubleLcd);