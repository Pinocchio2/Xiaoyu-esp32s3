#ifndef EYE_ANIMATION_DISPLAY_H
#define EYE_ANIMATION_DISPLAY_H

#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class EyeAnimationDisplay : public Display {
public:
    EyeAnimationDisplay();
    virtual ~EyeAnimationDisplay();

    virtual bool PlayAnimation(const Animation& animation) override;
    virtual void SetEmotion(const char* emotion) override;

    // ... 其他函数保持不变 ...

protected:
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

private:
     struct ImageUpdateData {
        lv_obj_t* img_obj;
        const void* img_src;
    };
    void PlayNextFrame();
    void StopAnimation();
    static void animation_timer_callback(void* arg);
    static void animation_task(void* pvParameters);

    static void update_image_callback(void* user_data);

    // --- 图片轮播动画所需的状态变量 (保持不变) ---
    const Animation* current_animation_ = nullptr;
    int current_frame_index_ = 0;
    bool is_looping_ = false;
    esp_timer_handle_t animation_timer_ = nullptr;
    TaskHandle_t animation_task_handle_ = nullptr;

    // LVGL对象 (保持不变)
    lv_obj_t* left_eye_img_ = nullptr;
    lv_obj_t* right_eye_img_ = nullptr;

    // 双屏引用 (保持不变)
    Display* primary_display_ = nullptr;
    Display* secondary_display_ = nullptr;
    
    
    bool is_programmatic_anim_active_ = false;

    // 添加静态成员声明
    static ImageUpdateData left_eye_data_;
    static ImageUpdateData right_eye_data_;
};

#endif // EYE_ANIMATION_DISPLAY_H