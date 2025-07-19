#ifndef DUAL_ANIMATION_H_
#define DUAL_ANIMATION_H_

#include <lvgl.h>
#include <esp_log.h>
#include <string>
#include <list>
#include <vector>

// 移除重复的图像声明，使用ui/eye.h中的声明
// 只声明dual_animation.h特有的图像
LV_IMG_DECLARE(eye_blink_00);
LV_IMG_DECLARE(eye_blink_01);
LV_IMG_DECLARE(eye_blink_2);
LV_IMG_DECLARE(eye_blink_03);
LV_IMG_DECLARE(eye_blink_04);
LV_IMG_DECLARE(eye_blink_05);
LV_IMG_DECLARE(eye_closed_00);
LV_IMG_DECLARE(eye_happy_00);
LV_IMG_DECLARE(eye_sad_00);
LV_IMG_DECLARE(eye_think_00);
LV_IMG_DECLARE(eye_angry_00);

// 动画函数类型定义
typedef uint32_t (*AnimationFunction)(int count, uint16_t duration);

// 结构体定义 - 移到使用前
typedef struct{
    AnimationFunction callback; // 动画函数
    int count;// 运行次数
    uint16_t duration; // 动画持续时间
}dual_anim_t;

typedef struct{
    dual_anim_t dual_anim;
    const char* text;
}emotions_anim_t;

// 函数声明
uint32_t shark_animation(int count, uint16_t duration);
uint32_t smile_animation(int count, uint16_t duration);

// 只保留一个emotions_anim向量定义
static const std::vector<emotions_anim_t> emotions_anim = {
    {{nullptr, 0, 0}, "neutral"},
    {{shark_animation, 1, 2000}, "happy"},
    {{smile_animation, 1, 2000}, "laughing"},
    {{nullptr, 0, 0}, "funny"},
    {{nullptr, 0, 0}, "sad"},
    {{nullptr, 0, 0}, "angry"},
    {{nullptr, 0, 0}, "crying"},
    {{nullptr, 0, 0}, "loving"},
    {{nullptr, 0, 0}, "embarrassed"},
    {{nullptr, 0, 0}, "surprised"},
    {{nullptr, 0, 0}, "shocked"},
    {{nullptr, 0, 0}, "thinking"},
    {{nullptr, 0, 0}, "winking"},
    {{nullptr, 0, 0}, "cool"},
    {{nullptr, 0, 0}, "relaxed"},
    {{nullptr, 0, 0}, "delicious"},
    {{nullptr, 0, 0}, "kissy"},
    {{nullptr, 0, 0}, "confident"},
    {{nullptr, 0, 0}, "sleepy"},
    {{nullptr, 0, 0}, "silly"},
    {{nullptr, 0, 0}, "confused"}
};

class DualAnimation {
private:
    std::list<dual_anim_t> anim_queue_;
    static void animation_task(void *param);
    
public:
    DualAnimation();
    ~DualAnimation() = default; 

    void ShowAnimation(const char* emotion);
    void ShowAnimationNow(const char* emotion); //立即执行，清空后面的表情动画
    const dual_anim_t* get_anim_by_name(const char* name);
};

#endif