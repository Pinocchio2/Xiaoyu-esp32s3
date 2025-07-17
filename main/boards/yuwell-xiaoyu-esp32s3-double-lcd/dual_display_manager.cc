#include "dual_display_manager.h"
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <driver/spi_common.h>
#include <vector>
#include <esp_lvgl_port.h>
#include "config.h" // 确保包含了包含引脚定义的头文件

// 添加字体声明 (尽管眼睛动画用不上，但保留着不会错)
LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

static const char* TAG = "DualDisplayManager";

DualDisplayManager::DualDisplayManager() 
    : primary_display_(nullptr), secondary_display_(nullptr),
      primary_img_obj_(nullptr), secondary_img_obj_(nullptr) {
}

DualDisplayManager::~DualDisplayManager() {
    // 理论上，lvgl_port_deinit()会处理显示和对象的释放
    // 但为了安全，我们手动删除
    if (primary_display_) {
        delete primary_display_;
    }
    if (secondary_display_) {
        delete secondary_display_;
    }
    // 注意：如果您的程序有明确的退出流程，最后应调用 lvgl_port_deinit()
}

void DualDisplayManager::Initialize() {
    ESP_LOGI(TAG, "Initializing SPI bus for displays...");
    // 1. 初始化SPI总线 (只需一次)
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.sclk_io_num = DISPLAY_CLK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    
    // 2. 初始化LVGL核心环境 (只需一次)
    ESP_LOGI(TAG, "Initializing LVGL core environment...");
    lv_init();
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 4; // 您可以根据需要调整任务优先级
    port_cfg.timer_period_ms = 5;
    lvgl_port_init(&port_cfg);

    DisplayFonts fonts = {
        .text_font = &font_puhui_16_4,
        .icon_font = &font_awesome_16_4,
        .emoji_font = font_emoji_32_init(),
    };
    
    // 3. 初始化并注册主显示屏 (左眼)
    //ESP_LOGI(TAG, "Initializing Primary Display...");
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
    
    primary_display_ = new SpiLcdDisplay(panel_io1, panel1, DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                       DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                       DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                       fonts);
    
    // 4. 初始化并注册副显示屏 (右眼)
    //ESP_LOGI(TAG, "Initializing Secondary Display...");
    esp_lcd_panel_io_handle_t panel_io2;
    esp_lcd_panel_handle_t panel2;
    
    esp_lcd_panel_io_spi_config_t io_config2 = {};
    io_config2.cs_gpio_num = DISPLAY2_CS_PIN;
    io_config2.dc_gpio_num = DISPLAY_DC_PIN;
    io_config2.spi_mode = DISPLAY_SPI_MODE;
    io_config2.pclk_hz = 40 * 1000 * 1000;
    io_config2.trans_queue_depth = 10;
    io_config2.lcd_cmd_bits = 8;
    io_config2.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config2, &panel_io2));
    
    esp_lcd_panel_dev_config_t panel_config2 = {};
    panel_config2.reset_gpio_num = -1; // 副屏通常不需要独立的复位引脚
    panel_config2.rgb_ele_order = DISPLAY_RGB_ORDER;
    panel_config2.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io2, &panel_config2, &panel2));
    
    esp_lcd_panel_init(panel2);
    esp_lcd_panel_disp_on_off(panel2, true);
    esp_lcd_panel_invert_color(panel2, DISPLAY_INVERT_COLOR);
    esp_lcd_panel_swap_xy(panel2, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel2, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    
    secondary_display_ = new SpiLcdDisplay(panel_io2, panel2, DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                         DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                         DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                         fonts);

    // 5. 初始化UI元素
    InitializeUI();
}


void DualDisplayManager::InitializeUI() {
    if (!primary_display_ || !secondary_display_) {
        ESP_LOGE(TAG, "Cannot initialize UI, displays are not ready.");
        return;
    }

    // 初始化主屏幕UI
    {
        DisplayLockGuard lock(primary_display_);
        lv_obj_t* prim_scr = lv_disp_get_scr_act(primary_display_->getLvDisplay());
        //lv_obj_clean(prim_scr);
        lv_obj_set_style_bg_color(prim_scr, lv_color_black(), 0);

        // 创建一个240x240的容器作为“视口”
        lv_obj_t* container = lv_obj_create(prim_scr);
        lv_obj_set_size(container, 240, 240);
        lv_obj_set_pos(container, 0, 0);
        lv_obj_set_style_radius(container, 0, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        // 在 LVGL v9 中, lv_obj_create 默认就会裁剪溢出的子对象，
        // 因此不需要额外添加裁剪标志，代码更简洁。

        // 在容器内创建图像对象
        primary_img_obj_ = lv_img_create(container);
    }

    // 初始化副屏幕UI (采用相同的逻辑)
    {
        DisplayLockGuard lock(secondary_display_);
        lv_obj_t* sec_scr = lv_disp_get_scr_act(secondary_display_->getLvDisplay());
        //lv_obj_clean(sec_scr);
        lv_obj_set_style_bg_color(sec_scr, lv_color_black(), 0);

        // 为副屏幕创建容器
        lv_obj_t* container = lv_obj_create(sec_scr);
        lv_obj_set_size(container, 240, 240);
        lv_obj_set_pos(container, 0, 0);
        lv_obj_set_style_radius(container, 0, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);

        // 在容器内创建图像对象
        secondary_img_obj_ = lv_img_create(container);
    }
    ESP_LOGI(TAG, "Dual screen UI initialized with clipping containers for eye animation.");
}


void DualDisplayManager::SetImage(bool is_primary, const void* src) {
    Display* target_display = is_primary ? primary_display_ : secondary_display_;
    lv_obj_t* target_img_obj = is_primary ? primary_img_obj_ : secondary_img_obj_;
    
    //ESP_LOGI(TAG, "Setting image for %s display", is_primary ? "primary" : "secondary");
    
    if (target_display && target_img_obj) {
        DisplayLockGuard lock(target_display);
        
        
        lv_img_set_src(target_img_obj, src);
        
       
        //    源图像尺寸为 480x480 像素。
        
        // 设置位置为(0, 0) 将显示左上角的眼睛
        lv_obj_set_pos(target_img_obj, 0, 0);

        /*
         * --- 如何显示其他眼睛 ---
         * 图像的坐标系原点(0,0)是左上角。
         *
         * 显示右上角的眼睛 (向左移动240像素): 
         * lv_obj_set_pos(target_img_obj, -240, 0);
         *
         * 显示左下角的眼睛 (向上移动240像素): 
         * lv_obj_set_pos(target_img_obj, 0, -240);
         *
         * 显示右下角的眼睛 (向左上移动240像素): 
         * lv_obj_set_pos(target_img_obj, -240, -240);
        */
        
        // 强制刷新显示
        lv_obj_invalidate(lv_obj_get_parent(target_img_obj));
    } else {
        ESP_LOGE(TAG, "Failed to set image: display or image object is null");
    }
}