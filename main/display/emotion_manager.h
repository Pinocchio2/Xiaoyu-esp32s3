#ifndef EMOTION_MANAGER_H
#define EMOTION_MANAGER_H

#include "emotion_animation.h"
#include "ui/eye.h"
#include <map>
#include <memory>
#include <esp_log.h>

/**
 * @brief 表情动画管理器
 * 单例模式，负责管理所有预定义的表情动画
 */
class EmotionManager {
public:
    // 获取单例实例
    static EmotionManager& GetInstance() {
        static EmotionManager instance;
        return instance;
    }
    
    /**
     * @brief 根据表情名称获取对应的动画
     * @param emotion_name 表情名称（如 "happy", "sad" 等）
     * @return 对应的动画对象，如果找不到则返回默认中性表情
     */
    const Animation& GetAnimation(const std::string& emotion_name);
    
    /**
     * @brief 预加载所有动画
     * 在系统启动时调用，初始化所有预定义的表情动画
     */
    void PreloadAllAnimations();
    
    /**
     * @brief 注册一个新的表情动画
     * @param emotion_name 表情名称
     * @param animation 动画对象
     */
    void RegisterAnimation(const std::string& emotion_name, const Animation& animation);
    
    /**
     * @brief 检查是否存在指定的表情动画
     * @param emotion_name 表情名称
     * @return true 如果存在，false 如果不存在
     */
    bool HasAnimation(const std::string& emotion_name) const;
    
    /**
     * @brief 获取默认的中性表情动画
     * @return 默认动画对象
     */
    const Animation& GetDefaultAnimation() const;

private:
    // 私有构造函数（单例模式）
    EmotionManager();
    
    // 禁用拷贝构造和赋值操作
    EmotionManager(const EmotionManager&) = delete;
    EmotionManager& operator=(const EmotionManager&) = delete;
    
    // 初始化所有预定义的表情动画
    void InitializeAnimations();
    
    // 创建静态表情动画的辅助方法
    Animation CreateStaticEmotion(const std::string& name, 
                                const lv_img_dsc_t* left_eye, 
                                const lv_img_dsc_t* right_eye);
    
    // 创建动态表情动画的辅助方法
    Animation CreateDynamicEmotion(const std::string& name, 
                                 const std::vector<AnimationFrame>& frames, 
                                 bool loop = false);
    
    // 创建眨眼动画的辅助方法
    Animation CreateBlinkingAnimation();
    
    // 创建眨眼循环动画的辅助方法
    Animation CreateWinkingAnimation();
    
    /**
     * @brief 创建yanzhu动画（眼珠转动动画）
     * @return yanzhu动画对象
     */
    Animation CreateYanzhuAnimation();
    
    std::map<std::string, Animation> animations_;  // 存储所有表情动画的映射表
    Animation default_animation_;                   // 默认的中性表情动画
    
    static const char* TAG;  // 日志标签
};

#endif // EMOTION_MANAGER_H