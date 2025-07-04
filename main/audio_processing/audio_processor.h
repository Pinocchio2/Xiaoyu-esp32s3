#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <esp_afe_sr_models.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <string>
#include <vector>
#include <functional>

#include "audio_codec.h"

/**
 * @class AudioProcessor
 * @brief 音频处理器类，用于处理音频数据流。
 *
 * 该类提供了初始化音频处理器、喂入音频数据、启动和停止音频处理、设置输出回调函数、
 * 设置语音活动检测（VAD）状态变化回调函数等功能。适用于实时聊天和其他音频处理场景。
 *
 * 使用示例：
 * @code
 * AudioCodec* codec = new AudioCodec();
 * AudioProcessor processor;
 * processor.Initialize(codec, true);
 * processor.OnOutput([](std::vector<int16_t>&& data) {
 *     // 处理输出数据
 * });
 * processor.OnVadStateChange([](bool speaking) {
 *     // 处理语音活动状态变化
 * });
 * processor.Start();
 * processor.Feed(audioData);
 * processor.Stop();
 * @endcode
 *
 * 构造函数参数：
 * 无
 *
 * 特殊使用限制或潜在的副作用：
 * - 在调用 `Feed` 方法之前，必须先调用 `Initialize` 方法进行初始化。
 * - 在调用 `Start` 方法之前，必须设置输出回调函数和语音活动检测状态变化回调函数。
 * - `Stop` 方法必须在 `Start` 方法之后调用，以确保资源正确释放。
 */
class AudioProcessor {
public:
    // 构造函数
    AudioProcessor();
    // 析构函数
    ~AudioProcessor();

    // 初始化音频处理器
    void Initialize(AudioCodec* codec, bool realtime_chat);
    // 输入音频数据
    void Feed(const std::vector<int16_t>& data);
    // 开始处理音频
    void Start();
    // 停止处理音频
    void Stop();
    // 是否正在处理音频
    bool IsRunning();
    // 设置输出回调函数
    void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback);
    // 设置VAD状态改变回调函数
    void OnVadStateChange(std::function<void(bool speaking)> callback);
    // 获取输入数据大小
    size_t GetFeedSize();

private:
    // 事件组句柄
    EventGroupHandle_t event_group_ = nullptr;
    // AFE接口
    esp_afe_sr_iface_t* afe_iface_ = nullptr;
    // AFE数据
    esp_afe_sr_data_t* afe_data_ = nullptr;
    // 输出回调函数
    std::function<void(std::vector<int16_t>&& data)> output_callback_;
    // VAD状态改变回调函数
    std::function<void(bool speaking)> vad_state_change_callback_;
    // 音频编解码器
    AudioCodec* codec_ = nullptr;
    // 是否正在说话
    bool is_speaking_ = false;

    // 音频处理任务
    void AudioProcessorTask();
};

#endif
