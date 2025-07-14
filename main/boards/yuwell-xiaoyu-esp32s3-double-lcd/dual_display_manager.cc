#include "dual_display_manager.h"
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <driver/spi_common.h>

// 添加字体声明
LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

static const char* TAG = "DualDisplayManager";

DualDisplayManager::DualDisplayManager() 
    : primary_display_(nullptr), secondary_display_(nullptr) {
}

DualDisplayManager::~DualDisplayManager() {
    if (primary_display_) delete primary_display_;
    if (secondary_display_) delete secondary_display_;
}

void DualDisplayManager::Initialize() {
    // 初始化SPI总线
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.sclk_io_num = DISPLAY_CLK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    
    // 初始化主显示屏
    esp_lcd_panel_io_handle_t panel_io1;
    esp_lcd_panel_handle_t panel1;
    
    esp_lcd_panel_io_spi_config_t io_config1 = {};
    io_config1.cs_gpio_num = DISPLAY_CS_PIN;
    io_config1.dc_gpio_num = DISPLAY_DC_PIN;
    io_config1.spi_mode = DISPLAY_SPI_MODE;
    io_config1.pclk_hz = 40 * 1000 * 1000;
    io_config1.trans_queue_depth = 10;
    io_config1.lcd_cmd_bits = 8;
    io_config1.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config1, &panel_io1));
    
    esp_lcd_panel_dev_config_t panel_config1 = {};
    panel_config1.reset_gpio_num = DISPLAY_RST_PIN;
    panel_config1.rgb_ele_order = DISPLAY_RGB_ORDER;
    panel_config1.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io1, &panel_config1, &panel1));
    
    esp_lcd_panel_reset(panel1);
    esp_lcd_panel_init(panel1);
    esp_lcd_panel_disp_on_off(panel1, true);
    esp_lcd_panel_invert_color(panel1, DISPLAY_INVERT_COLOR);
    esp_lcd_panel_swap_xy(panel1, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel1, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    
    // 修复主显示屏创建
    DisplayFonts fonts = {
        .text_font = &font_puhui_16_4,
        .icon_font = &font_awesome_16_4,
        .emoji_font = font_emoji_32_init(),
    };
    
    primary_display_ = new SpiLcdDisplay(panel_io1, panel1,
                                       DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                       DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                       DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                       fonts);
    
    // 初始化副显示屏
    esp_lcd_panel_io_handle_t panel_io2;
    esp_lcd_panel_handle_t panel2;
    
    esp_lcd_panel_io_spi_config_t io_config2 = {};
    io_config2.cs_gpio_num = DISPLAY2_CS_PIN;  // 不同的CS引脚
    io_config2.dc_gpio_num = DISPLAY_DC_PIN;
    io_config2.spi_mode = DISPLAY_SPI_MODE;
    io_config2.pclk_hz = 40 * 1000 * 1000;
    io_config2.trans_queue_depth = 10;
    io_config2.lcd_cmd_bits = 8;
    io_config2.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config2, &panel_io2));
    
    esp_lcd_panel_dev_config_t panel_config2 = {};
    panel_config2.reset_gpio_num = -1;  // 不使用复位引脚
    panel_config2.rgb_ele_order = DISPLAY_RGB_ORDER;
    panel_config2.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io2, &panel_config2, &panel2));
    
    // 注释掉第二个屏幕的复位操作
    // esp_lcd_panel_reset(panel2);  // 不执行复位
    esp_lcd_panel_init(panel2);
    esp_lcd_panel_disp_on_off(panel2, true);
    esp_lcd_panel_invert_color(panel2, DISPLAY_INVERT_COLOR);
    esp_lcd_panel_swap_xy(panel2, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel2, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    
    // 修复副显示屏创建
    secondary_display_ = new SpiLcdDisplay(panel_io2, panel2,
                                         DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                         DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                         DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                         fonts);
}

void DualDisplayManager::ShowOnBoth(const char* message) {
    if (primary_display_) primary_display_->SetStatus(message);
    if (secondary_display_) secondary_display_->SetStatus(message);
}

void DualDisplayManager::ShowOnPrimary(const char* message) {
    if (primary_display_) primary_display_->SetStatus(message);
}

void DualDisplayManager::ShowOnSecondary(const char* message) {
    if (secondary_display_) secondary_display_->SetStatus(message);
}

void DualDisplayManager::SetDifferentContent(const char* primary_content, const char* secondary_content) {
    if (primary_display_) primary_display_->SetStatus(primary_content);
    if (secondary_display_) secondary_display_->SetStatus(secondary_content);
}

void DualDisplayManager::SetMirrorMode(bool enable) {
    // 镜像模式实现 - 当enable为true时，副屏显示与主屏相同的内容
    // 这里可以设置一个标志位，在其他显示方法中根据此标志决定是否同步显示
    ESP_LOGI(TAG, "Mirror mode %s", enable ? "enabled" : "disabled");
    // 具体实现可根据需求扩展
}

// 简单的双屏内容测试
void DualDisplayManager::TestDifferentContent() {
    ESP_LOGI(TAG, "开始双屏不同内容测试...");
    
    // 检查显示屏是否初始化成功
    if (!primary_display_ || !secondary_display_) {
        ESP_LOGE(TAG, "显示屏未正确初始化");
        return;
    }
    
    // 测试1: 显示基本的不同内容
    ESP_LOGI(TAG, "测试1: 基本不同内容");
    SetDifferentContent("主屏幕", "副屏幕");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 测试2: 显示数字内容
    ESP_LOGI(TAG, "测试2: 数字内容");
    SetDifferentContent("屏幕 1", "屏幕 2");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试3: 显示英文内容
    ESP_LOGI(TAG, "测试3: 英文内容");
    SetDifferentContent("Left Screen", "Right Screen");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试4: 显示状态信息
    ESP_LOGI(TAG, "测试4: 状态信息");
    SetDifferentContent("主界面", "状态栏");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试5: 显示时间和日期
    ESP_LOGI(TAG, "测试5: 时间日期");
    SetDifferentContent("12:34:56", "2024-01-01");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 最终显示
    SetDifferentContent("测试完成", "✓ 成功");
    ESP_LOGI(TAG, "双屏不同内容测试完成!");
}