#include "protocol.h"

#include <esp_log.h>

#define TAG "Protocol"

// 定义一个函数，用于处理传入的JSON数据
void Protocol::OnIncomingJson(std::function<void(const cJSON* root)> callback) {
    // 将传入的回调函数赋值给成员变量on_incoming_json_
    on_incoming_json_ = callback;
}

// 定义一个函数，用于处理传入的音频数据
void Protocol::OnIncomingAudio(std::function<void(std::vector<uint8_t>&& data)> callback) {
    // 将传入的回调函数赋值给on_incoming_audio_成员变量
    on_incoming_audio_ = callback;
}

// 定义一个函数，用于设置音频通道打开的回调函数
void Protocol::OnAudioChannelOpened(std::function<void()> callback) {
    // 将传入的回调函数赋值给on_audio_channel_opened_成员变量
    on_audio_channel_opened_ = callback;
}

// 定义一个函数，用于处理音频通道关闭事件，参数为一个回调函数
void Protocol::OnAudioChannelClosed(std::function<void()> callback) {
    // 将回调函数赋值给on_audio_channel_closed_成员变量
    on_audio_channel_closed_ = callback;
}

// 定义Protocol类中的OnNetworkError函数，该函数接受一个回调函数作为参数，用于处理网络错误
void Protocol::OnNetworkError(std::function<void(const std::string& message)> callback) {
    // 将回调函数赋值给on_network_error_成员变量
    on_network_error_ = callback;
}

// 设置错误信息
void Protocol::SetError(const std::string& message) {
    // 设置错误标志为true
    error_occurred_ = true;
    // 如果有网络错误回调函数，则调用该函数并传入错误信息
    if (on_network_error_ != nullptr) {
        on_network_error_(message);
    }
}

// 发送停止说话的协议
void Protocol::SendAbortSpeaking(AbortReason reason) {
    // 构造消息字符串
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"abort\"";
    // 如果停止说话的原因是唤醒词检测到，则添加原因到消息字符串中
    if (reason == kAbortReasonWakeWordDetected) {
        message += ",\"reason\":\"wake_word_detected\"";
    }
    // 添加结束符
    message += "}";
    // 发送消息字符串
    SendText(message);
}

// 发送唤醒词检测
void Protocol::SendWakeWordDetected(const std::string& wake_word) {
    // 构造JSON字符串，包含会话ID、类型、状态和唤醒词
    std::string json = "{\"session_id\":\"" + session_id_ + 
                      "\",\"type\":\"listen\",\"state\":\"detect\",\"text\":\"" + wake_word + "\"}";
    // 发送JSON字符串
    SendText(json);
}

void Protocol::SendStartListening(ListeningMode mode) {
    // 构造开始监听的消息
    std::string message = "{\"session_id\":\"" + session_id_ + "\"";
    message += ",\"type\":\"listen\",\"state\":\"start\"";
    // 根据监听模式，添加相应的参数
    if (mode == kListeningModeRealtime) {
        message += ",\"mode\":\"realtime\"";
    } else if (mode == kListeningModeAutoStop) {
        message += ",\"mode\":\"auto\"";
    } else {
        message += ",\"mode\":\"manual\"";
    }
    message += "}";
    // 发送消息
    SendText(message);
}

// 发送停止监听消息
void Protocol::SendStopListening() {
    // 构造停止监听消息
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"listen\",\"state\":\"stop\"}";
    // 发送停止监听消息
    SendText(message);
}

void Protocol::SendIotDescriptors(const std::string& descriptors) {
    // 解析传入的IoT描述符
    cJSON* root = cJSON_Parse(descriptors.c_str());
    if (root == nullptr) {
        // 解析失败，打印错误日志
        ESP_LOGE(TAG, "Failed to parse IoT descriptors: %s", descriptors.c_str());
        return;
    }

    // 检查解析后的数据是否为数组
    if (!cJSON_IsArray(root)) {
        // 不是数组，打印错误日志
        ESP_LOGE(TAG, "IoT descriptors should be an array");
        cJSON_Delete(root);
        return;
    }

    // 获取数组的大小
    int arraySize = cJSON_GetArraySize(root);
    for (int i = 0; i < arraySize; ++i) {
        // 获取数组中的每个元素
        cJSON* descriptor = cJSON_GetArrayItem(root, i);
        if (descriptor == nullptr) {
            // 获取失败，打印错误日志
            ESP_LOGE(TAG, "Failed to get IoT descriptor at index %d", i);
            continue;
        }

        // 创建一个JSON对象，用于存储消息
        cJSON* messageRoot = cJSON_CreateObject();
        // 添加会话ID
        cJSON_AddStringToObject(messageRoot, "session_id", session_id_.c_str());
        // 添加消息类型
        cJSON_AddStringToObject(messageRoot, "type", "iot");
        // 添加更新标志
        cJSON_AddBoolToObject(messageRoot, "update", true);

        // 创建一个JSON数组，用于存储描述符
        cJSON* descriptorArray = cJSON_CreateArray();
        // 将描述符添加到数组中
        cJSON_AddItemToArray(descriptorArray, cJSON_Duplicate(descriptor, 1));
        // 将数组添加到消息对象中
        cJSON_AddItemToObject(messageRoot, "descriptors", descriptorArray);

        // 将消息对象转换为字符串
        char* message = cJSON_PrintUnformatted(messageRoot);
        if (message == nullptr) {
            // 转换失败，打印错误日志
            ESP_LOGE(TAG, "Failed to print JSON message for IoT descriptor at index %d", i);
            cJSON_Delete(messageRoot);
            continue;
        }

        // 发送文本消息
        SendText(std::string(message));
        // 释放内存
        cJSON_free(message);
        cJSON_Delete(messageRoot);
    }

    // 释放内存
    cJSON_Delete(root);
}

// 发送物联网状态
void Protocol::SendIotStates(const std::string& states) {
    // 构造消息字符串，包含session_id、type、update和states字段
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"iot\",\"update\":true,\"states\":" + states + "}";
    // 发送文本消息
    SendText(message);
}


bool Protocol::IsTimeout() const {
    // 定义超时时间为120秒
    const int kTimeoutSeconds = 120;
    // 获取当前时间
    auto now = std::chrono::steady_clock::now();
    // 计算当前时间与上次接收时间的时间差
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_incoming_time_);
    // 判断时间差是否大于超时时间
    bool timeout = duration.count() > kTimeoutSeconds;
    // 如果超时，则输出错误日志
    if (timeout) {
        ESP_LOGE(TAG, "Channel timeout %lld seconds", duration.count());
    }
    // 返回是否超时
    return timeout;
}

// 判断音频通道是否忙碌
bool Protocol::IsAudioChannelBusy() const {
    // 返回音频通道是否忙碌的标志
    return busy_sending_audio_;
}

///////////////////////////////新增///////////////////
// 发送自定义文本
bool Protocol::SendCustomText(const std::string& text) {
    // 调用SendText函数发送文本
    return SendText(text);
}

// 发送自定义消息
bool Protocol::SendCustomMessage(const std::string& type, const std::string& data) {
    // 构造消息字符串
    std::string message = "{";
    message += "\"session_id\":\"" + session_id_ + "\","; // 添加session_id
    message += "\"type\":\"" + type + "\","; // 添加消息类型
    message += "\"custom_data\":" + data; // 添加自定义数据
    message += "}";
    // 发送消息
    return SendText(message);
}