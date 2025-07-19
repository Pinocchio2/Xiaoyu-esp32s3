#ifndef DUAL_EYE_DISPLAY_H_
#define DUAL_EYE_DISPLAY_H_

#include "dual_animation.h"

/**
 * @brief 简化的双眼显示类
 * 直接封装 DualAnimation，提供简单的接口
 */
class DualEyeDisplay {
private:
    DualAnimation* animation_system_;  // 动画系统实例
    
public:
    DualEyeDisplay();
    ~DualEyeDisplay();
    
    // 简化的动画接口
    void SetEmotion(const char* emotion);           // 设置表情
    void PlayAnimation(const char* name);           // 播放动画
    void PlayAnimationNow(const char* name);        // 立即播放动画
};

#endif // DUAL_EYE_DISPLAY_H_