#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <font_emoji.h>

#include <atomic>

class LcdDisplay : public Display {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    
    lv_draw_buf_t draw_buf_;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;

    DisplayFonts fonts_;

    void SetupUI();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

protected:
    // 添加protected构造函数
    LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, DisplayFonts fonts)
        : panel_io_(panel_io), panel_(panel), fonts_(fonts) {}
    
public:
    ~LcdDisplay();
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetIcon(const char* icon) override;
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    virtual void SetChatMessage(const char* role, const char* content) override; 
#endif  

    // Add theme switching function
    virtual void SetTheme(const std::string& theme_name) override;
    
    // 添加获取容器的方法
    lv_obj_t* GetContainer() { return container_; }
};  

// RGB LCD显示器
class RgbLcdDisplay : public LcdDisplay {
public:
    RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
    
    // 实现基类的纯虚函数 - 内联实现
    virtual bool PlayAnimation(const Animation& animation) override {
        // RGB LCD显示器主要用于常规UI显示，不支持复杂动画
        ESP_LOGW("RgbLcdDisplay", "RgbLcdDisplay does not support animation playback");
        return false;
    }
};

// MIPI LCD显示器
class MipiLcdDisplay : public LcdDisplay {
public:
    MipiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
    
    // 实现基类的纯虚函数 - 内联实现
    virtual bool PlayAnimation(const Animation& animation) override {
        // MIPI LCD显示器主要用于常规UI显示，不支持复杂动画
        ESP_LOGW("MipiLcdDisplay", "MipiLcdDisplay does not support animation playback");
        return false;
    }
};

// SPI LCD显示器
class SpiLcdDisplay : public LcdDisplay {
public:
    SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
    
    // 实现基类的纯虚函数 - 内联实现
    virtual bool PlayAnimation(const Animation& animation) override {
        // SPI LCD显示器主要用于常规UI显示，不支持复杂动画
        ESP_LOGW("SpiLcdDisplay", "SpiLcdDisplay does not support animation playback");
        return false;
    }
};

// QSPI LCD显示器
class QspiLcdDisplay : public LcdDisplay {
public:
    QspiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
    
    // 实现基类的纯虚函数 - 内联实现
    virtual bool PlayAnimation(const Animation& animation) override {
        // QSPI LCD显示器主要用于常规UI显示，不支持复杂动画
        ESP_LOGW("QspiLcdDisplay", "QspiLcdDisplay does not support animation playback");
        return false;
    }
};

// MCU8080 LCD显示器
class Mcu8080LcdDisplay : public LcdDisplay {
public:
    Mcu8080LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                      int width, int height, int offset_x, int offset_y,
                      bool mirror_x, bool mirror_y, bool swap_xy,
                      DisplayFonts fonts);
    
    // 实现基类的纯虚函数 - 内联实现
    virtual bool PlayAnimation(const Animation& animation) override {
        // MCU8080 LCD显示器主要用于常规UI显示，不支持复杂动画
        ESP_LOGW("Mcu8080LcdDisplay", "Mcu8080LcdDisplay does not support animation playback");
        return false;
    }
};
#endif // LCD_DISPLAY_H
