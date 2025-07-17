#include "eye_display.h"
#include "../boards/yuwell-xiaoyu-esp32s3-double-lcd/dual_display_manager.h"
#include <esp_log.h>

static const char* TAG = "EyeDisplay";

// 修复构造函数 - 移除 DisplayFonts 参数
EyeDisplay::EyeDisplay(DualDisplayManager* dual_display_manager)
    : Display(),  // 调用无参构造函数
      dual_display_manager_(dual_display_manager),
      current_frame_index_(0),
      animation_timer_(nullptr),
      is_playing_animation_(false) {
    
    // 创建动画定时器
    esp_timer_create_args_t timer_args = {
        .callback = AnimationTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "eye_animation_timer"
    };
    
    esp_err_t ret = esp_timer_create(&timer_args, &animation_timer_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create animation timer: %s", esp_err_to_name(ret));
    }
}

EyeDisplay::~EyeDisplay() {
    // 停止当前动画
    StopCurrentAnimation();
    
    // 如果动画定时器存在，则删除
    if (animation_timer_) {
        esp_timer_delete(animation_timer_);
        animation_timer_ = nullptr;
    }
}

// 修复 PlayAnimation 函数 - 添加返回值
bool EyeDisplay::PlayAnimation(const Animation& animation) {
    // 打印日志信息，显示当前动画的帧数
    ESP_LOGI(TAG, "Playing animation with %zu frames", animation.frames.size());
    
    // 停止当前动画
    StopCurrentAnimation();
    
    // 设置新动画
    current_animation_ = animation;
    current_frame_index_ = 0;
    is_playing_animation_ = true;
    
    // 播放第一帧
    PlayNextFrame();
    
    return true;  // 添加返回值
}

void EyeDisplay::PlayNextFrame() {
    if (!is_playing_animation_ || current_animation_.frames.empty()) {
        return;
    }
    
    const AnimationFrame& frame = current_animation_.frames[current_frame_index_];
    
    // 修复日志格式 - 使用正确的格式说明符
    ESP_LOGD(TAG, "Playing frame %zu/%zu, duration: %lu ms", 
             current_frame_index_ + 1, current_animation_.frames.size(), 
             (unsigned long)frame.duration_ms);
    
    // 设置左右眼图像
    if (dual_display_manager_) {
        dual_display_manager_->SetImage(true, frame.left_eye_image);   // 主屏（左眼）
        dual_display_manager_->SetImage(false, frame.right_eye_image); // 副屏（右眼）
    }
    
    // 准备下一帧
    current_frame_index_++;
    
    // 检查是否需要循环或停止
    if (current_frame_index_ >= current_animation_.frames.size()) {
        if (current_animation_.loop) {
            current_frame_index_ = 0; // 重新开始
        } else {
            is_playing_animation_ = false;
            return; // 动画结束
        }
    }
    
    // 设置下一帧的定时器
    if (animation_timer_ && frame.duration_ms > 0) {
        esp_err_t ret = esp_timer_start_once(animation_timer_, frame.duration_ms * 1000); // 转换为微秒
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start animation timer: %s", esp_err_to_name(ret));
            is_playing_animation_ = false;
        }
    }
}

void EyeDisplay::StopCurrentAnimation() {
    if (animation_timer_) {
        esp_timer_stop(animation_timer_);
    }
    is_playing_animation_ = false;
    current_frame_index_ = 0;
}

void EyeDisplay::AnimationTimerCallback(void* arg) {
    EyeDisplay* eye_display = static_cast<EyeDisplay*>(arg);
    if (eye_display) {
        eye_display->PlayNextFrame();
    }
}

// 重写基类方法，委托给主显示屏
bool EyeDisplay::Lock(int timeout_ms) {
    if (dual_display_manager_ && dual_display_manager_->GetPrimaryDisplay()) {
        // 需要将 Lock 方法设为 public 或使用友元类
        // 暂时返回 true，具体实现需要修改 Display 基类
        return true;
    }
    return false;
}

void EyeDisplay::Unlock() {
    if (dual_display_manager_ && dual_display_manager_->GetPrimaryDisplay()) {
        // 需要将 Unlock 方法设为 public 或使用友元类
        // 暂时为空实现，具体实现需要修改 Display 基类
    }
}

lv_disp_t* EyeDisplay::getLvDisplay() {
    if (dual_display_manager_ && dual_display_manager_->GetPrimaryDisplay()) {
        return dual_display_manager_->GetPrimaryDisplay()->getLvDisplay();
    }
    return nullptr;
}