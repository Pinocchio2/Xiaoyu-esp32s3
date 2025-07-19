// [新代码] 用这个版本替换旧的 display.h 文件内容

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


// 再次确认：前向声明 DualDisplayManager
class DualDisplayManager;

class Display {
public:
    Display();
    virtual ~Display();

    // --- 关键修改：将这些函数声明为纯虚函数 ---
    // 因为它们的实现依赖于UI控件，而这些控件现在不由这个类创建了。
    // 子类必须提供自己的实现，即使是空实现。
    virtual void SetStatus(const char* status) = 0;
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) = 0;
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000) = 0;
    virtual void SetEmotion(const char* emotion) = 0;
    virtual void SetChatMessage(const char* role, const char* content) = 0;
    virtual void SetIcon(const char* icon) = 0;
    virtual void SetTheme(const std::string& theme_name) = 0;
    virtual void UpdateBluetoothStatus(bool is_enabled) = 0;
    
    // 这个函数已经是纯虚的，保持不变
    virtual bool PlayAnimation(const Animation& animation) = 0;

    virtual std::string GetTheme() { return current_theme_name_; }

    lv_display_t* getLvDisplay() { return display_; }

    inline int width() const { return width_; }
    inline int height() const { return height_; }

protected:
    int width_ = 0;
    int height_ = 0;
    
    esp_pm_lock_handle_t pm_lock_ = nullptr;
    lv_display_t *display_ = nullptr;

    // 这些UI控件的指针依然保留，因为它们可能会被子类（如OledDisplay）使用
    // 但在LcdDisplay的上下文中，它们将不会被初始化
    lv_obj_t *emotion_label_ = nullptr;
    lv_obj_t *network_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *notification_label_ = nullptr;
    lv_obj_t *mute_label_ = nullptr;
    lv_obj_t *battery_label_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    lv_obj_t* low_battery_popup_ = nullptr;
    lv_obj_t* low_battery_label_ = nullptr;
    lv_obj_t* bluetooth_label_ = nullptr;
    
    const char* battery_icon_ = nullptr;
    const char* network_icon_ = nullptr;
    const char* bluetooth_icon_ = nullptr;
    bool muted_ = false;
    bool bluetooth_enabled_ = false;
    std::string current_theme_name_;

    esp_timer_handle_t notification_timer_ = nullptr;
    esp_timer_handle_t update_timer_ = nullptr;

    // --- 关键修改：将 DualDisplayManager 声明为友元 ---
    // 这样它才能访问 LcdDisplay 的内部成员来创建UI
    friend class DualDisplayManager;
    friend class DisplayLockGuard;

    // 这两个已经是纯虚的，保持不变
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;

    virtual void Update();
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

// NoDisplay 类的定义保持不变
class NoDisplay : public Display {
private:
    virtual bool Lock(int timeout_ms = 0) override { return true; }
    virtual void Unlock() override {}
public:
    virtual bool PlayAnimation(const Animation& animation) override { return true; }

    // 为所有新增的纯虚函数提供空实现
    virtual void SetStatus(const char* status) override {}
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override {}
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000) override {}
    virtual void SetEmotion(const char* emotion) override {}
    virtual void SetChatMessage(const char* role, const char* content) override {}
    virtual void SetIcon(const char* icon) override {}
    virtual void SetTheme(const std::string& theme_name) override {}
    virtual void UpdateBluetoothStatus(bool is_enabled) override {}
};

#endif