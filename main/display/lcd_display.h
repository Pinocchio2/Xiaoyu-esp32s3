#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <font_emoji.h>

#include <atomic>

/**
 * @class LcdDisplay
 * @brief LcdDisplay类继承自Display类，用于管理LCD显示设备。
 *
 * LcdDisplay类提供了设置UI、锁定和解锁显示、设置情感图标、设置聊天消息以及切换主题等功能。
 * 该类通过构造函数初始化LCD面板IO句柄、面板句柄和显示字体。
 *
 * 核心功能包括：
 * - SetupUI(): 设置用户界面。
 * - Lock(int timeout_ms): 锁定显示，可选超时时间。
 * - Unlock(): 解锁显示。
 * - SetEmotion(const char* emotion): 设置显示的情感图标。
 * - SetIcon(const char* icon): 设置显示的图标。
 * - SetTheme(const std::string& theme_name): 切换显示主题。
 *
 * 使用示例：
 *
 * 构造函数参数：
 * - esp_lcd_panel_io_handle_t panel_io: LCD面板IO句柄。
 * - esp_lcd_panel_handle_t panel: LCD面板句柄。
 * - DisplayFonts fonts: 显示字体。
 *
 * 特殊使用限制或潜在的副作用：
 * - 在调用SetEmotion和SetIcon方法时，确保传入的参数是有效的字符串。
 * - 在切换主题时，确保主题名称是已注册的主题。
 */
class LcdDisplay : public Display {
protected:
    // 声明LCD面板IO句柄和LCD面板句柄
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    
    // 声明LVGL绘制缓冲区
    lv_draw_buf_t draw_buf_;
    // 声明状态栏、内容、容器、侧边栏对象
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;

    // 声明字体对象
    DisplayFonts fonts_;

    // 设置UI
    void SetupUI();
    // 覆盖Lock函数
    virtual bool Lock(int timeout_ms = 0) override;
    // 覆盖Unlock函数
    virtual void Unlock() override;

protected:
    // 添加protected构造函数
    LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, DisplayFonts fonts)
        : panel_io_(panel_io), panel_(panel), fonts_(fonts) {}
    
public:
    // 析构函数
    ~LcdDisplay();
    // 覆盖SetEmotion函数
    virtual void SetEmotion(const char* emotion) override;
    // 覆盖SetIcon函数
    virtual void SetIcon(const char* icon) override;
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    // 覆盖SetChatMessage函数
    virtual void SetChatMessage(const char* role, const char* content) override; 
#endif  

    // Add theme switching function
    virtual void SetTheme(const std::string& theme_name) override;
};

// RGB LCD显示器
/**
 * @brief RGB LCD 显示屏类，继承自 LcdDisplay 基类。
 *
 * 该类用于控制 RGB LCD 显示屏，提供基本的显示功能。
 * 主要功能包括初始化显示屏、设置显示参数等。
 *
 * 使用示例：
 *
 * 构造函数参数：
 * - `panel_io`: LCD 面板 IO 句柄，用于控制 LCD 面板的输入输出。
 * - `panel`: LCD 面板句柄，用于控制 LCD 面板。
 * - `width`: 显示屏的宽度，单位为像素。
 * - `height`: 显示屏的高度，单位为像素。
 * - `offset_x`: X 轴偏移量，用于调整显示内容的水平位置。
 * - `offset_y`: Y 轴偏移量，用于调整显示内容的垂直位置。
 * - `mirror_x`: 是否水平镜像显示内容，`true` 表示镜像，`false` 表示不镜像。
 * - `mirror_y`: 是否垂直镜像显示内容，`true` 表示镜像，`false` 表示不镜像。
 * - `swap_xy`: 是否交换 X 和 Y 轴，`true` 表示交换，`false` 表示不交换。
 * - `fonts`: 显示字体设置，用于配置显示内容的字体。
 *
 * 特殊使用限制或潜在的副作用：
 * - 确保在创建 RgbLcdDisplay 对象之前，`panel_io` 和 `panel` 已经正确初始化。
 * - 显示屏的宽度和高度应与实际硬件参数匹配，否则可能导致显示内容异常。
 */
class RgbLcdDisplay : public LcdDisplay {
public:
    // 构造函数，用于初始化RgbLcdDisplay对象
    RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// MIPI LCD显示器
/**
 * @brief MIPI LCD 显示屏类，用于控制和管理MIPI接口的LCD显示屏。
 *
 * 该类继承自LcdDisplay基类，提供对MIPI接口LCD显示屏的特定控制功能。
 * 主要用于初始化和配置MIPI LCD显示屏，包括设置显示屏的分辨率、偏移量、镜像和坐标交换等。
 *
 * 核心功能包括：
 * - 初始化MIPI LCD显示屏。
 * - 配置显示屏的基本参数，如分辨率、偏移量等。
 * - 控制显示屏的镜像和坐标交换。
 *
 * 使用示例：
 *
 * 构造函数参数：
 * - `esp_lcd_panel_io_handle_t panel_io`：LCD面板IO句柄，用于与LCD面板进行通信。
 * - `esp_lcd_panel_handle_t panel`：LCD面板句柄，用于控制LCD面板。
 * - `int width`：显示屏的宽度，单位为像素。
 * - `int height`：显示屏的高度，单位为像素。
 * - `int offset_x`：显示屏的X轴偏移量。
 * - `int offset_y`：显示屏的Y轴偏移量。
 * - `bool mirror_x`：是否在X轴上镜像显示屏内容。
 * - `bool mirror_y`：是否在Y轴上镜像显示屏内容。
 * - `bool swap_xy`：是否交换X轴和Y轴的坐标。
 * - `DisplayFonts fonts`：显示屏使用的字体设置。
 *
 * 特殊使用限制或潜在的副作用：
 * - 确保在创建MipiLcdDisplay对象之前，`panel_io`和`panel`已经正确初始化。
 * - 修改显示屏的参数可能会影响显示效果，请谨慎使用。
 */
class MipiLcdDisplay : public LcdDisplay {
public:
    // 构造函数，用于初始化MipiLcdDisplay对象
    MipiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// // SPI LCD显示器
/**
 * @brief SPI LCD 显示屏类，继承自 LcdDisplay。
 *
 * 该类用于控制通过 SPI 接口连接的 LCD 显示屏。它提供了一系列方法来配置和操作显示屏，
 * 包括设置显示屏的分辨率、偏移量、镜像和坐标轴交换等。
 *
 * 核心功能包括：
 * - 初始化 SPI LCD 显示屏。
 * - 设置显示屏的基本参数，如分辨率和偏移量。
 * - 控制显示屏的镜像和坐标轴交换。
 *
 * 使用示例：
 *
 * 构造函数参数：
 * - `panel_io`: SPI LCD 显示屏的 IO 句柄。
 * - `panel`: SPI LCD 显示屏的面板句柄。
 * - `width`: 显示屏的宽度，单位为像素。
 * - `height`: 显示屏的高度，单位为像素。
 * - `offset_x`: 显示屏的 X 轴偏移量。
 * - `offset_y`: 显示屏的 Y 轴偏移量。
 * - `mirror_x`: 是否在 X 轴上镜像显示屏内容，`true` 表示镜像，`false` 表示不镜像。
 * - `mirror_y`: 是否在 Y 轴上镜像显示屏内容，`true` 表示镜像，`false` 表示不镜像。
 * - `swap_xy`: 是否交换 X 和 Y 轴，`true` 表示交换，`false` 表示不交换。
 * - `fonts`: 显示屏使用的字体设置。
 *
 * 特殊使用限制或潜在的副作用：
 * - 确保在创建 SpiLcdDisplay 对象之前，`panel_io` 和 `panel` 已经正确初始化。
 * - 显示屏的分辨率和偏移量应根据实际显示屏的规格进行设置。
 */
class SpiLcdDisplay : public LcdDisplay {
public:
    SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// QSPI LCD显示器
/**
 * @brief QspiLcdDisplay 类用于控制基于 QSPI 接口的 LCD 显示屏。
 *
 * 该类继承自 LcdDisplay，提供对 QSPI 接口 LCD 显示屏的特定控制功能。
 * 主要用于初始化和配置显示屏，以及进行基本的显示操作。
 *
 * 核心功能包括：
 * - 初始化 QSPI 接口 LCD 显示屏
 * - 配置显示屏的基本参数，如宽度、高度、偏移量等
 * - 设置显示屏的镜像和坐标交换选项
 * - 支持多种字体显示
 *
 * 使用示例：
 *
 * 构造函数参数：
 * - panel_io: QSPI 接口的 LCD 面板 IO 句柄
 * - panel: LCD 面板句柄
 * - width: 显示屏的宽度
 * - height: 显示屏的高度
 * - offset_x: 显示屏的 X 轴偏移量
 * - offset_y: 显示屏的 Y 轴偏移量
 * - mirror_x: 是否在 X 轴上镜像显示内容
 * - mirror_y: 是否在 Y 轴上镜像显示内容
 * - swap_xy: 是否交换 X 和 Y 轴
 * - fonts: 显示字体配置
 *
 * 特殊使用限制或潜在的副作用：
 * - 确保 panel_io 和 panel 已正确初始化
 * - 显示屏的宽度和高度应与实际硬件匹配
 * - 镜像和坐标交换选项可能会影响显示内容的方向
 */
class QspiLcdDisplay : public LcdDisplay {
public:
    QspiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// MCU8080 LCD显示器
/**
 * @brief Mcu8080LcdDisplay 类用于控制基于 MCU 8080 接口的 LCD 显示器。
 *
 * 该类继承自 LcdDisplay 类，提供对 MCU 8080 接口 LCD 显示器的特定控制功能。
 * 主要用于初始化和配置 LCD 显示器，包括设置显示区域、镜像和坐标交换等。
 *
 * 核心功能包括：
 * - 初始化 LCD 显示器
 * - 设置显示器的宽度和高度
 * - 设置显示器的偏移量
 * - 设置 X 和 Y 轴的镜像
 * - 设置坐标交换
 *
 * 使用示例：
 *
 * 构造函数参数：
 * - `panel_io`: LCD 面板 IO 句柄，用于控制 LCD 显示器的数据传输。
 * - `panel`: LCD 面板句柄，用于表示特定的 LCD 显示器。
 * - `width`: 显示器的宽度，以像素为单位。
 * - `height`: 显示器的高度，以像素为单位。
 * - `offset_x`: 显示器的 X 轴偏移量。
 * - `offset_y`: 显示器的 Y 轴偏移量。
 * - `mirror_x`: 是否在 X 轴上镜像显示内容，`true` 表示镜像，`false` 表示不镜像。
 * - `mirror_y`: 是否在 Y 轴上镜像显示内容，`true` 表示镜像，`false` 表示不镜像。
 * - `swap_xy`: 是否交换 X 和 Y 轴，`true` 表示交换，`false` 表示不交换。
 * - `fonts`: 显示字体设置，用于配置显示的字体样式。
 *
 * 特殊使用限制或潜在的副作用：
 * - 确保 `panel_io` 和 `panel` 已经正确初始化。
 * - `width` 和 `height` 应该根据实际的 LCD 显示器规格进行设置。
 * - 镜像和坐标交换设置可能会影响显示内容的方向和位置。
 */
class Mcu8080LcdDisplay : public LcdDisplay {
public:
    Mcu8080LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                      int width, int height, int offset_x, int offset_y,
                      bool mirror_x, bool mirror_y, bool swap_xy,
                      DisplayFonts fonts);
};
#endif // LCD_DISPLAY_H
