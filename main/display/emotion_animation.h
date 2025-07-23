#ifndef EMOTION_ANIMATION_H
#define EMOTION_ANIMATION_H

#include "lvgl.h"
#include <vector>
#include <string>

// 定义动画帧结构体
struct AnimationFrame {
    const lv_img_dsc_t* left_eye_image;
    const lv_img_dsc_t* right_eye_image;
    int duration_ms;
};

// 定义程序化动画的创建函数指针类型
using ProgrammaticAnimCreator = void (*)(lv_obj_t*, lv_obj_t*);

// 核心的动画结构体定义
struct Animation {
    enum class Type {
        IMAGE_SEQUENCE,
        LVGL_PROGRAMMATIC
    };

    std::string name;
    Type type;
    bool loop;

    // 根据动画类型使用不同的数据结构
    union {
        // 用于图片序列动画
        struct {
            std::vector<AnimationFrame>* frames;
        } image_sequence;

        // 用于程序化LVGL动画
        struct {
            ProgrammaticAnimCreator creator_func;
        } programmatic;
    };

    // 构造函数
    Animation(const std::string& anim_name, bool is_loop = false)
        : name(anim_name), type(Type::IMAGE_SEQUENCE), loop(is_loop) {
        image_sequence.frames = new std::vector<AnimationFrame>();
    }
    
    Animation(const std::string& anim_name, ProgrammaticAnimCreator creator)
        : name(anim_name), type(Type::LVGL_PROGRAMMATIC), loop(true) {
        programmatic.creator_func = creator;
    }
    
    // 拷贝构造函数，处理深拷贝
    Animation(const Animation& other)
        : name(other.name), type(other.type), loop(other.loop) {
        if (type == Type::IMAGE_SEQUENCE) {
            image_sequence.frames = new std::vector<AnimationFrame>(*other.image_sequence.frames);
        } else {
            programmatic.creator_func = other.programmatic.creator_func;
        }
    }

    // 赋值运算符
    Animation& operator=(const Animation& other) {
        if (this != &other) {
            // 清理旧资源
            if (type == Type::IMAGE_SEQUENCE) {
                delete image_sequence.frames;
            }
            // 拷贝新数据
            name = other.name;
            type = other.type;
            loop = other.loop;
            if (type == Type::IMAGE_SEQUENCE) {
                image_sequence.frames = new std::vector<AnimationFrame>(*other.image_sequence.frames);
            } else {
                programmatic.creator_func = other.programmatic.creator_func;
            }
        }
        return *this;
    }

    // 析构函数，释放内存
    ~Animation() {
        if (type == Type::IMAGE_SEQUENCE) {
            delete image_sequence.frames;
        }
    }

    // 辅助函数
    void AddFrame(const lv_img_dsc_t* left, const lv_img_dsc_t* right, int duration) {
        if (type == Type::IMAGE_SEQUENCE) {
            image_sequence.frames->push_back({left, right, duration});
        }
    }

    bool IsValid() const {
        if (type == Type::IMAGE_SEQUENCE) {
            return image_sequence.frames != nullptr && !image_sequence.frames->empty();
        }
        if (type == Type::LVGL_PROGRAMMATIC) {
            return programmatic.creator_func != nullptr;
        }
        return false;
    }
};

#endif // EMOTION_ANIMATION_H