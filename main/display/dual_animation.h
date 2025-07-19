#ifndef DUAL_ANIMATION_H_
#define DUAL_ANIMATION_H_

#include <lvgl.h>
#include <esp_log.h>

#include <string>
#include <list>
#include <vector>


typedef uint32_t (*AnimationFunction)(int count, uint16_t duration);




uint32_t shark_animation(int count, uint16_t duration);
uint32_t smile_animation(int count, uint16_t duration);


typedef struct{
    AnimationFunction callback; // 动画函数
    int count;// 运行次数
    uint16_t duration; // 动画持续时间
}dual_anim_t;


typedef struct{
    dual_anim_t dual_anim;
    const char* text;
}emotions_anim_t;


static const std::vector<emotions_anim_t> emotions_anim = {
    {{0,0,0}, "neutral"},
    {{shark_animation,0,0}, "happy"},
    {{smile_animation,0,0}, "laughing"},
    {{0,0,0}, "funny"},
    {{0,0,0}, "sad"},
    {{0,0,0}, "angry"},
    {{0,0,0}, "crying"},
    {{0,0,0}, "loving"},
    {{0,0,0}, "embarrassed"},
    {{0,0,0}, "surprised"},
    {{0,0,0}, "shocked"},
    {{0,0,0}, "thinking"},
    {{0,0,0}, "winking"},
    {{0,0,0}, "cool"},
    {{0,0,0}, "relaxed"},
    {{0,0,0}, "delicious"},
    {{0,0,0}, "kissy"},
    {{0,0,0}, "confident"},
    {{0,0,0}, "sleepy"},
    {{0,0,0}, "silly"},
    {{0,0,0}, "confused"}
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