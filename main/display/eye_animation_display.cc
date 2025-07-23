//#include "display.h"
#include "eye_animation_display.h"
#include "emotion_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
// 添加必要的头文件包含
#include "../boards/yuwell-xiaoyu-esp32s3-double-lcd/dual_display_manager.h"
#include "lcd_display.h"

static const char* TAG = "EyeAnimationDisplay";

// 声明全局双屏管理器引用
extern DualDisplayManager* g_dual_display_manager;


void EyeAnimationDisplay::StopAnimation() {
    DisplayLockGuard lock(this); // 确保所有LVGL操作线程安全

    // 1. 停止图片轮播的定时器
    if (animation_timer_ && esp_timer_is_active(animation_timer_)) {
        esp_timer_stop(animation_timer_);
    }
    current_animation_ = nullptr;
    current_frame_index_ = 0;
    is_looping_ = false;

    // 2. 如果之前是程序化动画，只清理屏幕上的临时对象
    if (is_programmatic_anim_active_) {
        ESP_LOGD(TAG, "清理程序化动画...");
       
        if (primary_display_) {
            lv_obj_clean(lv_disp_get_scr_act(primary_display_->getLvDisplay()));
            // 因为 clean 了，所以 img 对象也没了
            left_eye_img_ = nullptr; 
        }
        if (secondary_display_) {
            lv_obj_clean(lv_disp_get_scr_act(secondary_display_->getLvDisplay()));
            right_eye_img_ = nullptr;
        }
        is_programmatic_anim_active_ = false;
    }
}

/**
 * @brief 播放动画（已重构为调度器）
 */
bool EyeAnimationDisplay::PlayAnimation(const Animation& animation) {
    if (!animation.IsValid()) {
        ESP_LOGW(TAG, "无效的动画");
        return false;
    }

    DisplayLockGuard lock(this);
    
    // 统一停止上一个动画 - 移到这里确保所有分支都会执行
    StopAnimation();

    //ESP_LOGI(TAG, "请求播放动画: %s (类型: %s)", animation.name.c_str(),
            // animation.type == Animation::Type::IMAGE_SEQUENCE ? "图片序列" : "程序化");
             
    lv_obj_t* scr_left = primary_display_ ? lv_disp_get_scr_act(primary_display_->getLvDisplay()) : nullptr;
    lv_obj_t* scr_right = secondary_display_ ? lv_disp_get_scr_act(secondary_display_->getLvDisplay()) : nullptr;

    if (!scr_left || !scr_right) return false;

    // 根据动画类型选择播放逻辑
    if (animation.type == Animation::Type::LVGL_PROGRAMMATIC) {
        // 确保停止所有之前的动画和定时器
        is_programmatic_anim_active_ = true;
        if (animation.programmatic.creator_func) {
             animation.programmatic.creator_func(scr_left, scr_right);
        }

    } else {
        is_programmatic_anim_active_ = false;
        
        // **关键修正**: 如果img对象不存在 (可能被上次的程序化动画清除了)，则重新创建它们
        if (!left_eye_img_) {
            left_eye_img_ = lv_img_create(scr_left);
            lv_obj_align(left_eye_img_, LV_ALIGN_CENTER, 0, 0);
            //ESP_LOGI(TAG, "重新创建左眼图像对象");
        }
        if (!right_eye_img_) {
            right_eye_img_ = lv_img_create(scr_right);
            lv_obj_align(right_eye_img_, LV_ALIGN_CENTER, 0, 0);
            ///ESP_LOGI(TAG, "重新创建右眼图像对象");
        }
        
        // 确保它们可见
        lv_obj_clear_flag(left_eye_img_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(right_eye_img_, LV_OBJ_FLAG_HIDDEN);
        
        
        current_animation_ = &animation;
        is_looping_ = animation.loop;
        current_frame_index_ = 0;
        
        if (!animation_timer_) {
            esp_timer_create_args_t timer_args = {
                .callback = &EyeAnimationDisplay::animation_timer_callback,
                .arg = this, .name = "eye_anim_timer"
            };
            esp_timer_create(&timer_args, &animation_timer_);
        }
        
        // 立即启动第一帧 (通过任务通知)
        if(animation_task_handle_) {
            xTaskNotifyGive(animation_task_handle_);
        }
    }
    
    return true;
}



// 在构造函数中创建任务
EyeAnimationDisplay::EyeAnimationDisplay() {
    ESP_LOGI(TAG, "初始化眼睛动画显示");
    
    // 确保LVGL已经初始化
    if (!lv_is_initialized()) {
        ESP_LOGE(TAG, "LVGL未初始化");
        return;
    }
    
    // 获取双屏管理器
    if (!g_dual_display_manager) {
        ESP_LOGE(TAG, "双屏管理器未初始化");
        return;
    }
    
    // 获取主屏幕和副屏幕
    Display* primary_display = g_dual_display_manager->GetPrimaryDisplay();
    Display* secondary_display = g_dual_display_manager->GetSecondaryDisplay();
    
    if (!primary_display || !secondary_display) {
        ESP_LOGE(TAG, "无法获取双屏显示对象");
        return;
    }
    
    // 在主屏幕创建左眼图像对象
    {
        DisplayLockGuard lock(primary_display);
        // 修复类型转换 - 使用正确的语法
        LcdDisplay* lcd_primary = static_cast<LcdDisplay*>(primary_display);
        lv_disp_t* primary_disp = lcd_primary->getLvDisplay();
        if (primary_disp) {
            lv_obj_t* primary_screen = lv_disp_get_scr_act(primary_disp);
            left_eye_img_ = lv_img_create(primary_screen);
            if (left_eye_img_) {
                lv_obj_align(left_eye_img_, LV_ALIGN_CENTER, 0, 0);
                // 设置背景透明
                lv_obj_set_style_bg_opa(left_eye_img_, LV_OPA_TRANSP, 0);
                ESP_LOGI(TAG, "左眼图像对象在主屏幕创建成功");
            }
        }
    }
    
    // 在副屏幕创建右眼图像对象
    {
        DisplayLockGuard lock(secondary_display);
        // 修复类型转换 - 使用正确的语法
        LcdDisplay* lcd_secondary = static_cast<LcdDisplay*>(secondary_display);
        lv_disp_t* secondary_disp = lcd_secondary->getLvDisplay();
        if (secondary_disp) {
            lv_obj_t* secondary_screen = lv_disp_get_scr_act(secondary_disp);
            right_eye_img_ = lv_img_create(secondary_screen);
            if (right_eye_img_) {
                lv_obj_align(right_eye_img_, LV_ALIGN_CENTER, 0, 0);
                // 设置背景透明
                lv_obj_set_style_bg_opa(right_eye_img_, LV_OPA_TRANSP, 0);
                ESP_LOGI(TAG, "右眼图像对象在副屏幕创建成功");
            }
        }
    }
    
    // 存储显示对象引用
    primary_display_ = primary_display;
    secondary_display_ = secondary_display;
    
    // 清空screen_成员变量，因为我们现在使用双屏
   // 由于我们现在使用双屏显示,不再需要单个screen_变量,所以这行可以删除
    
    // 创建动画任务
    xTaskCreate(
        animation_task,
        "eye_anim_task",
        4096,  // 栈大小
        this,  // 参数
        3,     // 优先级
        &animation_task_handle_
    );
}

// 任务函数实现
void EyeAnimationDisplay::animation_task(void* pvParameters) {
    EyeAnimationDisplay* self = static_cast<EyeAnimationDisplay*>(pvParameters);
    
    while (true) {
        // 等待任务通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // 在任务上下文中安全地播放下一帧
        if (self) {
            self->PlayNextFrame();
        }
    }
}

// 修改定时器回调
void EyeAnimationDisplay::animation_timer_callback(void* arg) {
    EyeAnimationDisplay* display = static_cast<EyeAnimationDisplay*>(arg);
    
    // 添加程序化动画检查，避免调用图片序列动画的逻辑
    if (display->is_programmatic_anim_active_) {
        return; // 程序化动画不需要定时器回调
    }
    
    if (display->animation_task_handle_) {
        xTaskNotifyGive(display->animation_task_handle_);
    }
}

// 在析构函数中删除任务
EyeAnimationDisplay::~EyeAnimationDisplay() {
    ESP_LOGI(TAG, "销毁眼睛动画显示");
    
    // 停止动画定时器
    StopAnimation();
    
    // 删除任务
    if (animation_task_handle_) {
        vTaskDelete(animation_task_handle_);
        animation_task_handle_ = nullptr;
    }
    
    // 删除定时器（只在析构时删除）
    if (animation_timer_) {
        esp_timer_delete(animation_timer_);
        animation_timer_ = nullptr;
    }
    
    // 清理LVGL对象
    DisplayLockGuard lock(this);
    if (left_eye_img_) {
        lv_obj_del(left_eye_img_);
        left_eye_img_ = nullptr;
    }
    if (right_eye_img_) {
        lv_obj_del(right_eye_img_);
        right_eye_img_ = nullptr;
    }
}


// 静态成员定义
EyeAnimationDisplay::ImageUpdateData EyeAnimationDisplay::left_eye_data_;
EyeAnimationDisplay::ImageUpdateData EyeAnimationDisplay::right_eye_data_;

// 优化的静态回调函数
void EyeAnimationDisplay::update_image_callback(void* user_data) {
    ImageUpdateData* data = static_cast<ImageUpdateData*>(user_data);
    if (data && data->img_obj && data->img_src) {
        lv_img_set_src(data->img_obj, data->img_src);
    }
}

// in file: main/display/eye_animation_display.cc

void EyeAnimationDisplay::PlayNextFrame() {
    // 步骤 1: 加上之前建议的锁，这是保证线程安全的前提
    DisplayLockGuard lock(this);

    // 步骤 2: 检查当前动画状态，这部分逻辑不变
    if (!current_animation_ ||
        current_animation_->type != Animation::Type::IMAGE_SEQUENCE ||
        !current_animation_->image_sequence.frames ||
        current_frame_index_ >= static_cast<int>(current_animation_->image_sequence.frames->size())) {
        ESP_LOGW(TAG, "无效的动画状态");
        return;
    }

    const auto& frame = (*current_animation_->image_sequence.frames)[current_frame_index_];

    // 步骤 3: 【核心修改】直接调用 lv_img_set_src, 替换掉原来的 lv_async_call
    if (frame.left_eye_image && left_eye_img_) {
        lv_img_set_src(left_eye_img_, frame.left_eye_image);
    }

    if (frame.right_eye_image && right_eye_img_) {
        lv_img_set_src(right_eye_img_, frame.right_eye_image);
    }

    // 步骤 4: 播放完当前帧后，启动下一帧的定时器，逻辑不变
    if (frame.duration_ms > 0) {
        esp_timer_start_once(animation_timer_, frame.duration_ms * 1000);
    }

    current_frame_index_++;

    // 步骤 5: 判断动画是否结束，逻辑不变
    if (current_frame_index_ >= static_cast<int>(current_animation_->image_sequence.frames->size())) {
        if (is_looping_) {
            current_frame_index_ = 0; // 循环播放则重置索引
        } else {
            StopAnimation(); // 不循环则停止动画
        }
    }
}


bool EyeAnimationDisplay::Lock(int timeout_ms) {
    // 使用LVGL端口锁定
    return lvgl_port_lock(timeout_ms);
}

void EyeAnimationDisplay::Unlock() {
    // 使用LVGL端口解锁
    lvgl_port_unlock();
}


void EyeAnimationDisplay::SetEmotion(const char* emotion) {
    // 直接委托给EmotionManager的异步处理方法
    EmotionManager::GetInstance().ProcessEmotionAsync(emotion);
}
// {
//    // ESP_LOGI(TAG, "设置眼睛表情: %s", emotion ? emotion : "null");
    
//     if (emotion == nullptr) {
//         ESP_LOGW(TAG, "表情名称为空，使用默认表情");
//         emotion = "neutral";
//     }
    
//     // 从 EmotionManager 获取动画
//     const Animation& animation = EmotionManager::GetInstance().GetAnimation(std::string(emotion));
    
//     // 播放动画
//     if (!PlayAnimation(animation)) {
//         ESP_LOGE(TAG, "播放表情动画失败: %s", emotion);
//         // 尝试播放默认动画
//         PlayAnimation(EmotionManager::GetInstance().GetDefaultAnimation());
//     }
// }