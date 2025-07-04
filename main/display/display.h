#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_pm.h>

#include <string>

struct DisplayFonts {
    const lv_font_t* text_font = nullptr;
    const lv_font_t* icon_font = nullptr;
    const lv_font_t* emoji_font = nullptr;
};

/**
 * @brief Display 类用于管理和控制显示设备，提供多种显示功能。
 *
 * Display 类是一个抽象基类，用于管理和控制显示设备。它提供了设置状态、显示通知、设置表情、设置聊天消息、设置图标和主题等功能。
 * 该类还提供了获取当前主题的方法。Display 类是一个虚拟基类，需要子类实现 Lock 和 Unlock 方法来控制显示设备的锁定和解锁。
 *
 * 核心功能包括：
 * - 设置显示状态（SetStatus）
 * - 显示通知（ShowNotification）
 * - 设置表情（SetEmotion）
 * - 设置聊天消息（SetChatMessage）
 * - 设置图标（SetIcon）
 * - 设置主题（SetTheme）
 * - 获取当前主题（GetTheme）
 *
 * 使用示例：
 *
 * 构造函数：
 * - Display()：构造函数，初始化 Display 对象。
 *
 * 析构函数：
 * - virtual ~Display()：虚析构函数，确保子类正确释放资源。
 *
 * 特殊限制或副作用：
 * - 子类必须实现 Lock 和 Unlock 方法，否则无法正确控制显示设备的锁定和解锁。
 * - 使用 SetTheme 方法时，需要确保主题名称是有效的。
 */
class Display {
public:
    Display();
    virtual ~Display();

    // 设置状态
    virtual void SetStatus(const char* status);
    // 显示通知，duration_ms为通知显示的时间，默认为3000毫秒
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    // 显示通知，duration_ms为通知显示的时间，默认为3000毫秒
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000);
    // 设置表情
    virtual void SetEmotion(const char* emotion);
    // 设置聊天消息
    virtual void SetChatMessage(const char* role, const char* content);
    // 设置图标
    virtual void SetIcon(const char* icon);
    // 设置主题
    virtual void SetTheme(const std::string& theme_name);
    // 获取当前主题
    virtual std::string GetTheme() { return current_theme_name_; }

    //void SetBluetoothIcon(bool enabled);

    // 获取宽度
    inline int width() const { return width_; }
    // 获取高度
    inline int height() const { return height_; }

protected:
    int width_ = 0;
    int height_ = 0;
    
    // 电源管理锁
    esp_pm_lock_handle_t pm_lock_ = nullptr;
    // 显示对象
    lv_display_t *display_ = nullptr;

    // 表情标签
    lv_obj_t *emotion_label_ = nullptr;
    // 网络标签
    lv_obj_t *network_label_ = nullptr;
    // 状态标签
    lv_obj_t *status_label_ = nullptr;
    // 通知标签
    lv_obj_t *notification_label_ = nullptr;
    // 静音标签
    lv_obj_t *mute_label_ = nullptr;
    // 电池标签
    lv_obj_t *battery_label_ = nullptr;
    // 聊天消息标签
    lv_obj_t* chat_message_label_ = nullptr;
    // 低电量弹窗
    lv_obj_t* low_battery_popup_ = nullptr;
    // 低电量标签
    lv_obj_t* low_battery_label_ = nullptr;
    
    // 电池图标
    const char* battery_icon_ = nullptr;
    // 网络图标
    const char* network_icon_ = nullptr;
    // 是否静音
    bool muted_ = false;
    // 当前主题名称
    std::string current_theme_name_;

    // 通知定时器
    esp_timer_handle_t notification_timer_ = nullptr;
    // 更新定时器
    esp_timer_handle_t update_timer_ = nullptr;

    // 好友类
    friend class DisplayLockGuard;
    // 锁定显示，timeout_ms为超时时间，默认为0
    virtual bool Lock(int timeout_ms = 0) = 0;
    // 解锁显示
    virtual void Unlock() = 0;

    // 更新显示
    virtual void Update();

    // 蓝牙标签
    // lv_obj_t* bluetooth_label_ = nullptr;
    // 蓝牙图标
    // const char* bluetooth_icon_ = nullptr;
    // 蓝牙是否启用
    // bool bluetooth_enabled_ = false;

};


/**
 * @brief 用于管理显示设备的锁定和解锁的类。
 *
 * 这个类是一个RAII（Resource Acquisition Is Initialization）风格的类，用于确保在对象的生命周期内显示设备被正确锁定。
 * 当对象被创建时，它会尝试锁定显示设备，并在对象被销毁时自动解锁显示设备。
 * 
 * 使用该类可以避免手动锁定和解锁显示设备时可能出现的错误，确保资源的正确释放。
 *
 * @param display 指向要锁定的显示设备的指针。
 *
 * 示例：
 *
 * 注意：
 * - 构造函数会尝试在30秒内锁定显示设备，如果锁定失败，会记录错误日志。
 * - 该类不负责检查 `display` 指针的有效性，使用时需确保 `display` 指针非空。
 */
class DisplayLockGuard {
public:
    // 构造函数，传入一个Display指针，并尝试锁定该Display
    DisplayLockGuard(Display *display) : display_(display) {
        if (!display_->Lock(30000)) {
            ESP_LOGE("Display", "Failed to lock display");
        }
    }
    // 析构函数，解锁Display
    ~DisplayLockGuard() {
        display_->Unlock();
    }

private:
    // Display指针
    Display *display_;
};

/**
 * @brief 一个不显示任何内容的显示类。
 *
 * NoDisplay 类继承自 Display 类，用于模拟一个不执行任何显示操作的显示设备。
 * 该类重写了 Display 类的 Lock 和 Unlock 方法，使其不执行任何操作。
 *
 * 核心功能：
 * - Lock: 始终返回 true，表示锁定操作总是成功。
 * - Unlock: 不执行任何操作。
 *
 * 使用示例：
 *
 * 构造函数：
 * - NoDisplay(): 默认构造函数，无需任何参数。
 *
 * 特殊使用限制或潜在的副作用：
 * - 无特殊使用限制或潜在的副作用。
 */
class NoDisplay : public Display {
private:
    // 重写Lock函数，返回true
    virtual bool Lock(int timeout_ms = 0) override {
        return true;
    }
    // 重写Unlock函数，不执行任何操作
    virtual void Unlock() override {}
};

#endif
