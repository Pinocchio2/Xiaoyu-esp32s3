#ifndef _ES8311_AUDIO_CODEC_H
#define _ES8311_AUDIO_CODEC_H

#include "audio_codec.h"

#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

/**
 * @brief 音频编解码器类，用于控制ES8311音频编解码器。
 *
 * 该类继承自AudioCodec，提供了对ES8311音频编解码器的控制和操作功能。
 * 主要功能包括创建双工通道、读取和写入音频数据、设置输出音量、启用或禁用输入输出等。
 *
 * 使用示例：
 *
 * 构造函数参数：
 * - `void* i2c_master_handle`：I2C主控句柄。
 * - `i2c_port_t i2c_port`：I2C端口。
 * - `int input_sample_rate`：输入采样率。
 * - `int output_sample_rate`：输出采样率。
 * - `gpio_num_t mclk`：主时钟引脚。
 * - `gpio_num_t bclk`：位时钟引脚。
 * - `gpio_num_t ws`：字选择引脚。
 * - `gpio_num_t dout`：数据输出引脚。
 * - `gpio_num_t din`：数据输入引脚。
 * - `gpio_num_t pa_pin`：功放引脚。
 * - `uint8_t es8311_addr`：ES8311设备地址。
 * - `bool use_mclk`：是否使用主时钟，默认为true。
 *
 * 特殊使用限制或潜在的副作用：
 * - 确保在创建对象前，I2C主控句柄已正确初始化。
 * - 在使用前，需要调用EnableOutput和EnableInput以启用相应的功能。
 * - 设置音量时，确保值在合理范围内（通常为0-100）。
 */
class Es8311AudioCodec : public AudioCodec {
private:
    // 音频数据接口
    const audio_codec_data_if_t* data_if_ = nullptr;
    // 音频控制接口
    const audio_codec_ctrl_if_t* ctrl_if_ = nullptr;
    // 音频接口
    const audio_codec_if_t* codec_if_ = nullptr;
    // 音频GPIO接口
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;

    // 输出设备句柄
    esp_codec_dev_handle_t output_dev_ = nullptr;
    // 输入设备句柄
    esp_codec_dev_handle_t input_dev_ = nullptr;
    // PA引脚
    gpio_num_t pa_pin_ = GPIO_NUM_NC;

    // 创建双工通道
    void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din);

    // 重写Read函数
    virtual int Read(int16_t* dest, int samples) override;
    // 重写Write函数
    virtual int Write(const int16_t* data, int samples) override;

public:
    // 构造函数
    Es8311AudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
        gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
        gpio_num_t pa_pin, uint8_t es8311_addr, bool use_mclk = true);
    // 析构函数
    virtual ~Es8311AudioCodec();

    // 设置输出音量
    virtual void SetOutputVolume(int volume) override;
    // 启用输入
    virtual void EnableInput(bool enable) override;
    // 启用输出
    virtual void EnableOutput(bool enable) override;
};

#endif // _ES8311_AUDIO_CODEC_H