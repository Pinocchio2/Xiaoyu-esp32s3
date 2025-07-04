#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

/**
 * @brief OledDisplay 类用于控制 OLED 显示屏，继承自 Display 类。
 *
 * 该类提供了初始化 OLED 显示屏、设置显示内容等功能。通过构造函数传入必要的参数来初始化显示屏，
 * 并提供了一些方法来设置和更新显示屏上的内容。该类还实现了锁定和解锁显示屏的功能，以确保在
 * 多线程环境下的安全操作。
 *
 * 核心功能包括：
 * - 初始化 OLED 显示屏（128x64 和 128x32 分辨率）
 * - 设置聊天消息显示
 * - 锁定和解锁显示屏
 *
 * 使用示例：
 *
 * 构造函数参数：
 * - `esp_lcd_panel_io_handle_t panel_io`：LCD 面板 IO 句柄
 * - `esp_lcd_panel_handle_t panel`：LCD 面板句柄
 * - `int width`：显示屏宽度
 * - `int height`：显示屏高度
 * - `bool mirror_x`：是否水平镜像
 * - `bool mirror_y`：是否垂直镜像
 * - `DisplayFonts fonts`：显示字体
 *
 * 特殊使用限制或潜在的副作用：
 * - 在多线程环境下使用时，请确保在操作显示屏之前调用 Lock 方法，并在操作完成后调用 Unlock 方法。
 */
class OledDisplay : public Display {
private:
    // esp_lcd_panel_io_handle_t类型的panel_io_变量
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    // esp_lcd_panel_handle_t类型的panel_变量
    esp_lcd_panel_handle_t panel_ = nullptr;

    // lv_obj_t类型的status_bar_变量
    lv_obj_t* status_bar_ = nullptr;
    // lv_obj_t类型的content_变量
    lv_obj_t* content_ = nullptr;
    // lv_obj_t类型的content_left_变量
    lv_obj_t* content_left_ = nullptr;
    // lv_obj_t类型的content_right_变量
    lv_obj_t* content_right_ = nullptr;
    // lv_obj_t类型的container_变量
    lv_obj_t* container_ = nullptr;
    // lv_obj_t类型的side_bar_变量
    lv_obj_t* side_bar_ = nullptr;

    // DisplayFonts类型的fonts_变量
    DisplayFonts fonts_;

    // 虚函数Lock，用于锁定显示，参数timeout_ms为超时时间，默认为0
    virtual bool Lock(int timeout_ms = 0) override;
    // 虚函数Unlock，用于解锁显示
    virtual void Unlock() override;

    // 函数SetupUI_128x64，用于设置128x64分辨率的UI
    void SetupUI_128x64();
    // 函数SetupUI_128x32，用于设置128x32分辨率的UI
    void SetupUI_128x32();

public:
    // 构造函数，参数为esp_lcd_panel_io_handle_t类型的panel_io，esp_lcd_panel_handle_t类型的panel，int类型的width，height，mirror_x，mirror_y，DisplayFonts类型的fonts
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, bool mirror_x, bool mirror_y,
                DisplayFonts fonts);
    // 析构函数
    ~OledDisplay();

    // 虚函数SetChatMessage，用于设置聊天消息，参数为const char*类型的role和content
    virtual void SetChatMessage(const char* role, const char* content) override;
};

#endif // OLED_DISPLAY_H
