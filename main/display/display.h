#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_pm.h>

#include <string>

struct Animation;

struct DisplayFonts {
    const lv_font_t* text_font = nullptr;
    const lv_font_t* icon_font = nullptr;
    const lv_font_t* emoji_font = nullptr;
};

class Display {
public:
    Display();
    virtual ~Display();

    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000);
    virtual void SetEmotion(const char* emotion);
    virtual void SetChatMessage(const char* role, const char* content);
    virtual void SetIcon(const char* icon);
    virtual void SetTheme(const std::string& theme_name);
    
    virtual bool PlayAnimation(const Animation& animation) = 0;

// 修改现有的 SetEmotion 方法，使其调用 EmotionManager
    //virtual void SetEmotion(const char* emotion) override;


    virtual std::string GetTheme() { return current_theme_name_; }

    

    /**
     * @brief 新增：一个独立的接口，用于根据蓝牙状态更新屏幕图标。
     * @param is_enabled true: 显示蓝牙图标, false: 隐藏图标
     */
    void UpdateBluetoothStatus(bool is_enabled);

    lv_display_t* getLvDisplay() { return display_; }//
    

    inline int width() const { return width_; }
    inline int height() const { return height_; }

protected:
    int width_ = 0;
    int height_ = 0;
    
    esp_pm_lock_handle_t pm_lock_ = nullptr;
    lv_display_t *display_ = nullptr;

    lv_obj_t *emotion_label_ = nullptr;
    lv_obj_t *network_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *notification_label_ = nullptr;
    lv_obj_t *mute_label_ = nullptr;
    lv_obj_t *battery_label_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    lv_obj_t* low_battery_popup_ = nullptr;
    lv_obj_t* low_battery_label_ = nullptr;
    
    const char* battery_icon_ = nullptr;
    const char* network_icon_ = nullptr;
    bool muted_ = false;
    std::string current_theme_name_;

    esp_timer_handle_t notification_timer_ = nullptr;
    esp_timer_handle_t update_timer_ = nullptr;

    friend class DisplayLockGuard;
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;

    virtual void Update();

    // *** 新增以下三个变量 ***
    lv_obj_t* bluetooth_label_ = nullptr;     // 指向蓝牙图标的LVGL对象
    const char* bluetooth_icon_ = nullptr;  // 存储当前图标的字符
    bool bluetooth_enabled_ = false;          // 存储蓝牙的当前状态


};


class DisplayLockGuard {
public:
    DisplayLockGuard(Display *display) : display_(display) {
        if (!display_->Lock(30000)) {
            ESP_LOGE("Display", "Failed to lock display");
        }
    }
    ~DisplayLockGuard() {
        display_->Unlock();
    }

private:
    Display *display_;
};

class NoDisplay : public Display { // 定义一个名为NoDisplay的类，继承自Display类
private:
    virtual bool Lock(int timeout_ms = 0) override { // 重写Lock函数，参数为timeout_ms，默认值为0
        return true; // 返回true
    }
    virtual void Unlock() override {}
public:
        // 新增：实现纯虚函数PlayAnimation
    virtual bool PlayAnimation(const Animation& animation) override {
            // NoDisplay不需要实际播放动画，直接返回true表示"成功"
            return true;
        } // 重写Unlock函数，无参数，无返回值
    };

#endif
