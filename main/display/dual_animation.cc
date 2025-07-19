// [NEW AND CORRECTED FILE CONTENT]

#include "dual_animation.h"
#include <cstring>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "ui/eye.h" // 包含现有的图像资源声明
#include "boards/yuwell-xiaoyu-esp32s3-double-lcd/dual_display_manager.h"

extern DualDisplayManager* g_dual_display_manager;

#define TAG "DualAnimation"

// --- 修复：使用项目中实际存在的图像 ---
// 眨眼动画使用zhayang系列图像
const lv_img_dsc_t* blink_anim_images[] = {&zhayang1, &zhayang2, &zhayang3, &zhayang4, &zhayang3, &zhayang2, &zhayang1, nullptr};
// 闭眼状态使用biyan图像
const lv_img_dsc_t* closed_eyes_images[] = {&biyan, nullptr};
// 开心表情使用happy图像
const lv_img_dsc_t* happy_images[] = {&happy, nullptr};
// 中性表情使用neutral图像
const lv_img_dsc_t* sad_images[] = {&neutral, nullptr};
// 思考表情使用zhenyan图像
const lv_img_dsc_t* think_images[] = {&zhenyan, nullptr};
// 愤怒表情使用funny图像（临时替代）
const lv_img_dsc_t* angry_images[] = {&funny, nullptr};
// 微笑动画使用yanzhu系列图像
const lv_img_dsc_t* smile_anim_images[] = {&yanzhu1, &yanzhu2, &yanzhu3, &yanzhu4, nullptr};

// --- 动画配置列表 ---
const std::vector<emotions_anim_t> emotions_anim = {
    {{blink_anim_images, "yanzhu", nullptr, 7, 100}, "yanzhu"},        // 眨眼动画，7帧，每帧100ms
    {{blink_anim_images, "wakeup", nullptr, 7, 100}, "wakeup"},        // 唤醒动画
    {{closed_eyes_images, "closed_eyes", nullptr, 1, 0}, "closed_eyes"}, // 闭眼状态
    {{closed_eyes_images, "sleep", nullptr, 1, 0}, "sleep"},           // 睡眠状态
    {{happy_images, "happy", nullptr, 1, 0}, "happy"},                 // 开心表情
    {{sad_images, "sad", nullptr, 1, 0}, "sad"},                       // 悲伤表情
    {{think_images, "think", nullptr, 1, 0}, "think"},                 // 思考表情
    {{angry_images, "angry", nullptr, 1, 0}, "angry"},                 // 愤怒表情
    {{nullptr, "shark", shark_animation, 1, 2000}, "shark"},            // 鲨鱼动画（自定义函数）
    {{smile_anim_images, "smile", nullptr, 4, 200}, "smile"},          // 微笑动画，4帧，每帧200ms
};

void DualAnimation::animation_task(void *param) {
    DualAnimation* self = static_cast<DualAnimation*>(param);
    uint32_t delay_time = 0;

    while (1) {
        if (!self->anim_queue_.empty()) {
            dual_anim_t anim = self->anim_queue_.front();
            self->anim_queue_.pop_front();
            delay_time = anim.duration;

            ESP_LOGI(TAG, "Running animation: %s", anim.name);

            if (g_dual_display_manager == nullptr) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            
            lv_obj_clean(g_dual_display_manager->GetPrimaryImgObj());
            lv_obj_clean(g_dual_display_manager->GetSecondaryImgObj());

            if (anim.callback) {
                delay_time = anim.callback(anim.count, anim.duration);
            } else if (anim.images) {
                lv_animimg_set_src(g_dual_display_manager->GetPrimaryImgObj(), (const void**)anim.images, anim.count);
                lv_animimg_set_duration(g_dual_display_manager->GetPrimaryImgObj(), anim.duration);
                lv_animimg_set_repeat_count(g_dual_display_manager->GetPrimaryImgObj(), LV_ANIM_REPEAT_INFINITE);
                lv_animimg_start(g_dual_display_manager->GetPrimaryImgObj());
                
                lv_animimg_set_src(g_dual_display_manager->GetSecondaryImgObj(), (const void**)anim.images, anim.count);
                lv_animimg_set_duration(g_dual_display_manager->GetSecondaryImgObj(), anim.duration);
                lv_animimg_set_repeat_count(g_dual_display_manager->GetSecondaryImgObj(), LV_ANIM_REPEAT_INFINITE);
                lv_animimg_start(g_dual_display_manager->GetSecondaryImgObj());
            }

            if (delay_time > 0) {
                vTaskDelay(pdMS_TO_TICKS(delay_time));
            }

        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    vTaskDelete(NULL);
}

DualAnimation::DualAnimation() {
    xTaskCreate(animation_task, "anim_task", 4096, this, 5, NULL);
}

void DualAnimation::ShowAnimation(const char* emotion) {
    const dual_anim_t* anim = get_anim_by_name(emotion);
    if(anim) anim_queue_.push_back(*anim);
}

void DualAnimation::ShowAnimationNow(const char* emotion) {
    anim_queue_.clear();
    const dual_anim_t* anim = get_anim_by_name(emotion);
    if(anim) anim_queue_.push_back(*anim);
}

const dual_anim_t* DualAnimation::get_anim_by_name(const char* name) {
    std::string_view emotion_view(name);
    auto it = std::find_if(emotions_anim.begin(), emotions_anim.end(),
        [&emotion_view](const emotions_anim_t& e) { return e.text == emotion_view; });

    if (it != emotions_anim.end()) {
        return &it->dual_anim;
    } else {
        return &emotions_anim.front().dual_anim;
    }
}

void set_eye_size(void* obj, int32_t num) {
    int height = 200 * num / 100;
    lv_obj_set_size((lv_obj_t*)obj, 200, height);
}

uint32_t shark_animation(int count, uint16_t duration) {
    ESP_LOGI(TAG, "Shark animation");
    if (!g_dual_display_manager) return duration;

    lv_obj_t* primary_screen = lv_disp_get_scr_act(g_dual_display_manager->GetPrimaryDisplay()->getLvDisplay());
    lv_obj_t* secondary_screen = lv_disp_get_scr_act(g_dual_display_manager->GetSecondaryDisplay()->getLvDisplay());
    
    lv_obj_t *eye1_obj = lv_obj_create(primary_screen);
    lv_obj_set_size(eye1_obj, 200, 200);
    lv_obj_center(eye1_obj);
    lv_obj_set_style_bg_color(eye1_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(eye1_obj, 0, 0);
    lv_obj_set_style_radius(eye1_obj, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *eye2_obj = lv_obj_create(secondary_screen);
    lv_obj_set_size(eye2_obj, 200, 200);
    lv_obj_center(eye2_obj);
    lv_obj_set_style_bg_color(eye2_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(eye2_obj, 0, 0);
    lv_obj_set_style_radius(eye2_obj, LV_RADIUS_CIRCLE, 0);

    lv_anim_t a1, a2;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, eye1_obj);
    lv_anim_set_exec_cb(&a1, set_eye_size);
    lv_anim_set_values(&a1, 100, 20);
    lv_anim_set_time(&a1, 100);
    lv_anim_set_playback_time(&a1, 100);
    lv_anim_set_repeat_delay(&a1, 1000);
    lv_anim_set_repeat_count(&a1, LV_ANIM_REPEAT_INFINITE);

    lv_anim_init(&a2);
    lv_anim_set_var(&a2, eye2_obj);
    lv_anim_set_exec_cb(&a2, set_eye_size);
    lv_anim_set_values(&a2, 100, 20);
    lv_anim_set_time(&a2, 100);
    lv_anim_set_playback_time(&a2, 100);
    lv_anim_set_repeat_delay(&a2, 1000);
    lv_anim_set_repeat_count(&a2, LV_ANIM_REPEAT_INFINITE);
    
    lv_anim_start(&a1);
    lv_anim_start(&a2);

    return duration;
}