#include "emotion_manager.h"
#include <esp_log.h>

const char* EmotionManager::TAG = "EmotionManager";

EmotionManager::EmotionManager() 
    : default_animation_("blinking", true) {  // 改为眨眼动画作为默认S
    // 在构造函数中初始化默认眨眼动画
    default_animation_.AddFrame(&zhenyan, &zhenyan, 2000);    // 睁眼 2秒
    default_animation_.AddFrame(&zhayang1, &zhayang1, 100);   // 眨眼帧1 100ms
    default_animation_.AddFrame(&zhayang2, &zhayang2, 100);   // 眨眼帧2 100ms
    default_animation_.AddFrame(&zhayang3, &zhayang3, 100);   // 眨眼帧3 100ms
    default_animation_.AddFrame(&zhayang4, &zhayang4, 100);   // 眨眼帧4 100ms
    default_animation_.AddFrame(&zhayang3, &zhayang3, 100);   // 眨眼帧3 100ms
    default_animation_.AddFrame(&zhayang2, &zhayang2, 100);   // 眨眼帧2 100ms
    default_animation_.AddFrame(&zhayang1, &zhayang1, 100);   // 眨眼帧1 100ms
}

const Animation& EmotionManager::GetAnimation(const std::string& emotion_name) {
    auto it = animations_.find(emotion_name);
    if (it != animations_.end()) {
        ESP_LOGD(TAG, "找到表情动画: %s", emotion_name.c_str());
        return it->second;
    }
    
    ESP_LOGW(TAG, "未找到表情动画 '%s'，使用默认中性表情", emotion_name.c_str());
    return default_animation_;
}

void EmotionManager::PreloadAllAnimations() {
    // 打印开始预加载所有表情动画的日志信息
    ESP_LOGI(TAG, "开始预加载所有表情动画...");
    // 初始化表情动画
    InitializeAnimations();
    // 打印表情动画预加载完成的日志信息，并输出加载的动画数量
    ESP_LOGI(TAG, "表情动画预加载完成，共加载 %d 个动画", animations_.size());
}

void EmotionManager::RegisterAnimation(const std::string& emotion_name, const Animation& animation) {
    // 检查动画是否有效
    if (!animation.IsValid()) {
        // 如果无效，则输出错误日志
        ESP_LOGE(TAG, "尝试注册无效的动画: %s", emotion_name.c_str());
        return;
    }
    
    // 将动画注册到animations_中
    animations_[emotion_name] = animation;
    // 输出注册成功的日志
    ESP_LOGD(TAG, "注册表情动画: %s (帧数: %d)", emotion_name.c_str(), animation.frames.size());
}

// 检查emotion_name是否存在于animations_中
bool EmotionManager::HasAnimation(const std::string& emotion_name) const {
    // 如果animations_中存在emotion_name，则返回true，否则返回false
    return animations_.find(emotion_name) != animations_.end();
}

const Animation& EmotionManager::GetDefaultAnimation() const {
    return default_animation_;
}

void EmotionManager::InitializeAnimations() {
    // 基础静态表情
    RegisterAnimation("neutral", CreateStaticEmotion("neutral", &zhenyan , &zhenyan ));
    RegisterAnimation("happy", CreateStaticEmotion("happy", &happy, &happy));
    RegisterAnimation("sad", CreateStaticEmotion("sad", &crying_l, &crying_r));
    RegisterAnimation("funny", CreateStaticEmotion("funny", &funny, &funny));
    RegisterAnimation("sleepy", CreateStaticEmotion("sleepy", &biyan , &biyan ));
    
    // 添加listen表情
    RegisterAnimation("listen", CreateStaticEmotion("listen", &listen_l, &listen_r));
    
    // 对称表情（左右眼相同）
    RegisterAnimation("laughing", CreateStaticEmotion("laughing", &funny, &funny));
    RegisterAnimation("angry", CreateStaticEmotion("angry", &neutral, &neutral));  // 暂时用neutral代替
    RegisterAnimation("crying", CreateStaticEmotion("crying", &crying_l, &crying_r));
    RegisterAnimation("loving", CreateStaticEmotion("loving", &happy, &happy));  // 暂时用happy代替
    RegisterAnimation("embarrassed", CreateStaticEmotion("embarrassed", &neutral, &neutral));
    RegisterAnimation("surprised", CreateStaticEmotion("surprised", &zhenyan , &zhenyan ));
    RegisterAnimation("shocked", CreateStaticEmotion("shocked", &zhenyan , &zhenyan ));
    RegisterAnimation("thinking", CreateStaticEmotion("thinking", &neutral, &neutral));
    RegisterAnimation("cool", CreateStaticEmotion("cool", &neutral, &neutral));
    RegisterAnimation("relaxed", CreateStaticEmotion("relaxed", &biyan , &biyan ));
    RegisterAnimation("delicious", CreateStaticEmotion("delicious", &happy, &happy));
    RegisterAnimation("kissy", CreateStaticEmotion("kissy", &happy, &happy));
    RegisterAnimation("confident", CreateStaticEmotion("confident", &zhenyan , &zhenyan ));
    RegisterAnimation("silly", CreateStaticEmotion("silly", &funny, &funny));
    RegisterAnimation("confused", CreateStaticEmotion("confused", &neutral, &neutral));
    
    // 特殊动画：眨眼（左眼眨，右眼睁开）
    RegisterAnimation("winking", CreateWinkingAnimation());
    
    // 特殊动画：眨眼循环
    RegisterAnimation("blinking", CreateBlinkingAnimation());
}

Animation EmotionManager::CreateStaticEmotion(const std::string& name, 
                                            const lv_img_dsc_t* left_eye, 
                                            const lv_img_dsc_t* right_eye) {
    Animation animation(name, false);
    animation.AddFrame(left_eye, right_eye, 0);  // 持续时间为0表示静态显示
    return animation;
}

Animation EmotionManager::CreateDynamicEmotion(const std::string& name, 
                                             const std::vector<AnimationFrame>& frames, 
                                             bool loop) {
    Animation animation(name, loop);
    for (const auto& frame : frames) {
        animation.frames.push_back(frame);
    }
    return animation;
}

// 创建眨眼动画的私有方法
Animation EmotionManager::CreateWinkingAnimation() {
    Animation animation("winking", false);
    // 左眼眨，右眼睁开，持续500ms
    animation.AddFrame(&biyan, &zhenyan, 500);
    // 恢复双眼睁开
    animation.AddFrame(&zhenyan, &zhenyan, 0);
    return animation;
}

// 创建眨眼循环动画的私有方法
Animation EmotionManager::CreateBlinkingAnimation() {
    Animation animation("blinking", true);  // 循环播放
    // 睁眼状态，持续2秒
    animation.AddFrame(&zhenyan, &zhenyan, 2000);
    // 眨眼动画序列
    animation.AddFrame(&zhayang1, &zhayang1, 100);
    animation.AddFrame(&zhayang2, &zhayang2, 100);
    animation.AddFrame(&zhayang3, &zhayang3, 100);
    animation.AddFrame(&zhayang4, &zhayang4, 100);
    animation.AddFrame(&zhayang3, &zhayang3, 100);
    animation.AddFrame(&zhayang2, &zhayang2, 100);
    animation.AddFrame(&zhayang1, &zhayang1, 100);
    return animation;
}