#include "emotion_manager.h"
#include <esp_log.h>

const char* EmotionManager::TAG = "EmotionManager";

EmotionManager::EmotionManager() 
    : default_animation_("blinking", true) {  // 改为眨眼动画作为默认S
    // 在构造函数中初始化默认眨眼动画
    default_animation_.AddFrame(&zhayang1, &zhayang1, 1000);    // 睁眼 2秒
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
    
    ESP_LOGI(TAG, "开始预加载所有表情动画...");
    InitializeAnimations();    
    ESP_LOGI(TAG, "表情动画预加载完成，共加载 %d 个动画", animations_.size());
}

void EmotionManager::RegisterAnimation(const std::string& emotion_name, const Animation& animation) {
    
    if (!animation.IsValid()) {     
        ESP_LOGE(TAG, "尝试注册无效的动画: %s", emotion_name.c_str());
        return;
    }
    

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
    
    // 添加眼睛状态表情 - 新增这两行
    RegisterAnimation("closed_eyes", CreateStaticEmotion("closed_eyes", &biyan, &biyan));  // 闭眼状态
    RegisterAnimation("open_eyes", CreateYanzhuAnimation());  // 睁眼状态使用yanzhu动画
    
    // 添加listen表情
    //RegisterAnimation("listen", CreateStaticEmotion("listen", &listen_l, &listen_r));
    
    // 对称表情（左右眼相同）
    RegisterAnimation("laughing", CreateStaticEmotion("laughing", &funny, &funny));
    RegisterAnimation("angry", CreateStaticEmotion("angry", &neutral, &neutral));  // 暂时用neutral代替
    RegisterAnimation("crying", CreateStaticEmotion("crying", &crying_l, &crying_r));
    RegisterAnimation("loving", CreateStaticEmotion("loving", &happy, &happy));  // 暂时用happy代替
    RegisterAnimation("embarrassed", CreateStaticEmotion("embarrassed", &neutral, &neutral));
    RegisterAnimation("surprised", CreateStaticEmotion("surprised", &zhenyan , &zhenyan ));
    RegisterAnimation("shockeinkid", CreateStaticEmotion("shocked", &zhenyan , &zhenyan ));
    RegisterAnimation("thng", CreateStaticEmotion("thinking", &neutral, &neutral));
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

    RegisterAnimation("winking", CreateWinkingAnimation());
    
    // 新增yanzhu动画
    RegisterAnimation("yanzhu", CreateYanzhuAnimation());

    //眼珠缩放动画
    RegisterAnimation("eyeball", CreateYanzhuScaleAnimation());

    // 新增微笑动画
    RegisterAnimation("smile", CreateSmileAnimation());
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
    //animation.AddFrame(&zhenyan, &zhenyan, 2000);
    // 眨眼动画序列
    animation.AddFrame(&zhayang1, &zhayang1, 1000);
    animation.AddFrame(&zhayang2, &zhayang2, 100);
    animation.AddFrame(&zhayang3, &zhayang3, 100);
    animation.AddFrame(&zhayang4, &zhayang4, 100);
    animation.AddFrame(&zhayang3, &zhayang3, 100);
    animation.AddFrame(&zhayang2, &zhayang2, 100);
    animation.AddFrame(&zhayang1, &zhayang1, 100);
    return animation;
}

/**
 * @brief 创建yanzhu动画（眼珠转动动画）
 * @return yanzhu动画对象
 */
Animation EmotionManager::CreateYanzhuAnimation() {
    Animation animation("yanzhu", true);  // 循环播放
    
    // 添加8帧yanzhu动画，每帧持续200ms
    animation.AddFrame(&yanzhu1, &yanzhu1, 500);
    animation.AddFrame(&yanzhu2, &yanzhu2, 500);
    animation.AddFrame(&yanzhu3, &yanzhu3, 500);
    animation.AddFrame(&yanzhu2, &yanzhu2, 500);
    
    // animation.AddFrame(&yanzhu6, &yanzhu6, 100);
    // animation.AddFrame(&yanzhu7, &yanzhu7, 100);
    // animation.AddFrame(&yanzhu8, &yanzhu8, 100);
    
    return animation;
}

//眼珠缩放动画
Animation EmotionManager::CreateYanzhuScaleAnimation() {
    Animation animation("eyeball", true);  // 循环播放
    
    // 添加缩放动画帧
    animation.AddFrame(&yanzhu_da_m, &yanzhu_da, 300);
    animation.AddFrame(&yanzhu_xiao_m, &yanzhu_xiao, 600);
    animation.AddFrame(&yanzhu_da_m, &yanzhu_da, 300);
    
    return animation;
}

Animation EmotionManager::CreateSmileAnimation() {
    Animation animation("smile", false);  // 不循环播放
    
    // 微笑动画序列：从正常表情逐渐变成微笑
    animation.AddFrame(&smile1, &smile1, 200);  // 微笑帧1 - 200ms
    animation.AddFrame(&smile2, &smile2, 200);  // 微笑帧2 - 200ms  
    animation.AddFrame(&smile3, &smile3, 200);  // 微笑帧3 - 200ms
    animation.AddFrame(&smile4, &smile4, 500);  // 微笑帧4 - 保持500ms
    animation.AddFrame(&smile3, &smile3, 200);  // 微笑帧3 - 200ms
    animation.AddFrame(&smile2, &smile2, 200);  // 微笑帧2 - 200ms
    animation.AddFrame(&smile1, &smile1, 200);  // 微笑帧1 - 200ms
    
    return animation;
}

