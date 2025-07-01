#include "wifi_board.h"
#include "ml307_board.h"
#include "dual_network_board.h" 
#include "audio_codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"

// [修改] 移除了节能模式相关的头文件
#include "power_manager.h"
#include "display/oled_display.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include "driver/gpio.h"


#define TAG "MovecallMojiESP32S3_OLED" 

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

class MovecallMojiESP32S3 : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Button internal_button_;
    Button wifi_switch_button_;
    
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;

    // [修改] 只保留 PowerManager
    PowerManager* power_manager_ = nullptr;
    
    // [修改] InitializePowerManager 现在只负责创建对象
    void InitializePowerManager() {
        power_manager_ = new PowerManager();
    }

    // [删除] 整个 InitializePowerSaveTimer 函数已被移除

    void InitUart() {
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

    void InitializeI2cBus() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = { .enable_internal_pullup = 1 },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
        ESP_LOGI(TAG, "I2C总线初始化完成");
    }

    void InitializeSsd1306Display() {
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .scl_speed_hz = 400 * 1000,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(i2c_bus_, &io_config, &panel_io_));
        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;
        esp_lcd_panel_ssd1306_config_t ssd1306_config = { .height = static_cast<uint8_t>(DISPLAY_HEIGHT) };
        panel_config.vendor_config = &ssd1306_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));
        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
            {&font_puhui_14_1, &font_awesome_14_1});
    }

    void InitializeButtons() {
        
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            app.ToggleChatState();
        });

        internal_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            app.ChangeChatState();
        });

        wifi_switch_button_.OnLongPress([this]() {
            ESP_LOGI(TAG, "WiFi切换按键长按");
            SwitchNetworkType();
        });
    }

    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker")); 
        thing_manager.AddThing(iot::CreateThing("Screen"));
        thing_manager.AddThing(iot::CreateThing("BluetoothControl"));  
        thing_manager.AddThing(iot::CreateThing("Battery"));
    }

public:
    MovecallMojiESP32S3() : 
        DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, 4096),
        boot_button_(BOOT_BUTTON_GPIO),
        internal_button_(INTERNAL_BUTTON_GPIO),
        wifi_switch_button_(NETWORK_SWITCH_BUTTON_GPIO) {
        // [修改] 移除了对节能模式的初始化调用
        InitializePowerManager();
        InitializeI2cBus();
        InitializeSsd1306Display();
        InitUart();
        InitializeButtons();
        InitializeIot();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    // 保留 GetBatteryLevel 函数，用于向上层提供电量数据
    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (!power_manager_) {
            return false;
        }
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        level = power_manager_->GetBatteryLevel();
        return true;
    }
    
    // [删除] SetPowerSaveMode 函数已被移除
};

DECLARE_BOARD(MovecallMojiESP32S3);