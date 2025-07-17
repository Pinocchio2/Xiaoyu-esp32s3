#ifndef EYE_DISPLAY_H
#define EYE_DISPLAY_H

#include "display.h"
#include "emotion_animation.h"
#include <esp_timer.h>

class DualDisplayManager; // 前向声明

class EyeDisplay : public Display {
private:
    DualDisplayManager* dual_display_manager_;
    
    // 动画状态管理
    Animation current_animation_;
    size_t current_frame_index_;
    esp_timer_handle_t animation_timer_;
    bool is_playing_animation_;
    
    // 静态回调函数
    static void AnimationTimerCallback(void* arg);
    
    // 内部方法
    void PlayNextFrame();
    void StopCurrentAnimation();
    
public:
    EyeDisplay(DualDisplayManager* dual_display_manager);
    virtual ~EyeDisplay();
    
    // 重写基类的虚函数
    virtual bool PlayAnimation(const Animation& animation) override;
    
    // 重写基类的其他必要方法
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;
    //virtual lv_disp_t* getLvDisplay() override;
    lv_disp_t* getLvDisplay();
};

#endif // EYE_DISPLAY_H