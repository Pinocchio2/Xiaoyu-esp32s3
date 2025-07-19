#include "dual_animation.h"
#include <cstring>
#include <algorithm>
#include "freertos/FreeRTOS.h"

#define TAG "DualAnimation"



DualAnimation::DualAnimation()
{
    xTaskCreate(animation_task, "anim_task", 4096, this, 5, NULL);
}

void DualAnimation::animation_task(void *param) {
    DualAnimation* self = static_cast<DualAnimation*>(param);
    uint32_t delay_time = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t xFrequency = pdMS_TO_TICKS(20);

    self->anim_queue_.clear();

    while (1) {
        if (!self->anim_queue_.empty()) {
            dual_anim_t anim = self->anim_queue_.front();
            self->anim_queue_.pop_front();
            delay_time = 0;
            ESP_LOGI(TAG, "Running animation: count=%d, duration=%d", anim.count, anim.duration);
            if (anim.callback) {
                delay_time = anim.callback(anim.count, anim.duration);
            }
            xFrequency = pdMS_TO_TICKS(delay_time);
            ESP_LOGI(TAG, "Animation completed, delay: %ld ms", delay_time);
            xLastWakeTime = xTaskGetTickCount();
            vTaskDelayUntil(&xLastWakeTime, xFrequency);
        }else{
            vTaskDelay(pdMS_TO_TICKS(20)); // 如果队列为空，等待20ms
        }
    }
    vTaskDelete(NULL);
}

void DualAnimation::ShowAnimation(const char* emotion)
{
    // 获取对应的动画
    const dual_anim_t* anim = get_anim_by_name(emotion);

    anim_queue_.push_back(*anim);

}


void DualAnimation::ShowAnimationNow(const char* emotion)
{
    // 清空动画队列
    anim_queue_.clear();

    // 获取对应的动画
    const dual_anim_t* anim = get_anim_by_name(emotion);

    anim_queue_.push_back(*anim);

}

const dual_anim_t* DualAnimation::get_anim_by_name(const char* name) {
    std::string_view emotion_view(name);
    auto it = std::find_if(emotions_anim.begin(), emotions_anim.end(),
        [&emotion_view](const emotions_anim_t& e) { return e.text == emotion_view; });

    if (it != emotions_anim.end()) {
        return &it->dual_anim;
    } else{
        return &emotions_anim.front().dual_anim;
    }
}


void set_eye_size(void  * obj, int32_t num)
{
    int height=200*num/100;
    //int width = 190 + 10*num/100;
    lv_obj_set_size((lv_obj_t*)obj,200,height);
}


LV_IMAGE_DECLARE(screen_animimg_1laugh_0);
LV_IMAGE_DECLARE(screen_animimg_1laugh_1);
LV_IMAGE_DECLARE(screen_animimg_1laugh_2);
LV_IMAGE_DECLARE(screen_animimg_1laugh_3);
LV_IMAGE_DECLARE(screen_animimg_1laugh_4);
LV_IMAGE_DECLARE(screen_animimg_1laugh_5);
LV_IMAGE_DECLARE(screen_animimg_1laugh_6);

const lv_image_dsc_t * screen_animimg_1_imgs[7] = {
    &screen_animimg_1laugh_0,
    &screen_animimg_1laugh_1,
    &screen_animimg_1laugh_2,
    &screen_animimg_1laugh_3,
    &screen_animimg_1laugh_4,
    &screen_animimg_1laugh_5,
    &screen_animimg_1laugh_6,
};

uint32_t smile_animation(int count, uint16_t duration) {
    // 实现微笑动画的逻辑
    ESP_LOGI(TAG, "Smile animation executed with count=%d and duration=%d", count, duration);

    lv_obj_t*  a1 = lv_animimg_create(lv_scr_act());
    lv_obj_set_pos(a1, 0, 0);
    lv_obj_set_size(a1, 240, 240);
    lv_animimg_set_src(a1, (const void **) screen_animimg_1_imgs, 7);
    lv_animimg_set_duration(a1, 80*7);
    lv_animimg_set_repeat_count(a1, 0);
    //lv_animimg_start(a);


    lv_obj_t*  a2 = lv_animimg_create(lv_scr_act());
    lv_obj_set_pos(a2, 0, 0);
    lv_obj_set_size(a2, 240, 240);
    lv_animimg_set_src(a2, (const void **) screen_animimg_1_imgs, 7);
    lv_animimg_set_duration(a1, 80*7);
    lv_animimg_set_repeat_count(a2, 0);


    lv_anim_timeline_t* anim_timeline = lv_anim_timeline_create();
    lv_anim_timeline_add(anim_timeline, 0, (lv_anim_t*)a1);
    lv_anim_timeline_add(anim_timeline, 0, (lv_anim_t*)a2);

    lv_anim_timeline_start(anim_timeline);



    return 560; // 返回动画执行的延迟时间
}


uint32_t shark_animation(int count, uint16_t duration) {
    // 实现眨眼动画的逻辑
    ESP_LOGI(TAG, "Shark animation");
    lv_obj_t *eye1_obj = lv_obj_create(lv_scr_act());//屏幕1
    lv_obj_set_size(eye1_obj, 200, 200);
    lv_obj_center(eye1_obj);

    lv_obj_t *eye2_obj = lv_obj_create(lv_scr_act());//屏幕2
    lv_obj_set_size(eye2_obj, 200, 200);
    lv_obj_center(eye2_obj);

    // 2. 设置初始样式（睁开状态）
    lv_obj_set_style_bg_color(eye1_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(eye1_obj, 0, 0);
    lv_obj_set_style_radius(eye1_obj, LV_RADIUS_CIRCLE, 0);

    lv_obj_set_style_bg_color(eye2_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(eye2_obj, 0, 0);
    lv_obj_set_style_radius(eye2_obj, LV_RADIUS_CIRCLE, 0);

    // 3. 创建动画时间线
    lv_anim_t a1;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, eye1_obj);
    lv_anim_set_exec_cb(&a1, set_eye_size);
    lv_anim_set_values(&a1, 100, 20); // 从圆形变为椭圆
    lv_anim_set_path_cb(&a1, lv_anim_path_linear);
    lv_anim_set_time(&a1, 100);
    //lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&a1, 0);  //
    lv_anim_set_playback_time(&a1, 100);
    lv_anim_set_playback_delay(&a1, 100);

    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, eye1_obj);
    lv_anim_set_exec_cb(&a2, set_eye_size);
    lv_anim_set_values(&a2, 100, 20); // 从圆形变为椭圆
    lv_anim_set_path_cb(&a2, lv_anim_path_linear);
    lv_anim_set_time(&a2, 100);
    //lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&a2, 0);  // 每隔1秒眨眼一次
    lv_anim_set_playback_time(&a2, 100);
    lv_anim_set_playback_delay(&a2, 100);

    lv_anim_timeline_t* anim_timeline = lv_anim_timeline_create();
    lv_anim_timeline_add(anim_timeline, 0, &a1);
    lv_anim_timeline_add(anim_timeline, 0, &a2);

    lv_anim_timeline_start(anim_timeline);

    //lv_anim_start(&a1);

    return 300; // 返回动画执行的延迟时间
}

