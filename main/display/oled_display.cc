#include "oled_display.h"
#include "font_awesome_symbols.h"
#include "assets/lang_config.h"

#include <string>
#include <algorithm>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>

#define TAG "OledDisplay"

LV_FONT_DECLARE(font_awesome_30_1);

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
    int width, int height, bool mirror_x, bool mirror_y, DisplayFonts fonts)
    : panel_io_(panel_io), panel_(panel), fonts_(fonts) {
    // 初始化宽度、高度
    width_ = width;
    height_ = height;

    // 初始化LVGL
    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    // 添加LCD屏幕
    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    // 添加显示
    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    // 根据高度设置UI
    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }
}

OledDisplay::~OledDisplay() {
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

bool OledDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void OledDisplay::Unlock() {
    lvgl_port_unlock();
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }

    // Replace all newlines with spaces
    std::string content_str = content;
    std::replace(content_str.begin(), content_str.end(), '\n', ' ');

    if (content_right_ == nullptr) {
        lv_label_set_text(chat_message_label_, content_str.c_str());
    } else {
        if (content == nullptr || content[0] == '\0') {
            lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(chat_message_label_, content_str.c_str());
            lv_obj_clear_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void OledDisplay::SetupUI_128x64() {
    // 创建一个DisplayLockGuard对象，用于锁定显示
    DisplayLockGuard lock(this);

    // 获取当前屏幕
    auto screen = lv_screen_active();
    // 设置屏幕的文本字体和文本颜色
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    // 创建一个容器
    container_ = lv_obj_create(screen);
    // 设置容器的大小
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    // 设置容器的布局方式
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    // 设置容器的内边距
    lv_obj_set_style_pad_all(container_, 0, 0);
    // 设置容器的边框宽度
    lv_obj_set_style_border_width(container_, 0, 0);
    // 设置容器的行间距
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Status bar */
    // 创建一个状态栏
    status_bar_ = lv_obj_create(container_);
    // 设置状态栏的大小
    lv_obj_set_size(status_bar_, LV_HOR_RES, 16);
    // 设置状态栏的边框宽度
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    // 设置状态栏的内边距
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    // 设置状态栏的圆角半径
    lv_obj_set_style_radius(status_bar_, 0, 0);

    /* Content */
    // 创建一个内容区域
    content_ = lv_obj_create(container_);
    // 设置内容区域的滚动条模式
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    // 设置内容区域的圆角半径
    lv_obj_set_style_radius(content_, 0, 0);
    // 设置内容区域的内边距
    lv_obj_set_style_pad_all(content_, 0, 0);
    // 设置内容区域的宽度
    lv_obj_set_width(content_, LV_HOR_RES);
    // 设置内容区域的比例增长
    lv_obj_set_flex_grow(content_, 1);
    // 设置内容区域的布局方式
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_ROW);
    // 设置内容区域的主轴对齐方式
    lv_obj_set_style_flex_main_place(content_, LV_FLEX_ALIGN_CENTER, 0);

    // 创建左侧固定宽度的容器
    content_left_ = lv_obj_create(content_);
    // 设置左侧容器的大小
    lv_obj_set_size(content_left_, 32, LV_SIZE_CONTENT);  // 固定宽度32像素
    // 设置左侧容器的内边距
    lv_obj_set_style_pad_all(content_left_, 0, 0);
    // 设置左侧容器的边框宽度
    lv_obj_set_style_border_width(content_left_, 0, 0);

    // 创建一个表情标签
    emotion_label_ = lv_label_create(content_left_);
    // 设置表情标签的字体
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    // 设置表情标签的文本
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    // 居中表情标签
    lv_obj_center(emotion_label_);
    // 设置表情标签的上边距
    lv_obj_set_style_pad_top(emotion_label_, 8, 0);

    // 创建右侧可扩展的容器
    content_right_ = lv_obj_create(content_);
    // 设置右侧容器的大小
    lv_obj_set_size(content_right_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    // 设置右侧容器的内边距
    lv_obj_set_style_pad_all(content_right_, 0, 0);
    // 设置右侧容器的边框宽度
    lv_obj_set_style_border_width(content_right_, 0, 0);
    // 设置右侧容器的比例增长
    lv_obj_set_flex_grow(content_right_, 1);
    // 隐藏右侧容器
    lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);

    // 创建一个聊天消息标签
    chat_message_label_ = lv_label_create(content_right_);
    // 设置聊天消息标签的文本
    lv_label_set_text(chat_message_label_, "");
    // 设置聊天消息标签的长文本模式
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // 设置聊天消息标签的文本对齐方式
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_LEFT, 0);
    // 设置聊天消息标签的宽度
    lv_obj_set_width(chat_message_label_, width_ - 32);
    // 设置聊天消息标签的上边距
    lv_obj_set_style_pad_top(chat_message_label_, 14, 0);

    // 延迟一定的时间后开始滚动字幕
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    /* Status bar */
    // 设置状态栏的布局方式
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    // 设置状态栏的内边距
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    // 设置状态栏的边框宽度
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    // 设置状态栏的列间距
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    // 创建一个网络标签
    network_label_ = lv_label_create(status_bar_);
    // 设置网络标签的文本
    lv_label_set_text(network_label_, "");
    // 设置网络标签的字体
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);

    // 创建一个通知标签
    notification_label_ = lv_label_create(status_bar_);
    // 设置通知标签的比例增长
    lv_obj_set_flex_grow(notification_label_, 1);
    // 设置通知标签的文本对齐方式
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    // 设置通知标签的文本
    lv_label_set_text(notification_label_, "");
    // 隐藏通知标签
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    // 创建一个状态标签
    status_label_ = lv_label_create(status_bar_);
    // 设置状态标签的比例增长
    lv_obj_set_flex_grow(status_label_, 1);
    // 设置状态标签的文本
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    // 设置状态标签的文本对齐方式
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    // 创建一个静音标签
    mute_label_ = lv_label_create(status_bar_);
    // 设置静音标签的文本
    lv_label_set_text(mute_label_, "");
    // 设置静音标签的字体
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);

    // 创建一个电池标签
    battery_label_ = lv_label_create(status_bar_);
    // 设置电池标签的文本
    lv_label_set_text(battery_label_, "");
    // 设置电池标签的字体
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);

    // 创建一个低电量弹窗
    low_battery_popup_ = lv_obj_create(screen);
    // 设置低电量弹窗的滚动条模式
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    // 设置低电量弹窗的大小
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    // 居中对齐低电量弹窗
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    // 设置低电量弹窗的背景颜色
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    // 设置低电量弹窗的圆角半径
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    // 创建一个低电量标签
    low_battery_label_ = lv_label_create(low_battery_popup_);
    // 设置低电量标签的文本
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    // 设置低电量标签的文本颜色
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    // 居中对齐低电量标签
    lv_obj_center(low_battery_label_);
    // 隐藏低电量弹窗
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}

void OledDisplay::SetupUI_128x32() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_column(container_, 0, 0);

    /* Emotion label on the left side */
    content_ = lv_obj_create(container_);
    lv_obj_set_size(content_, 32, 32);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_radius(content_, 0, 0);

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    lv_obj_center(emotion_label_);

    /* Right side */
    side_bar_ = lv_obj_create(container_);
    lv_obj_set_size(side_bar_, width_ - 32, 32);
    lv_obj_set_flex_flow(side_bar_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(side_bar_, 0, 0);
    lv_obj_set_style_border_width(side_bar_, 0, 0);
    lv_obj_set_style_radius(side_bar_, 0, 0);
    lv_obj_set_style_pad_row(side_bar_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(side_bar_);
    lv_obj_set_size(status_bar_, width_ - 32, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_obj_set_style_pad_left(status_label_, 2, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_pad_left(notification_label_, 2, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);

    chat_message_label_ = lv_label_create(side_bar_);
    lv_obj_set_size(chat_message_label_, width_ - 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(chat_message_label_, 2, 0);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chat_message_label_, "");

    // 延迟一定的时间后开始滚动字幕
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);
}

