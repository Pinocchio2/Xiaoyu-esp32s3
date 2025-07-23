#ifndef EMOTION_MANAGER_H
#define EMOTION_MANAGER_H

#include <map>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "emotion_animation.h"

// 消息结构体定义
struct EmotionMessage {
    char emotion_name[32];
    uint32_t timestamp;
};

class EmotionManager {
public:
    static EmotionManager& GetInstance();
    void PreloadAllAnimations();
    const Animation& GetAnimation(const std::string& emotion_name);
    void ProcessEmotionAsync(const char* emotion_name);
    const Animation& GetDefaultAnimation() const;
    bool HasAnimation(const std::string& emotion_name) const;

private:
    EmotionManager();
    ~EmotionManager();
    EmotionManager(const EmotionManager&) = delete;
    EmotionManager& operator=(const EmotionManager&) = delete;

    void InitializeAnimations();
    void RegisterAnimation(const std::string& emotion_name, const Animation& animation);

    static void EmotionTaskWrapper(void* param);
    void EmotionTask();

    Animation CreateStaticEmotion(const std::string& name, const lv_img_dsc_t* left_eye, const lv_img_dsc_t* right_eye);
    Animation CreateDynamicEmotion(const std::string& name, const std::vector<AnimationFrame>& frames, bool loop = false);
    Animation CreateBlinkingAnimation();
    Animation CreateYanzhuAnimation();
    Animation CreateYanzhuScaleAnimation();
    Animation CreateSmileAnimation();
    Animation CreateSleepAnimation();

    std::map<std::string, Animation> animations_;
    Animation default_animation_;

    static QueueHandle_t emotion_queue_;
    static TaskHandle_t emotion_task_handle_;
    static const char* TAG;
};

#endif // EMOTION_MANAGER_H