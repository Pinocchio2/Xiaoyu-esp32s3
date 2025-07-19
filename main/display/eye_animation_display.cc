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
    screen_ = nullptr;
    
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
    EyeAnimationDisplay* self = static_cast<EyeAnimationDisplay*>(arg);
    if (self && self->animation_task_handle_) {
        // 从中断上下文发送任务通知
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(self->animation_task_handle_, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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

bool EyeAnimationDisplay::PlayAnimation(const Animation& animation) {
    if (!animation.IsValid()) {
        ESP_LOGW(TAG, "无效的动画");
        return false;
    }
    
    if (!left_eye_img_ || !right_eye_img_) {
        ESP_LOGE(TAG, "眼睛图像对象未初始化");
        return false;
    }
    
    // 修复：使用 static_cast 转换 size() 为 int
    ESP_LOGI(TAG, "开始播放动画: %s，帧数: %d", animation.name.c_str(), static_cast<int>(animation.frames.size()));
    
    // 停止之前的动画
    StopAnimation();
    
    current_animation_ = &animation;
    current_frame_index_ = 0;  // 使用正确的成员变量名
    is_looping_ = animation.loop;  // 使用新增的成员变量
    
    // 创建动画定时器（只创建一次，避免频繁创建删除）
    if (!animation_timer_) {
        esp_timer_create_args_t timer_args = {
            .callback = &EyeAnimationDisplay::animation_timer_callback,
            .arg = this,
            .name = "eye_anim_timer"
        };
        
        esp_err_t ret = esp_timer_create(&timer_args, &animation_timer_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "创建动画定时器失败: %s", esp_err_to_name(ret));
            return false;
        }
    }
    
    // 立即显示第一帧
    PlayNextFrame();
    
    return true;
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

void EyeAnimationDisplay::PlayNextFrame() {
    if (!current_animation_ || current_frame_index_ >= static_cast<int>(current_animation_->frames.size())) {
        ESP_LOGW(TAG, "无效的动画状态");
        return;
    }

    const auto& frame = current_animation_->frames[current_frame_index_];

    // 优化：使用静态缓存，避免频繁内存分配
    if (left_eye_img_ && frame.left_eye_image) {
        left_eye_data_.img_obj = left_eye_img_;
        left_eye_data_.img_src = frame.left_eye_image;
        lv_async_call(update_image_callback, &left_eye_data_);
    }

    if (right_eye_img_ && frame.right_eye_image) {
        right_eye_data_.img_obj = right_eye_img_;
        right_eye_data_.img_src = frame.right_eye_image;
        lv_async_call(update_image_callback, &right_eye_data_);
    }

    ESP_LOGD(TAG, "提交第 %d 帧更新请求，持续时间: %u ms", 
             current_frame_index_, (unsigned int)frame.duration_ms);
    
    // 准备下一帧
    current_frame_index_++;
    
    // 检查是否需要循环或停止
    if (current_frame_index_ >= static_cast<int>(current_animation_->frames.size())) {
        if (is_looping_) {
            current_frame_index_ = 0;  // 重新开始
            ESP_LOGD(TAG, "动画循环，重新开始");
        } else {
            // 动画结束，停止播放
            StopAnimation();
            ESP_LOGD(TAG, "动画播放完成");
            return;
        }
    }
    
    // 启动下一帧定时器
    if (animation_timer_) {
        esp_timer_start_once(animation_timer_, frame.duration_ms * 1000);  // 转换为微秒
    } else {
        ESP_LOGE(TAG, "动画定时器未初始化");
    }
}

void EyeAnimationDisplay::StopAnimation() {
    // 只停止定时器，不删除，避免频繁创建删除
    if (animation_timer_) {
        esp_timer_stop(animation_timer_);
        // 不删除定时器，保留供下次使用
    }
    current_animation_ = nullptr;
    current_frame_index_ = 0;  // 修复：使用正确的成员变量名
    is_looping_ = false;       // 修复：使用正确的成员变量名
}

// void EyeAnimationDisplay::animation_timer_callback(void* arg) {
//     EyeAnimationDisplay* self = static_cast<EyeAnimationDisplay*>(arg);
//     if (self) {
//         self->PlayNextFrame();
//     }
// }

bool EyeAnimationDisplay::Lock(int timeout_ms) {
    // 使用LVGL端口锁定
    return lvgl_port_lock(timeout_ms);
}

void EyeAnimationDisplay::Unlock() {
    // 使用LVGL端口解锁
    lvgl_port_unlock();
}

/**
 * @brief 设置情感表情 - 眼睛动画实现
 * @param emotion 情感名称
 */
void EyeAnimationDisplay::SetEmotion(const char* emotion) {
    ESP_LOGI(TAG, "设置眼睛表情: %s", emotion ? emotion : "null");
    
    if (emotion == nullptr) {
        ESP_LOGW(TAG, "表情名称为空，使用默认表情");
        emotion = "neutral";
    }
    
    // 从 EmotionManager 获取动画
    const Animation& animation = EmotionManager::GetInstance().GetAnimation(std::string(emotion));
    
    // 播放动画
    if (!PlayAnimation(animation)) {
        ESP_LOGE(TAG, "播放表情动画失败: %s", emotion);
        // 尝试播放默认动画
        PlayAnimation(EmotionManager::GetInstance().GetDefaultAnimation());
    }
}