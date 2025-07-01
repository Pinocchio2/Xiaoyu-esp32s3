#include "wifi_board.h"
#include "ml307_board.h"
#include "dual_network_board.h" //新增切换类
#include "audio_codecs/es8311_audio_codec.h"
//#include "display/lcd_ display.h"
#include "application.h"
#include "button.h"
#include "config.h" // OLED的I2C引脚和屏幕尺寸定义
#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>
#include "display/oled_display.h"   // 使用OLED的显示类
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h> // SSD1306驱动

#include "driver/gpio.h"
#include "power_manager.h"



#define TAG "MovecallMojiESP32S3_OLED" 

// 声明您要用于OLED的字体
LV_FONT_DECLARE(font_puhui_14_1);    
LV_FONT_DECLARE(font_awesome_14_1); 

// 移除了 CustomLcdDisplay 类，因为我们将直接使用 OledDisplay 或其父类 Display

class MovecallMojiESP32S3 : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_; // 统一的I2C总线句柄，供显示屏和音频编解码器共用
    Button boot_button_;
    //增加两个按键
    Button internal_button_;
    Button wifi_switch_button_;
    
    //Button button2_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;

    PowerManager* power_manager_ = nullptr;

    void InitializePowerManager() {
        power_manager_ = new PowerManager();
    }

    void InitUart() {
        ESP_LOGI(TAG, "初始化串口，用于血压数据接收");
        
        // 串口配置参数
        uart_config_t uart_config = {
            .baud_rate = 115200,                    // 波特率设置为115200
            .data_bits = UART_DATA_8_BITS,        // 8位数据位
            .parity = UART_PARITY_DISABLE,        // 无校验位
            .stop_bits = UART_STOP_BITS_1,        // 1位停止位
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // 无硬件流控
            .rx_flow_ctrl_thresh = 122,
        };
        
        // 配置串口参数
        ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
        
        // 配置串口引脚
        ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, BT_TX_PIN, BT_RX_PIN, 
                                    UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        
        // 安装串口驱动，设置缓冲区大小
        const int uart_buffer_size = 1024;
        ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, uart_buffer_size * 2, 0, 0, NULL, 0));
        
        ESP_LOGI(TAG, "串口初始化完成 - TX: GPIO%d, RX: GPIO%d, 波特率: %d", 
                BT_TX_PIN, BT_RX_PIN, uart_config.baud_rate);
    }

    // 初始化共享的I2C总线
    void InitializeI2cBus() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0, // 使用I2C0总线
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN, // SDA引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN, // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
        ESP_LOGI(TAG, "I2C总线初始化完成");
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        // 使用共享的I2C总线
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        // 创建OLED显示对象
        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
            {&font_puhui_14_1, &font_awesome_14_1});
    }


    // void InitializeButtons() {
    //     boot_button_.OnClick([this]() {
    //         auto& app = Application::GetInstance();
    //         // if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
    //         //     ResetWifiConfiguration();
    //         // }
    //         app.ToggleChatState();
    //     });
    // }

    void InitializeButtons() {
        // Boot按键：切换聊天状态
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            app.ToggleChatState();
        });

        internal_button_.OnClick([this]() {
            //ESP_LOGI(TAG, "Internal button pressed");
            auto& app = Application::GetInstance();
            app.ChangeChatState();
        });
        // *** 将 key1 的单击事件放在这里 ***
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

     virtual Display* GetDisplay() override {
        return display_;
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
        internal_button_(INTERNAL_BUTTON_GPIO), // 新增内部按钮
        wifi_switch_button_(NETWORK_SWITCH_BUTTON_GPIO) { 
        InitializePowerManager();    // 初始化电源管理 
        InitializeI2cBus();          // 首先初始化I2C总线
        InitializeSsd1306Display();  // 然后初始化显示屏
        InitUart();                  // 初始化串口
        InitializeButtons();         // 初始化按钮
        InitializeIot();             // 初始化IOT
    }

    virtual AudioCodec* GetAudioCodec() override {
        // 使用共享的I2C总线
        static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }
    
    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (!power_manager_) {
            return false;
        }
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        level = power_manager_->GetBatteryLevel();
        return true;
    }
   


};

DECLARE_BOARD(MovecallMojiESP32S3);