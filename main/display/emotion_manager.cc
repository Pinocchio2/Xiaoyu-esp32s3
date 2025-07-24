#include "emotion_manager.h"
#include <esp_log.h>
#include <cmath> 
#include <cstring> // <--- 新增：为了使用 strncpy

#include "emotion_animation.h"
#include "eye_animation_display.h"  // 添加这个头文件包含
#include "board.h" // <--- 新增：为了使用 Board::GetInstance()
#include "ui/eye.h"

const char* EmotionManager::TAG = "EmotionManager";

QueueHandle_t EmotionManager::emotion_queue_ = nullptr;
TaskHandle_t EmotionManager::emotion_task_handle_ = nullptr;

// 定义圆周率常量
#ifndef LV_PI
#define LV_PI 3.1415926535f
#endif

// 删除未使用的变量
// static bool anim_direction_forward = true;  // 删除这行
// static float current_angle = 180.0f;        // 删除这行

// 修改后的轨道动画回调函数
static void anim_path_cb(void * var, int32_t v) {
    // 计算折返角度：在180-360度之间来回移动
    float angle_range = 180.0f; // 角度范围
    float progress = (float)v / 360.0f; // 将输入值标准化为0-1
    
    // 使用正弦波函数实现折返效果
    float angle_offset = angle_range * sinf(progress * LV_PI);
    float angle_rad = (180.0f + angle_offset) * LV_PI / 180.0f;
    
    // 计算眼珠在圆形轨迹上的位置（半径45像素）
    lv_coord_t new_x = 120 + (lv_coord_t)(45 * cosf(angle_rad));
    lv_coord_t new_y = 120 + (lv_coord_t)(45 * sinf(angle_rad));
    
    // 设置瞳孔位置（减去瞳孔半径30进行居中）
    lv_obj_set_pos((lv_obj_t*)var, new_x - 30, new_y - 30);
}

// 修改轨道动画创建函数
void create_orbiting_eye_anim_on_screen(lv_obj_t* scr) {
    // 设置屏幕背景颜色为黑色
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_anim_del(scr, NULL);

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

    // 创建并配置折返轨道动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, pupil);
    lv_anim_set_values(&a, 0, 360); // 使用0-360作为进度值
    lv_anim_set_time(&a, 4000); // 增加动画时间使移动更平滑
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

///---------------------------------------------------------------------------------------------

// 眼珠缩放动画回调函数
static void scale_anim_cb(void * var, int32_t v) {
    // v的范围是60-72，对应正常大小到放大20%的缩放
    lv_obj_set_size((lv_obj_t*)var, v, v);
}

// 创建眼珠放大缩小动画
void create_scaling_eye_anim_on_screen(lv_obj_t* scr) {
    // 设置屏幕背景颜色为黑色
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_anim_del(scr, NULL);
    lv_obj_clean(scr); // 清理屏幕

    // 创建眼白（固定大小）
    lv_obj_t * eyeball = lv_obj_create(scr);
    lv_obj_set_size(eyeball, 180, 180);
    lv_obj_align(eyeball, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(eyeball, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eyeball, lv_color_white(), 0);
    lv_obj_set_style_border_width(eyeball, 0, 0);

    // 创建瞳孔（会进行缩放动画）
    lv_obj_t * pupil = lv_obj_create(scr);
    lv_obj_set_size(pupil, 60, 60); // 正常大小60x60
    lv_obj_align(pupil, LV_ALIGN_CENTER, 0, 0); // 居中对齐
    lv_obj_set_style_radius(pupil, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pupil, lv_color_black(), 0);
    lv_obj_set_style_border_width(pupil, 0, 0);

    // 创建并配置缩放动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, pupil);
    lv_anim_set_values(&a, 60, 72); // 从正常大小60到200放大20%的72
    lv_anim_set_time(&a, 1500); // 1.5秒完成一次缩放
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_playback_time(&a, 1500); // 设置回放时间，实现放大后缩小
    lv_anim_set_exec_cb(&a, scale_anim_cb);
    lv_anim_start(&a);
}

// 双眼缩放动画函数
void create_dual_scaling_eye_animation(lv_obj_t* parent_left, lv_obj_t* parent_right) {
    if (parent_left) {
        create_scaling_eye_anim_on_screen(parent_left);
    }
    if (parent_right) {
        create_scaling_eye_anim_on_screen(parent_right);
    }
}

//------------------------------------------------------------------------------

static void breathing_arc_cb(void * var, int32_t v) {
    lv_obj_t * line = (lv_obj_t *)var;
    
    // 定义弧线的三个点：起点，中点（控制点），终点
    // 修复：使用 lv_point_precise_t 替代 lv_point_t 以兼容新版本 LVGL
    static lv_point_precise_t p[] = {{40, 120}, {120, 135}, {200, 120}};
    
    // v 是动画值 (从0变化到10)，我们用它作为中点Y坐标的偏移量，实现起伏效果
    // 将点数组转换为lv_point_t类型
    lv_point_t points[3];
    for(int i = 0; i < 3; i++) {
        points[i].x = (lv_coord_t)p[i].x;
        points[i].y = (lv_coord_t)p[i].y;
    }
    points[1].y = 135 + v;
    // 将 lv_point_t 数组转换为 lv_point_precise_t 数组以匹配函数参数类型
    lv_point_precise_t precise_points[3];
    for(int i = 0; i < 3; i++) {
        precise_points[i].x = points[i].x;
        precise_points[i].y = points[i].y;
    }
    lv_line_set_points(line, precise_points, 3);
    
    // 应用新的点坐标到弧线
    lv_line_set_points(line, p, 3);
}



void create_breathing_eye_on_screen(lv_obj_t* scr) {
    if (!scr) return;

    
    lv_anim_del(scr, NULL);
    lv_obj_clean(scr); 
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // 2. 创建代表“闭合眼睑”的弧线 (使用lv_line对象实现)
    lv_obj_t * eyelid = lv_line_create(scr);

    // 3. 设置弧线的样式
    lv_obj_set_style_line_width(eyelid, 8, 0);                   // 线宽为8像素
    lv_obj_set_style_line_color(eyelid, lv_color_white(), 0);    // 线条颜色为白色
    lv_obj_set_style_line_rounded(eyelid, true, 0);              // 线条端点为圆形
    lv_obj_center(eyelid);                                       // 将弧线对象居中

    // 4. 创建并配置动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, eyelid);
    lv_anim_set_values(&a, 0, 10);            // 动画值从0变化到10
    lv_anim_set_time(&a, 2500);               // “吸气”过程(0->10)持续2.5秒
    lv_anim_set_playback_time(&a, 2500);      // “呼气”过程(10->0)持续2.5秒
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); // 无限重复
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);     // 使用平滑的动画路径
    lv_anim_set_exec_cb(&a, breathing_arc_cb);             // 设置执行回调函数
    
    // 启动动画
    lv_anim_start(&a);
}


void create_dual_breathing_eye_animation(lv_obj_t* parent_left, lv_obj_t* parent_right) {
    if (parent_left) {
        create_breathing_eye_on_screen(parent_left);
    }
    if (parent_right) {
        create_breathing_eye_on_screen(parent_right);
    }
}
// ================================================================



// ================================================================

// 单例模式实现：获取 EmotionManager 实例
EmotionManager& EmotionManager::GetInstance() {
    static EmotionManager instance;  // 静态局部变量，确保只创建一次
    return instance;
}

EmotionManager::EmotionManager() : default_animation_("default", true) 
{
    // 创建一个临时的动画对象，用于定义默认动画的内容
    Animation sleep_anim("default", true); 
    sleep_anim.AddFrame(&sleep0, &sleep0, 200);
    sleep_anim.AddFrame(&sleep1, &sleep1, 200);  
    sleep_anim.AddFrame(&sleep2, &sleep2, 200);  
    sleep_anim.AddFrame(&sleep3, &sleep3, 500); 
    sleep_anim.AddFrame(&sleep2, &sleep2, 200);
    sleep_anim.AddFrame(&sleep1, &sleep1, 200);
    
    // 将初始化好的动画内容赋值给成员变量
    default_animation_ = sleep_anim;
    
    // 创建表情队列
    emotion_queue_ = xQueueCreate(10, sizeof(EmotionMessage));
    if (emotion_queue_ == nullptr) {
        ESP_LOGE(TAG, "创建表情队列失败");
        return;
    }
    
    // 创建表情处理任务
    xTaskCreatePinnedToCore(
        EmotionTaskWrapper, "emotion_task", 4096, this, 5, &emotion_task_handle_, 1);
    
    ESP_LOGI(TAG, "表情队列系统初始化完成");
}

EmotionManager::~EmotionManager() {
    if (emotion_task_handle_ != nullptr) vTaskDelete(emotion_task_handle_);
    if (emotion_queue_ != nullptr) vQueueDelete(emotion_queue_);
}

void EmotionManager::EmotionTaskWrapper(void* param) {
    static_cast<EmotionManager*>(param)->EmotionTask();
}

void EmotionManager::EmotionTask() {
    EmotionMessage msg;
    ESP_LOGI(TAG, "表情处理任务启动");
    while (true) {
        if (xQueueReceive(emotion_queue_, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGD(TAG, "处理表情请求: %s", msg.emotion_name);
            
            auto display = Board::GetInstance().GetDisplay();
            if (display != nullptr) {
                const Animation& animation = GetAnimation(std::string(msg.emotion_name));
                
                // 使用虚函数，避免类型转换
                display->PlayAnimation(animation);
            } else {
                ESP_LOGW(TAG, "获取显示器实例失败");
            }
        }
    }
}

void EmotionManager::ProcessEmotionAsync(const char* emotion_name) {
    if (emotion_queue_ == nullptr || emotion_name == nullptr) {
        ESP_LOGW(TAG, "表情队列未初始化或表情名称为空");
        return;
    }
    
    EmotionMessage msg;
    strncpy(msg.emotion_name, emotion_name, sizeof(msg.emotion_name) - 1);
    msg.emotion_name[sizeof(msg.emotion_name) - 1] = '\0';
    msg.timestamp = xTaskGetTickCount();
    
    if (xQueueSend(emotion_queue_, &msg, 0) != pdTRUE) {
        EmotionMessage old_msg;
        if (xQueueReceive(emotion_queue_, &old_msg, 0) == pdTRUE) {
            ESP_LOGD(TAG, "丢弃旧表情请求: %s", old_msg.emotion_name);
            xQueueSend(emotion_queue_, &msg, 0);
        } else {
            ESP_LOGW(TAG, "表情队列满，丢弃请求: %s", emotion_name);
        }
    }
}

const Animation& EmotionManager::GetAnimation(const std::string& emotion_name) {
    auto it = animations_.find(emotion_name);
    if (it != animations_.end()) {
        return it->second;
    }
    ESP_LOGW(TAG, "未找到表情动画 '%s'，使用默认表情", emotion_name.c_str());
    return default_animation_;
}

void EmotionManager::PreloadAllAnimations() {
    ESP_LOGI(TAG, "开始预加载所有表情动画...");
    InitializeAnimations();    
    ESP_LOGI(TAG, "表情动画预加载完成，共加载 %d 个动画", animations_.size());
}

// 关键修改：只保留一个 RegisterAnimation 函数定义
// void EmotionManager::RegisterAnimation(const std::string& emotion_name, const Animation& animation) {
//     if (!animation.IsValid()) {     
//         ESP_LOGE(TAG, "尝试注册无效的动画: %s", emotion_name.c_str());
//         return;
//     }
    
//     animations_.insert({emotion_name, animation});
    
//     if (animation.type == Animation::Type::IMAGE_SEQUENCE && animation.image_sequence.frames) {
//         ESP_LOGD(TAG, "注册表情动画: %s (帧数: %d)", emotion_name.c_str(), animation.image_sequence.frames->size());
//     } else {
//         ESP_LOGD(TAG, "注册表情动画: %s (程序化动画)", emotion_name.c_str());
//     }
// }

void EmotionManager::RegisterAnimation(const std::string& emotion_name, const Animation& animation) {
    if (!animation.IsValid()) {     
        ESP_LOGE(TAG, "尝试注册无效的动画: %s", emotion_name.c_str());
        return;
    }

    // 【修改】使用 emplace 或下标赋值，更安全高效
    animations_.emplace(emotion_name, animation);

    // 【修改】使用 std::get_if 访问 variant
    if (const auto* seq_data = std::get_if<ImageSequenceData>(&animation.data)) {
        ESP_LOGD(TAG, "注册表情动画: %s (帧数: %d)", emotion_name.c_str(), seq_data->frames.size());
    } else {
        ESP_LOGD(TAG, "注册表情动画: %s (程序化动画)", emotion_name.c_str());
    }
}

bool EmotionManager::HasAnimation(const std::string& emotion_name) const {
    return animations_.find(emotion_name) != animations_.end();
}

const Animation& EmotionManager::GetDefaultAnimation() const {
    return default_animation_;
}

void EmotionManager::InitializeAnimations() {

    // 基础静态表情
    RegisterAnimation("neutral", CreateStaticEmotion("neutral", &Black , &Black ));
   // RegisterAnimation("happy", CreateStaticEmotion("happy", &happy, &happy));
   // RegisterAnimation("sad", CreateStaticEmotion("sad", &crying_l, &crying_r));
   // RegisterAnimation("funny", CreateStaticEmotion("funny", &funny, &funny));
        
    // 添加眼睛状态表情 - 新增这两行
    //RegisterAnimation("closed_eyes", CreateStaticEmotion("closed_eyes", &biyan, &biyan));  // 闭眼状态
    //RegisterAnimation("open_eyes", CreateYanzhuAnimation());  // 睁眼状态使用yanzhu动画
    
    // 添加listen表情
    //RegisterAnimation("listen", CreateStaticEmotion("listen", &listen_l, &listen_r));
    
    // 对称表情（左右眼相同）
    //RegisterAnimation("laughing", CreateStaticEmotion("laughing", &funny, &funny));
    //RegisterAnimation("angry", CreateStaticEmotion("angry", &neutral, &neutral));  // 暂时用neutral代替
    //RegisterAnimation("crying", CreateStaticEmotion("crying", &crying_l, &crying_r));
    //RegisterAnimation("loving", CreateStaticEmotion("loving", &happy, &happy));  // 暂时用happy代替
    //RegisterAnimation("embarrassed", CreateStaticEmotion("embarrassed", &neutral, &neutral));
    //RegisterAnimation("surprised", CreateStaticEmotion("surprised", &zhenyan , &zhenyan ));
    //RegisterAnimation("shockeinkid", CreateStaticEmotion("shocked", &zhenyan , &zhenyan ));
    //RegisterAnimation("thng", CreateStaticEmotion("thinking", &neutral, &neutral));
    //RegisterAnimation("cool", CreateStaticEmotion("cool", &neutral, &neutral));
    //RegisterAnimation("relaxed", CreateStaticEmotion("relaxed", &biyan , &biyan ));
    // RegisterAnimation("delicious", CreateStaticEmotion("delicious", &happy, &happy));
    // RegisterAnimation("kissy", CreateStaticEmotion("kissy", &happy, &happy));
    //RegisterAnimation("confident", CreateStaticEmotion("confident", &zhenyan , &zhenyan ));
    // RegisterAnimation("silly", CreateStaticEmotion("silly", &funny, &funny));
    //RegisterAnimation("confused", CreateStaticEmotion("confused", &neutral, &neutral));
    //RegisterAnimation("winking", CreateWinkingAnimation());
    
    // 特殊动画：眨眼循环
    RegisterAnimation("blinking", CreateBlinkingAnimation());

    // 新增yanzhu动画
    RegisterAnimation("yanzhu", CreateYanzhuAnimation());

    //新增睡眠动画
    RegisterAnimation("sleep", CreateSleepAnimation());

    //眼珠缩放动画
    RegisterAnimation("eyeball", CreateYanzhuScaleAnimation());

    // 新增微笑动画
    RegisterAnimation("smile", CreateSmileAnimation());
    
    // 添加程序化orbiting动画注册
    RegisterAnimation("orbiting", Animation("orbiting", create_dual_orbiting_eye_animation));
    
    // 修复：使用正确的双眼缩放动画函数
    RegisterAnimation("listening", Animation("listening", create_dual_scaling_eye_animation));

     RegisterAnimation("close_eye", Animation("close_eye", create_dual_breathing_eye_animation));
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

// 创建眼动动画序列
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
    Animation animation("smile", true);  // 不循环播放
    
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

//睡眠动画
Animation EmotionManager:: CreateSleepAnimation(){ 
    Animation animation("sleep", false); 

    animation.AddFrame(&sleep0, &sleep0, 200);  
    animation.AddFrame(&sleep1, &sleep1, 200);  
    animation.AddFrame(&sleep2, &sleep2, 200);  
    animation.AddFrame(&sleep3, &sleep3, 500); 
    animation.AddFrame(&sleep2, &sleep2, 200);
    animation.AddFrame(&sleep1, &sleep1, 200);

    return animation;
 
}


