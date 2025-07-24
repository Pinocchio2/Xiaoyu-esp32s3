#ifndef EMOTION_ANIMATION_H
#define EMOTION_ANIMATION_H

#include "lvgl.h"
#include <vector>
#include <string>
#include <variant> // <--- 确保包含了这个关键的头文件

// 动画帧结构体
struct AnimationFrame {
    const lv_img_dsc_t* left_eye_image;
    const lv_img_dsc_t* right_eye_image;
    int duration_ms;
};

// 程序化动画的创建函数指针
using ProgrammaticAnimCreator = void (*)(lv_obj_t*, lv_obj_t*);

// 用于 variant 的图片序列数据结构
struct ImageSequenceData {
    std::vector<AnimationFrame> frames;
};

// 用于 variant 的程序化动画数据结构
struct ProgrammaticData {
    ProgrammaticAnimCreator creator_func;
};

// 最终的 Animation 结构体，使用 std::variant
struct Animation {
    std::string name;
    bool loop;

    // 使用 std::variant 安全地存储两种动画数据中的一种
    std::variant<ImageSequenceData, ProgrammaticData> data;

    // 图片序列动画的构造函数
    Animation(const std::string& anim_name, bool is_loop = false)
        : name(anim_name), loop(is_loop), data(std::in_place_type<ImageSequenceData>) {}
    
    // 程序化动画的构造函数
    Animation(const std::string& anim_name, ProgrammaticAnimCreator creator)
        : name(anim_name), loop(true), data(std::in_place_type<ProgrammaticData>, creator) {}
    
    // 编译器现在可以正确地为我们生成拷贝、赋值和析构函数了

    // 辅助函数：添加动画帧
    void AddFrame(const lv_img_dsc_t* left, const lv_img_dsc_t* right, int duration) {
        if (auto* seq_data = std::get_if<ImageSequenceData>(&data)) {
            seq_data->frames.push_back({left, right, duration});
        }
    }

    // 辅助函数：检查动画是否有效
    bool IsValid() const {
        return std::visit([](const auto& arg) -> bool {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, ImageSequenceData>) {
                return !arg.frames.empty();
            }
            if constexpr (std::is_same_v<T, ProgrammaticData>) {
                return arg.creator_func != nullptr;
            }
            return false;
        }, data);
    }
};

#endif // EMOTION_ANIMATION_H