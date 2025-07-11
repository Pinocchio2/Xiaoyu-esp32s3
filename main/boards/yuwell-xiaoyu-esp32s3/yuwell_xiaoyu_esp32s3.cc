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
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>



#include <driver/spi_common.h>
#include "display/lcd_display.h"

//#include <esp_lcd_panel_vendor.h> // SSD1306驱动

#include "driver/gpio.h"

////////////////////////////////////////新增多种屏幕支持///////////////////////////
#if defined(LCD_TYPE_ILI9341_SERIAL)
#include "esp_lcd_ili9341.h"
#endif

#if defined(LCD_TYPE_GC9A01_SERIAL)
#include "esp_lcd_gc9a01.h"
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb1, (uint8_t[]){0x80}, 1, 0},
    {0xb2, (uint8_t[]){0x27}, 1, 0},
    {0xb3, (uint8_t[]){0x13}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x05}, 1, 0},
    {0xac, (uint8_t[]){0xc8}, 1, 0},
    {0xab, (uint8_t[]){0x0f}, 1, 0},
    {0x3a, (uint8_t[]){0x05}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x08}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xea, (uint8_t[]){0x02}, 1, 0},
    {0xe8, (uint8_t[]){0x2A}, 1, 0},
    {0xe9, (uint8_t[]){0x47}, 1, 0},
    {0xe7, (uint8_t[]){0x5f}, 1, 0},
    {0xc6, (uint8_t[]){0x21}, 1, 0},
    {0xc7, (uint8_t[]){0x15}, 1, 0},
    {0xf0,
    (uint8_t[]){0x1D, 0x38, 0x09, 0x4D, 0x92, 0x2F, 0x35, 0x52, 0x1E, 0x0C,
                0x04, 0x12, 0x14, 0x1f},
    14, 0},
    {0xf1,
    (uint8_t[]){0x16, 0x40, 0x1C, 0x54, 0xA9, 0x2D, 0x2E, 0x56, 0x10, 0x0D,
                0x0C, 0x1A, 0x14, 0x1E},
    14, 0},
    {0xf4, (uint8_t[]){0x00, 0x00, 0xFF}, 3, 0},
    {0xba, (uint8_t[]){0xFF, 0xFF}, 2, 0},
};
#endif


#define TAG "yuwell-xiaoyu-esp32s3" 

// 声明您要用于OLED的字体
LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4); 

// 移除了 CustomLcdDisplay 类，因为我们将直接使用 OledDisplay 或其父类 Display

class YuwellXiaoyuEsp32S3Board : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_; // 统一的I2C总线句柄，供显示屏和音频编解码器共用
    Button boot_button_;
    //增加两个按键
    Button internal_button_;
    Button wifi_switch_button_;
    
    //Button button2_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    LcdDisplay* display_;;
    //////////////////////////////////////屏幕相关/////////////////////////////
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
#if defined(LCD_TYPE_ILI9341_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_GC9A01_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds,
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
        };        
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif
        
        esp_lcd_panel_reset(panel);
 

        esp_lcd_panel_init(panel);
        esp_lcd_panel_disp_on_off(panel, true); // 打开显示
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
#ifdef  LCD_TYPE_GC9A01_SERIAL
        panel_config.vendor_config = &gc9107_vendor_config;
#endif
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_16_4,
                                        .icon_font = &font_awesome_16_4,
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
                                        .emoji_font = font_emoji_32_init(),
#else
                                        .emoji_font = DISPLAY_HEIGHT >= 240 ? font_emoji_64_init() : font_emoji_32_init(),
#endif
                                    });
    }

////////////////////////////////////////////////////end/////////////////////////////////////

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

    // void InitializeSsd1306Display() {
    //     // SSD1306 config
    //     esp_lcd_panel_io_i2c_config_t io_config = {
    //         .dev_addr = 0x3C,
    //         .on_color_trans_done = nullptr,
    //         .user_ctx = nullptr,
    //         .control_phase_bytes = 1,
    //         .dc_bit_offset = 6,
    //         .lcd_cmd_bits = 8,
    //         .lcd_param_bits = 8,
    //         .flags = {
    //             .dc_low_on_data = 0,
    //             .disable_control_phase = 0,
    //         },
    //         .scl_speed_hz = 400 * 1000,
    //     };

   

    //  void InitializeSpi() {
    //     spi_bus_config_t buscfg = {};
    //     buscfg.mosi_io_num = LCD_MOSI_PIN;
    //     buscfg.miso_io_num = GPIO_NUM_NC;
    //     buscfg.sclk_io_num = LCD_SCLK_PIN;
    //     buscfg.quadwp_io_num = GPIO_NUM_NC;
    //     buscfg.quadhd_io_num = GPIO_NUM_NC;
    //     buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    //     ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
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
        // Screen Thing 
         thing_manager.AddThing(iot::CreateThing("Screen"));
         thing_manager.AddThing(iot::CreateThing("BluetoothControl"));   
    }
public:
    YuwellXiaoyuEsp32S3Board() : 
        DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, 4096),
        boot_button_(BOOT_BUTTON_GPIO),
        internal_button_(INTERNAL_BUTTON_GPIO), // 新增内部按钮
        wifi_switch_button_(NETWORK_SWITCH_BUTTON_GPIO) {  
        InitializeI2cBus();          // 首先初始化I2C总线
        InitializeSpi();
        InitializeLcdDisplay();
        //InitializeSsd1306Display();  // 然后初始化显示屏
        InitUart();                  // 初始化串口
        InitializeButtons();         // 初始化按钮
        InitializeIot();            // 初始化IOT
        // 初始化背光
         if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            //GetBacklight()->RestoreBrightness();
            GetBacklight()->SetBrightness(100);
        }             
    }

    virtual AudioCodec* GetAudioCodec() override {
        // 使用共享的I2C总线
        static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    // 获取背光
    virtual Backlight* GetBacklight() override {
        // 如果背光引脚不为NC
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            // 静态创建一个PwmBacklight对象
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            // 返回背光对象
            return &backlight;
        }
        // 否则返回空指针
        return nullptr;
    }
};
   


DECLARE_BOARD(YuwellXiaoyuEsp32S3Board);