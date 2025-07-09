#ifndef BUTTON_H_
#define BUTTON_H_

#include <driver/gpio.h>
#include <iot_button.h>
#include <functional>

class Button {
public:
#if CONFIG_SOC_ADC_SUPPORTED
    // 构造函数，用于创建一个ADC按钮
    Button(const button_adc_config_t& cfg);
#endif
    // 构造函数，用于创建一个GPIO按钮
    Button(gpio_num_t gpio_num, bool active_high = false);
    // 析构函数，用于销毁按钮
    ~Button();

    // 设置按下回调函数
    void OnPressDown(std::function<void()> callback);
    // 设置松开回调函数
    void OnPressUp(std::function<void()> callback);
    // 设置长按回调函数
    void OnLongPress(std::function<void()> callback);
    // 设置单击回调函数
    void OnClick(std::function<void()> callback);
    // 设置双击回调函数
    void OnDoubleClick(std::function<void()> callback);
private:
    // GPIO引脚号
    gpio_num_t gpio_num_;
    // 按钮句柄
    button_handle_t button_handle_ = nullptr;


    // 按下回调函数
    std::function<void()> on_press_down_;
    // 松开回调函数
    std::function<void()> on_press_up_;
    // 长按回调函数
    std::function<void()> on_long_press_;
    // 单击回调函数
    std::function<void()> on_click_;
    // 双击回调函数
    std::function<void()> on_double_click_;
};

#endif // BUTTON_H_
