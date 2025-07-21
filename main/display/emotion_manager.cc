#include "emotion_manager.h"
#include <esp_log.h>
#include <cmath> 

// eye.h 应该已经包含了所有图片资源的声明
#include "ui/eye.h"

const char* EmotionManager::TAG = "EmotionManager";

//================================================================
// 1. 定义新的程序化动画创建函数
//================================================================

#ifndef LV_PI
#define LV_PI 3.1415926535f
#endif

// LVGL动画的回调函数 - 计算眼珠在下眼睑的轨迹
static void anim_path_cb(void * var, int32_t v) {
    // 将角度值转换为弧度
    float angle_rad = (float)v * LV_PI / 180.0f;
    // 计算眼珠在圆形轨迹上的位置（半径45像素）
    lv_coord_t new_x = 120 + (lv_coord_t)(45 * cosf(angle_rad));
    lv_coord_t new_y = 120 + (lv_coord_t)(45 * sinf(angle_rad));
    // 设置瞳孔位置（减去瞳孔半径30进行居中）
    lv_obj_set_pos((lv_obj_t*)var, new_x - 30, new_y - 30);
}

// 实际创建动画的函数 (需要接收父对象)
void create_orbiting_eye_anim_on_screen(lv_obj_t* scr) {
    // 设置屏幕背景颜色为黑色
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_clean(scr); // 清理屏幕

    // 创建眼白
    lv_obj_t * eyeball = lv_obj_create(scr);
    lv_obj_set_size(eyeball, 180, 180);
    lv_obj_align(eyeball, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(eyeball, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eyeball, lv_color_white(), 0);
    lv_obj_set_style_border_width(eyeball, 0, 0);

    // 创建瞳孔
    lv_obj_t * pupil = lv_obj_create(scr);
    lv_obj_set_size(pupil, 60, 60);
    lv_obj_set_style_radius(pupil, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pupil, lv_color_black(), 0);
    lv_obj_set_style_border_width(pupil, 0, 0);

    // 创建并配置动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, pupil);
    lv_anim_set_values(&a, 180, 360);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, anim_path_cb);
    lv_anim_start(&a);
}


// 符合我们新接口的创建函数
void create_dual_orbiting_eye_animation(lv_obj_t* parent_left, lv_obj_t* parent_right) {
    if (parent_left) {
        create_orbiting_eye_anim_on_screen(parent_left);
    }
    if (parent_right) {
        create_orbiting_eye_anim_on_screen(parent_right);
    }
}




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
    // 修复：根据动画类型访问正确的成员
    if (animation.type == Animation::Type::IMAGE_SEQUENCE && animation.image_sequence.frames) {
        ESP_LOGD(TAG, "注册表情动画: %s (帧数: %d)", emotion_name.c_str(), animation.image_sequence.frames->size());
    } else {
        ESP_LOGD(TAG, "注册表情动画: %s (程序化动画)", emotion_name.c_str());
    }
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
    
    // 添加程序化orbiting动画注册
    RegisterAnimation("orbiting", Animation("orbiting", create_dual_orbiting_eye_animation));
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
        // 修复：使用AddFrame方法而不是直接访问frames
        animation.AddFrame(frame.left_eye_image, frame.right_eye_image, frame.duration_ms);
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

