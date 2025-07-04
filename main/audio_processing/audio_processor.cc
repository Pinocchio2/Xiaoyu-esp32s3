#include "audio_processor.h"
#include <esp_log.h>

#define PROCESSOR_RUNNING 0x01

static const char* TAG = "AudioProcessor";

// 构造函数
AudioProcessor::AudioProcessor()
    : afe_data_(nullptr) { // 初始化afe_data_为nullptr
    event_group_ = xEventGroupCreate(); // 创建一个事件组
}

void AudioProcessor::Initialize(AudioCodec* codec, bool realtime_chat) {
    // 初始化音频处理器
    codec_ = codec;
    // 获取输入参考通道数
    int ref_num = codec_->input_reference() ? 1 : 0;

    // 构建输入格式字符串
    std::string input_format;
    for (int i = 0; i < codec_->input_channels() - ref_num; i++) {
        input_format.push_back('M');
    }
    for (int i = 0; i < ref_num; i++) {
        input_format.push_back('R');
    }

    // 初始化语音识别模型
    srmodel_list_t *models = esp_srmodel_init("model");
    char* ns_model_name = esp_srmodel_filter(models, ESP_NSNET_PREFIX, NULL);

    // 初始化AFE配置
    afe_config_t* afe_config = afe_config_init(input_format.c_str(), NULL, AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
    if (realtime_chat) {
        // 如果是实时聊天，则启用AEC
        afe_config->aec_init = true;
        afe_config->aec_mode = AEC_MODE_VOIP_HIGH_PERF;
    } else {
        // 否则不启用AEC
        afe_config->aec_init = false;
    }
    // 启用噪音抑制
    afe_config->ns_init = true;
    afe_config->ns_model_name = ns_model_name;
    afe_config->afe_ns_mode = AFE_NS_MODE_NET;
    if (realtime_chat) {
        // 如果是实时聊天，则不启用VAD
        afe_config->vad_init = false;
    } else {
        // 否则启用VAD
        afe_config->vad_init = true;
        afe_config->vad_mode = VAD_MODE_0;
        afe_config->vad_min_noise_ms = 100;
    }
    // 设置AFE优先级和核心
    afe_config->afe_perferred_core = 1;
    afe_config->afe_perferred_priority = 1;
    // 不启用AGC
    afe_config->agc_init = false;
    // 设置内存分配模式
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

    // 根据配置创建AFE接口和数据
    afe_iface_ = esp_afe_handle_from_config(afe_config);
    afe_data_ = afe_iface_->create_from_config(afe_config);
    
    // 创建音频处理任务
    xTaskCreate([](void* arg) {
        auto this_ = (AudioProcessor*)arg;
        this_->AudioProcessorTask();
        vTaskDelete(NULL);
    }, "audio_communication", 4096, this, 3, NULL);
}

AudioProcessor::~AudioProcessor() {
    // 如果afe_data_不为空，则调用afe_iface_的destroy函数销毁afe_data_
    if (afe_data_ != nullptr) {
        afe_iface_->destroy(afe_data_);
    }
    // 调用vEventGroupDelete函数销毁event_group_
    vEventGroupDelete(event_group_);
}

// 获取音频处理器的输入数据大小
size_t AudioProcessor::GetFeedSize() {
    // 如果afe_data_为空，则返回0
    if (afe_data_ == nullptr) {
        return 0;
    }
    // 返回afe_iface_的get_feed_chunksize函数与codec_的input_channels函数的乘积
    return afe_iface_->get_feed_chunksize(afe_data_) * codec_->input_channels();
}

// 向音频处理器中输入数据
void AudioProcessor::Feed(const std::vector<int16_t>& data) {
    // 如果AFE数据为空，则返回
    if (afe_data_ == nullptr) {
        return;
    }
    // 调用AFE接口的feed函数，将AFE数据和输入数据传入
    afe_iface_->feed(afe_data_, data.data());
}

// 开始音频处理
void AudioProcessor::Start() {
    // 设置事件组中的位，表示处理器正在运行
    xEventGroupSetBits(event_group_, PROCESSOR_RUNNING);
}

// 停止音频处理
void AudioProcessor::Stop() {
    // 清除事件组中的PROCESSOR_RUNNING位
    xEventGroupClearBits(event_group_, PROCESSOR_RUNNING);
    // 如果AFE数据不为空，则重置AFE缓冲区
    if (afe_data_ != nullptr) {
        afe_iface_->reset_buffer(afe_data_);
    }
}

// 判断音频处理器是否正在运行
bool AudioProcessor::IsRunning() {
    // 获取事件组的位
    return xEventGroupGetBits(event_group_) & PROCESSOR_RUNNING;
}

// 设置输出回调函数
void AudioProcessor::OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) {
    // 将传入的回调函数赋值给output_callback_成员变量
    output_callback_ = callback;
}

// 设置语音活动检测状态改变时的回调函数
void AudioProcessor::OnVadStateChange(std::function<void(bool speaking)> callback) {
    // 将回调函数赋值给vad_state_change_callback_成员变量
    vad_state_change_callback_ = callback;
}

void AudioProcessor::AudioProcessorTask() {
    // 获取AFE接口的fetch和feed块大小
    auto fetch_size = afe_iface_->get_fetch_chunksize(afe_data_);
    auto feed_size = afe_iface_->get_feed_chunksize(afe_data_);
    // 打印AFE接口的fetch和feed块大小
    ESP_LOGI(TAG, "Audio communication task started, feed size: %d fetch size: %d",
        feed_size, fetch_size);

    while (true) {
        // 等待PROCESSOR_RUNNING事件组
        xEventGroupWaitBits(event_group_, PROCESSOR_RUNNING, pdFALSE, pdTRUE, portMAX_DELAY);

        // 从AFE接口获取数据
        auto res = afe_iface_->fetch_with_delay(afe_data_, portMAX_DELAY);
        // 如果PROCESSOR_RUNNING事件组不再存在，则继续循环
        if ((xEventGroupGetBits(event_group_) & PROCESSOR_RUNNING) == 0) {
            continue;
        }
        // 如果获取数据失败，则打印错误码并继续循环
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            if (res != nullptr) {
                ESP_LOGI(TAG, "Error code: %d", res->ret_value);
            }
            continue;
        }

        // VAD state change
        if (vad_state_change_callback_) {
            if (res->vad_state == VAD_SPEECH && !is_speaking_) {
                is_speaking_ = true;
                vad_state_change_callback_(true);
            } else if (res->vad_state == VAD_SILENCE && is_speaking_) {
                is_speaking_ = false;
                vad_state_change_callback_(false);
            }
        }

        if (output_callback_) {
            output_callback_(std::vector<int16_t>(res->data, res->data + res->data_size / sizeof(int16_t)));
        }
    }
}
