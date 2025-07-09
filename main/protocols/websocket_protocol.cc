#include "websocket_protocol.h"
#include "board.h"
#include "system_info.h"
#include "application.h"

#include <cstring>
#include <cJSON.h>
#include <esp_log.h>
#include <arpa/inet.h>
#include "assets/lang_config.h"

#define TAG "WS"

// 构造函数，创建一个事件组
WebsocketProtocol::WebsocketProtocol() {
    // 创建一个事件组，用于处理websocket协议的事件
    event_group_handle_ = xEventGroupCreate();
}

// 析构函数，释放资源
WebsocketProtocol::~WebsocketProtocol() {
    // 如果websocket_不为空，则释放资源
    if (websocket_ != nullptr) {
        delete websocket_;
    }
    // 删除事件组
    vEventGroupDelete(event_group_handle_);
}

void WebsocketProtocol::Start() {
}

void WebsocketProtocol::SendAudio(const std::vector<uint8_t>& data) {
    if (websocket_ == nullptr) {
        return;
    }

    busy_sending_audio_ = true;
    websocket_->Send(data.data(), data.size(), true);
    busy_sending_audio_ = false;
}

bool WebsocketProtocol::SendText(const std::string& text) {
    if (websocket_ == nullptr) {
        return false;
    }

    if (!websocket_->Send(text)) {
        ESP_LOGE(TAG, "Failed to send text: %s", text.c_str());
        SetError(Lang::Strings::SERVER_ERROR);
        return false;
    }

    return true;
}

bool WebsocketProtocol::IsAudioChannelOpened() const {
    return websocket_ != nullptr && websocket_->IsConnected() && !error_occurred_ && !IsTimeout();
}

void WebsocketProtocol::CloseAudioChannel() {
    if (websocket_ != nullptr) {
        delete websocket_;
        websocket_ = nullptr;
    }
}

bool WebsocketProtocol::OpenAudioChannel() {
    // 如果websocket_不为空，则删除
    if (websocket_ != nullptr) {
        delete websocket_;
    }

    // 设置发送音频状态为false
    busy_sending_audio_ = false;
    // 设置错误状态为false
    error_occurred_ = false;
    // 获取websocket的url
    std::string url = CONFIG_WEBSOCKET_URL;
    // 获取websocket的token
    std::string token = "Bearer " + std::string(CONFIG_WEBSOCKET_ACCESS_TOKEN);
    // 创建websocket
    websocket_ = Board::GetInstance().CreateWebSocket();
    // 设置websocket的header
    websocket_->SetHeader("Authorization", token.c_str());
    websocket_->SetHeader("Protocol-Version", "1");
    websocket_->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    websocket_->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());

    // 设置websocket的回调函数
    websocket_->OnData([this](const char* data, size_t len, bool binary) {
        // 如果是二进制数据
        if (binary) {
            // 如果有回调函数，则调用回调函数
            if (on_incoming_audio_ != nullptr) {
                on_incoming_audio_(std::vector<uint8_t>((uint8_t*)data, (uint8_t*)data + len));
            }
        } else {
            // Parse JSON data
            auto root = cJSON_Parse(data);
            auto type = cJSON_GetObjectItem(root, "type");
            if (type != NULL) {
                if (strcmp(type->valuestring, "hello") == 0) {
                    ParseServerHello(root);
                } else {
                    if (on_incoming_json_ != nullptr) {
                        on_incoming_json_(root);
                    }
                }
            } else {
                ESP_LOGE(TAG, "Missing message type, data: %s", data);
            }
            cJSON_Delete(root);
        }
        last_incoming_time_ = std::chrono::steady_clock::now();
    });

    websocket_->OnDisconnected([this]() {
        ESP_LOGI(TAG, "Websocket disconnected");
        if (on_audio_channel_closed_ != nullptr) {
            on_audio_channel_closed_();
        }
    });

    if (!websocket_->Connect(url.c_str())) {
        ESP_LOGE(TAG, "Failed to connect to websocket server");
        SetError(Lang::Strings::SERVER_NOT_FOUND);
        return false;
    }

    
    // 创建一个字符串变量message，并赋值为"{"
    std::string message = "{";
    // 在message中添加"type":"hello"
    message += "\"type\":\"hello\",";
    // 在message中添加"version": 1
    message += "\"version\": 1,";
    // 在message中添加"transport":"websocket"
    message += "\"transport\":\"websocket\",";
    // 在message中添加"audio_params":{}
    message += "\"audio_params\":{";
    // 在message中添加"format":"opus", "sample_rate":16000, "channels":1, "frame_duration":OPUS_FRAME_DURATION_MS
    message += "\"format\":\"opus\", \"sample_rate\":16000, \"channels\":1, \"frame_duration\":" + std::to_string(OPUS_FRAME_DURATION_MS);
    // 在message中添加"}"和"}"
    message += "}}";
    // 如果SendText(message)返回false，则返回false
    if (!SendText(message)) {
        return false;
    }
    
    // 等待事件组中的WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT事件，超时时间为10000毫秒
    EventBits_t bits = xEventGroupWaitBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    // 如果没有收到WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT事件，则打印错误信息，设置错误码，并返回false
    if (!(bits & WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT)) {
        ESP_LOGE(TAG, "Failed to receive server hello");
        SetError(Lang::Strings::SERVER_TIMEOUT);
        return false;
    }

    if (on_audio_channel_opened_ != nullptr) {
        on_audio_channel_opened_();
    }

    return true;
}

void WebsocketProtocol::ParseServerHello(const cJSON* root) {
    // 获取transport字段
    auto transport = cJSON_GetObjectItem(root, "transport");
    // 如果transport字段不存在或者值不是websocket，则返回
    if (transport == nullptr || strcmp(transport->valuestring, "websocket") != 0) {
        ESP_LOGE(TAG, "Unsupported transport: %s", transport->valuestring);
        return;
    }

    // 获取audio_params字段
    auto audio_params = cJSON_GetObjectItem(root, "audio_params");
    // 如果audio_params字段存在
    if (audio_params != NULL) {
        // 获取sample_rate字段
        auto sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        // 如果sample_rate字段存在，则将值赋给server_sample_rate_
        if (sample_rate != NULL) {
            server_sample_rate_ = sample_rate->valueint;
        }
        // 获取frame_duration字段
        auto frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
        // 如果frame_duration字段存在，则将值赋给server_frame_duration_
        if (frame_duration != NULL) {
            server_frame_duration_ = frame_duration->valueint;
        }
    }

    // 设置WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT事件
    xEventGroupSetBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT);
}


// bool WebsocketProtocol::SendText(const std::string& text) {
//     // 具体的WebSocket发送逻辑
//     return websocket_->Send(text);
// }