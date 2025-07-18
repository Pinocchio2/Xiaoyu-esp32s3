#ifndef EYE_ANIMATION_DISPLAY_H
#define EYE_ANIMATION_DISPLAY_H

#include "display.h"

/**
 * @brief 眼睛动画显示类 - 专门用于双屏眼睛动画
 * 继承自Display基类，去掉UI功能，只保留双屏眼睛动画
 */
class EyeAnimationDisplay : public Display {
public:
    /**
     * @brief 构造函数 - 初始化眼睛动画显示
     */
    EyeAnimationDisplay();
    
    /**
     * @brief 析构函数 - 清理资源
     */
    virtual ~EyeAnimationDisplay();
    
    // 重写基类方法 - 保留需要的功能
    
    /**
     * @brief 播放动画 - 核心功能
     * @param animation 要播放的动画
     * @return true 播放成功, false 播放失败
     */
    virtual bool PlayAnimation(const Animation& animation) override;
    
    /**
     * @brief 设置情感表情 - 用于眼睛动画
     * @param emotion 情感名称
     */
    virtual void SetEmotion(const char* emotion) override;
    
    // 重写基类方法 - 置空不需要的UI功能，节省资源
    
    /**
     * @brief 设置状态文本 - 眼睛动画不需要，置空
     */
    virtual void SetStatus(const char* status) override {}
    
    /**
     * @brief 显示通知 - 眼睛动画不需要，置空
     */
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override {}
    
    /**
     * @brief 显示通知(string版本) - 眼睛动画不需要，置空
     */
    virtual void ShowNotification(const std::string& notification, int duration_ms = 3000) override {}
    
    /**
     * @brief 设置聊天消息 - 眼睛动画不需要，置空
     */
    virtual void SetChatMessage(const char* role, const char* content) override {}
    
    /**
     * @brief 设置图标 - 眼睛动画不需要，置空
     */
    virtual void SetIcon(const char* icon) override {}
    
    /**
     * @brief 设置主题 - 眼睛动画不需要，置空
     */
    virtual void SetTheme(const std::string& theme_name) override {}
    
    // 添加非虚函数方法（不使用override）
    
    /**
     * @brief 清除通知 - 眼睛动画不需要，但提供接口兼容性
     */
    void ClearNotification() {}
    
    /**
     * @brief 设置亮度 - 眼睛动画不需要，但提供接口兼容性
     */
    void SetBrightness(int brightness) {}
    
    /**
     * @brief 清除屏幕 - 眼睛动画不需要，但提供接口兼容性
     */
    void Clear() {}

protected:
    /**
     * @brief 锁定显示 - 用于线程安全
     * @param timeout_ms 超时时间（毫秒）
     * @return true 锁定成功, false 锁定失败
     */
    virtual bool Lock(int timeout_ms = 0) override;
    
    /**
     * @brief 解锁显示 - 用于线程安全
     */
    virtual void Unlock() override;

private:
    /**
     * @brief 播放下一帧动画
     */
    void PlayNextFrame();
    
    /**
     * @brief 停止动画播放
     */
    void StopAnimation();
    
    /**
     * @brief 动画定时器回调函数
     * @param arg 回调参数（this指针）
     */
    static void animation_timer_callback(void* arg);
    
    // 动画状态变量
    const Animation* current_animation_ = nullptr;  // 当前播放的动画
    int current_frame_index_ = 0;                  // 当前帧索引
    bool is_looping_ = false;                      // 是否循环播放
    esp_timer_handle_t animation_timer_ = nullptr; // 动画定时器句柄
    
    // LVGL对象
    lv_obj_t* screen_ = nullptr;        // 屏幕对象
    lv_obj_t* left_eye_img_ = nullptr;  // 左眼图像对象
    lv_obj_t* right_eye_img_ = nullptr; // 右眼图像对象
    
    // 双屏引用
    Display* primary_display_ = nullptr;    // 主屏幕引用
    Display* secondary_display_ = nullptr;  // 副屏幕引用
};

#endif // EYE_ANIMATION_DISPLAY_H