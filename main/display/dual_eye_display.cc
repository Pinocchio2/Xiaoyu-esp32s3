#include "dual_eye_display.h"
#include "esp_log.h"

static const char* TAG = "DualEyeDisplay";

DualEyeDisplay::DualEyeDisplay() : animation_system_(nullptr) {
    ESP_LOGI(TAG, "初始化双眼显示系统");
    
    // 创建动画系统实例
    animation_system_ = new DualAnimation();
    if (!animation_system_) {
        ESP_LOGE(TAG, "动画系统创建失败");
        return;
    }
    
    ESP_LOGI(TAG, "双眼显示系统初始化完成");
}

DualEyeDisplay::~DualEyeDisplay() {
    if (animation_system_) {
        delete animation_system_;
        animation_system_ = nullptr;
    }
}

void DualEyeDisplay::SetEmotion(const char* emotion) {
    if (!emotion || !animation_system_) {
        ESP_LOGW(TAG, "参数无效");
        return;
    }
    
    ESP_LOGI(TAG, "设置表情: %s", emotion);
    animation_system_->ShowAnimation(emotion);
}

void DualEyeDisplay::PlayAnimation(const char* name) {
    if (!name || !animation_system_) {
        ESP_LOGW(TAG, "参数无效");
        return;
    }
    
    animation_system_->ShowAnimation(name);
}

void DualEyeDisplay::PlayAnimationNow(const char* name) {
    if (!name || !animation_system_) {
        ESP_LOGW(TAG, "参数无效");
        return;
    }
    
    animation_system_->ShowAnimationNow(name);
}