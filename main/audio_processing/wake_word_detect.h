#ifndef WAKE_WORD_DETECT_H
#define WAKE_WORD_DETECT_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_afe_sr_models.h>
#include <esp_nsn_models.h>

#include <list>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "audio_codec.h"

/**
 * @class WakeWordDetect
 * @brief 用于检测音频中的唤醒词的类。
 *
 * 该类提供了初始化、音频数据输入、唤醒词检测、编码和获取检测结果的接口。
 * 用户可以通过设置回调函数来处理检测到的唤醒词。
 *
 * @example
 * WakeWordDetect detector;
 * detector.Initialize(codec);
 * detector.OnWakeWordDetected([](const std::string& wake_word) {
 *     std::cout << "Detected wake word: " << wake_word << std::endl;
 * });
 * detector.StartDetection();
 * std::vector<int16_t> audio_data = ...; // 获取音频数据
 * detector.Feed(audio_data);
 * detector.StopDetection();
 *
 * @param codec 音频编解码器指针，用于初始化。
 */
class WakeWordDetect {
public:
    WakeWordDetect(); // 构造函数
    ~WakeWordDetect(); // 析构函数

    void Initialize(AudioCodec* codec); // 初始化函数
    void Feed(const std::vector<int16_t>& data); // 输入音频数据
    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback); // 设置唤醒词检测回调函数
    void StartDetection(); // 开始唤醒词检测
    void StopDetection(); // 停止唤醒词检测
    bool IsDetectionRunning(); // 判断唤醒词检测是否正在进行
    size_t GetFeedSize(); // 获取输入音频数据的大小
    void EncodeWakeWordData(); // 编码唤醒词数据
    bool GetWakeWordOpus(std::vector<uint8_t>& opus); // 获取唤醒词的Opus编码数据
    const std::string& GetLastDetectedWakeWord() const { return last_detected_wake_word_; } // 获取最后检测到的唤醒词

private:
    esp_afe_sr_iface_t* afe_iface_ = nullptr; // AFE接口
    esp_afe_sr_data_t* afe_data_ = nullptr; // AFE数据
    char* wakenet_model_ = NULL; // 唤醒词模型
    std::vector<std::string> wake_words_; // 唤醒词列表
    EventGroupHandle_t event_group_; // 事件组句柄
    std::function<void(const std::string& wake_word)> wake_word_detected_callback_; // 唤醒词检测回调函数
    AudioCodec* codec_ = nullptr; // 音频编解码器
    std::string last_detected_wake_word_; // 最后检测到的唤醒词

    TaskHandle_t wake_word_encode_task_ = nullptr; // 唤醒词编码任务句柄
    StaticTask_t wake_word_encode_task_buffer_; // 唤醒词编码任务缓冲区
    StackType_t* wake_word_encode_task_stack_ = nullptr; // 唤醒词编码任务堆栈
    std::list<std::vector<int16_t>> wake_word_pcm_; // 唤醒词PCM数据列表
    std::list<std::vector<uint8_t>> wake_word_opus_; // 唤醒词Opus编码数据列表
    std::mutex wake_word_mutex_; // 唤醒词互斥锁
    std::condition_variable wake_word_cv_; // 唤醒词条件变量

    void StoreWakeWordData(uint16_t* data, size_t size); // 存储唤醒词数据
    void AudioDetectionTask(); // 音频检测任务
};

#endif
