#ifndef EMOTION_ANIMATION_H
#define EMOTION_ANIMATION_H

#include <vector>
#include <string>
#include "lvgl.h"

// 动画帧结构体 (用于图片序列动画)
struct AnimationFrame {
    const lv_img_dsc_t* left_eye_image;
    const lv_img_dsc_t* right_eye_image;
    uint32_t duration_ms;

    AnimationFrame(const lv_img_dsc_t* left, const lv_img_dsc_t* right, uint32_t duration = 0)
        : left_eye_image(left), right_eye_image(right), duration_ms(duration) {}
};

// 定义一个函数指针类型，用于创建程序化LVGL动画
// 参数: parent_left - 左眼屏幕, parent_right - 右眼屏幕
using lvgl_anim_creator_t = void (*)(lv_obj_t* parent_left, lv_obj_t* parent_right);

// 动画结构体 (已扩展)
struct Animation {
    // 枚举动画类型
    enum class Type {
        IMAGE_SEQUENCE,     // 图片序列帧动画 (旧方式)
        LVGL_PROGRAMMATIC   // LVGL程序化动画 (新方式)
    };

    std::string name;
    bool loop;
    Type type; // 新增：动画类型

    // 使用 union 存储不同类型的数据
    union {
        // 图片序列动画的数据 (保持不变)
        struct {
             std::vector<AnimationFrame>* frames;
        } image_sequence;

        // 程序化动画的数据
        struct {
            lvgl_anim_creator_t creator_func; // 指向创建函数
        } programmatic;
    };

    // --- 构造函数 ---

    // 图片序列动画的构造函数 (保持不变)
    Animation(const std::string& anim_name, bool is_loop = false)
        : name(anim_name), loop(is_loop), type(Type::IMAGE_SEQUENCE) {
        image_sequence.frames = new std::vector<AnimationFrame>();
    }

    // 新增：程序化动画的构造函数
    Animation(const std::string& anim_name, lvgl_anim_creator_t creator)
        : name(anim_name), loop(true), type(Type::LVGL_PROGRAMMATIC) {
        programmatic.creator_func = creator;
    }

    // 默认构造函数
    Animation() : name(""), loop(false), type(Type::IMAGE_SEQUENCE) {}


    void AddFrame(const lv_img_dsc_t* left, const lv_img_dsc_t* right, uint32_t duration) {
        if (type == Type::IMAGE_SEQUENCE && image_sequence.frames) {
            image_sequence.frames->emplace_back(left, right, duration);
        }
    }

    bool IsValid() const {
        if (name.empty()) return false;
        if (type == Type::IMAGE_SEQUENCE) {
            return image_sequence.frames != nullptr && !image_sequence.frames->empty();
        }
        return programmatic.creator_func != nullptr;
    }
};

#endif // EMOTION_ANIMATION_H