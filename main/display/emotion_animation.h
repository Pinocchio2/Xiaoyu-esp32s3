#ifndef EMOTION_ANIMATION_H
#define EMOTION_ANIMATION_H

#include <vector>
#include <string>
#include <lvgl.h>

// 动画帧结构体
struct AnimationFrame {
    const lv_img_dsc_t* left_eye_image;   // 左眼图像
    const lv_img_dsc_t* right_eye_image;  // 右眼图像
    uint32_t duration_ms;                 // 帧持续时间（毫秒）
    
    // 构造函数
    AnimationFrame(const lv_img_dsc_t* left, const lv_img_dsc_t* right, uint32_t duration = 0)
        : left_eye_image(left), right_eye_image(right), duration_ms(duration) {}
    
    // 默认构造函数
    AnimationFrame() : left_eye_image(nullptr), right_eye_image(nullptr), duration_ms(0) {}
};

// 动画结构体
struct Animation {
    std::vector<AnimationFrame> frames;   // 动画帧序列
    std::string name;                     // 动画名称
    bool loop;                           // 是否循环播放
    
    // 构造函数
    Animation(const std::string& anim_name, bool is_loop = false)
        : name(anim_name), loop(is_loop) {}
    
    // 默认构造函数
    Animation() : name(""), loop(false) {}
    
    // 添加动画帧
    void AddFrame(const lv_img_dsc_t* left_eye, const lv_img_dsc_t* right_eye, uint32_t duration_ms) {
        frames.emplace_back(left_eye, right_eye, duration_ms);
    }
    
    // 检查动画是否有效
    bool IsValid() const {
        return !frames.empty() && !name.empty();
    }
    
    // 获取动画总时长
    uint32_t GetTotalDuration() const {
        uint32_t total = 0;
        for (const auto& frame : frames) {
            total += frame.duration_ms;
        }
        return total;
    }
};

// 动画优先级枚举
enum class AnimationPriority {
    LOW = 0,      // 低优先级（如空闲动画）
    NORMAL = 1,   // 普通优先级（如表情动画）
    HIGH = 2,     // 高优先级（如系统通知）
    URGENT = 3    // 紧急优先级（如错误提示）
};

// 动画状态枚举
enum class AnimationState {
    STOPPED = 0,  // 停止状态
    PLAYING = 1,  // 播放中
    PAUSED = 2    // 暂停状态
};

#endif // EMOTION_ANIMATION_H