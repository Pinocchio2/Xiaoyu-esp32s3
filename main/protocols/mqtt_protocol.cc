#include "mqtt_protocol.h"
#include "board.h"
#include "application.h"
#include "settings.h"

#include <esp_log.h>
#include <ml307_mqtt.h>
#include <ml307_udp.h>
#include <cstring>
#include <arpa/inet.h>
#include "assets/lang_config.h"

#define TAG "MQTT"

// 构造函数，创建一个事件组
MqttProtocol::MqttProtocol() {
    // 创建一个事件组
    event_group_handle_ = xEventGroupCreate();
}

MqttProtocol::~MqttProtocol() {
    // 打印日志信息
    ESP_LOGI(TAG, "MqttProtocol deinit");
    // 如果udp_不为空，则删除
    if (udp_ != nullptr) {
        delete udp_;
    }
    // 如果mqtt_不为空，则删除
    if (mqtt_ != nullptr) {
        delete mqtt_;
    }
    // 删除事件组
    vEventGroupDelete(event_group_handle_);
}

void MqttProtocol::Start() {
    StartMqttClient(false);
}

bool MqttProtocol::StartMqttClient(bool report_error) {
    // 如果mqtt_不为空，则删除
    if (mqtt_ != nullptr) {
        ESP_LOGW(TAG, "Mqtt client already started");
        delete mqtt_;
    }

    // 从设置中获取mqtt的相关信息
    Settings settings("mqtt", false);
    endpoint_ = settings.GetString("endpoint");
    client_id_ = settings.GetString("client_id");
    username_ = settings.GetString("username");
    password_ = settings.GetString("password");
    publish_topic_ = settings.GetString("publish_topic");

    // 如果endpoint_为空，则返回false
    if (endpoint_.empty()) {
        ESP_LOGW(TAG, "MQTT endpoint is not specified");
        if (report_error) {
            SetError(Lang::Strings::SERVER_NOT_FOUND);
        }
        return false;
    }

    // 创建mqtt对象
    mqtt_ = Board::GetInstance().CreateMqtt();
    mqtt_->SetKeepAlive(90);

    // 设置mqtt断开连接时的回调函数
    mqtt_->OnDisconnected([this]() {
        ESP_LOGI(TAG, "Disconnected from endpoint");
    });

    // 设置mqtt接收到消息时的回调函数
    mqtt_->OnMessage([this](const std::string& topic, const std::string& payload) {
        cJSON* root = cJSON_Parse(payload.c_str());
        if (root == nullptr) {
            ESP_LOGE(TAG, "Failed to parse json message %s", payload.c_str());
            return;
        }
        cJSON* type = cJSON_GetObjectItem(root, "type");
        if (type == nullptr) {
            ESP_LOGE(TAG, "Message type is not specified");
            cJSON_Delete(root);
            return;
        }

        // 如果消息类型为hello，则解析服务器hello消息
        if (strcmp(type->valuestring, "hello") == 0) {
            ParseServerHello(root);
        // 如果消息类型为goodbye，则处理服务器goodbye消息
        } else if (strcmp(type->valuestring, "goodbye") == 0) {
            auto session_id = cJSON_GetObjectItem(root, "session_id");
            ESP_LOGI(TAG, "Received goodbye message, session_id: %s", session_id ? session_id->valuestring : "null");
            // 如果session_id为空或者session_id_等于session_id，则关闭音频通道
            if (session_id == nullptr || session_id_ == session_id->valuestring) {
                Application::GetInstance().Schedule([this]() {
                    CloseAudioChannel();
                });
            }
        // 如果有自定义的回调函数，则调用回调函数
        } else if (on_incoming_json_ != nullptr) {
            on_incoming_json_(root);
        }
        cJSON_Delete(root);
        last_incoming_time_ = std::chrono::steady_clock::now();
    });

    // 连接到endpoint_
    // 打印连接到端点的信息
    ESP_LOGI(TAG, "Connecting to endpoint %s", endpoint_.c_str());
    // 如果连接失败
    if (!mqtt_->Connect(endpoint_, 8883, client_id_, username_, password_)) {
        // 打印连接失败的信息
        ESP_LOGE(TAG, "Failed to connect to endpoint");
        // 设置错误信息
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        // 返回false
        return false;
    }

    ESP_LOGI(TAG, "Connected to endpoint");
    return true;
}

// 发送文本消息
bool MqttProtocol::SendText(const std::string& text) {
    // 如果发布主题为空，则返回false
    if (publish_topic_.empty()) {
        return false;
    }
    // 如果发布消息失败，则记录错误日志，并设置错误信息，返回false
    if (!mqtt_->Publish(publish_topic_, text)) {
        ESP_LOGE(TAG, "Failed to publish message: %s", text.c_str());
        SetError(Lang::Strings::SERVER_ERROR);
        return false;
    }
    // 发布消息成功，返回true
    return true;
}

void MqttProtocol::SendAudio(const std::vector<uint8_t>& data) {
    // 加锁，防止多线程同时操作
    std::lock_guard<std::mutex> lock(channel_mutex_);
    // 如果udp_为空，则直接返回
    if (udp_ == nullptr) {
        return;
    }

    // 生成随机数
    std::string nonce(aes_nonce_);
    // 将数据大小写入随机数
    *(uint16_t*)&nonce[2] = htons(data.size());
    // 将本地序列号写入随机数
    *(uint32_t*)&nonce[12] = htonl(++local_sequence_);

    // 生成加密后的数据
    std::string encrypted;
    encrypted.resize(aes_nonce_.size() + data.size());
    // 将随机数复制到加密数据中
    memcpy(encrypted.data(), nonce.data(), nonce.size());

    // 初始化计数器
    size_t nc_off = 0;
    // 初始化流块
    uint8_t stream_block[16] = {0};
    // 使用CTR模式加密数据
    if (mbedtls_aes_crypt_ctr(&aes_ctx_, data.size(), &nc_off, (uint8_t*)nonce.c_str(), stream_block,
        (uint8_t*)data.data(), (uint8_t*)&encrypted[nonce.size()]) != 0) {
        // 如果加密失败，则输出错误信息
        ESP_LOGE(TAG, "Failed to encrypt audio data");
        return;
    }

    // 设置正在发送音频标志
    busy_sending_audio_ = true;
    // 发送加密后的数据
    udp_->Send(encrypted);
    // 清除正在发送音频标志
    busy_sending_audio_ = false;
}

void MqttProtocol::CloseAudioChannel() {
    // 加锁，防止多线程同时操作udp_指针
    {
        std::lock_guard<std::mutex> lock(channel_mutex_);
        // 如果udp_指针不为空，则释放内存
        if (udp_ != nullptr) {
            delete udp_;
            udp_ = nullptr;
        }
    }

    // 构造消息内容
    std::string message = "{";
    message += "\"session_id\":\"" + session_id_ + "\",";
    message += "\"type\":\"goodbye\"";
    message += "}";
    // 发送消息
    SendText(message);

    // 如果on_audio_channel_closed_不为空，则调用回调函数
    if (on_audio_channel_closed_ != nullptr) {
        on_audio_channel_closed_();
    }
}

bool MqttProtocol::OpenAudioChannel() {
    if (mqtt_ == nullptr || !mqtt_->IsConnected()) {
        ESP_LOGI(TAG, "MQTT is not connected, try to connect now");
        if (!StartMqttClient(true)) {
            return false;
        }
    }

    busy_sending_audio_ = false;
    error_occurred_ = false;
    session_id_ = "";
    xEventGroupClearBits(event_group_handle_, MQTT_PROTOCOL_SERVER_HELLO_EVENT);

    // 发送 hello 消息申请 UDP 通道
    std::string message = "{";
    message += "\"type\":\"hello\",";
    message += "\"version\": 3,";
    message += "\"transport\":\"udp\",";
    message += "\"audio_params\":{";
    message += "\"format\":\"opus\", \"sample_rate\":16000, \"channels\":1, \"frame_duration\":" + std::to_string(OPUS_FRAME_DURATION_MS);
    message += "}}";
    if (!SendText(message)) {
        return false;
    }

    // 等待服务器响应
    EventBits_t bits = xEventGroupWaitBits(event_group_handle_, MQTT_PROTOCOL_SERVER_HELLO_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & MQTT_PROTOCOL_SERVER_HELLO_EVENT)) {
        ESP_LOGE(TAG, "Failed to receive server hello");
        SetError(Lang::Strings::SERVER_TIMEOUT);
        return false;
    }

    std::lock_guard<std::mutex> lock(channel_mutex_);
    if (udp_ != nullptr) {
        delete udp_;
    }
    udp_ = Board::GetInstance().CreateUdp();
    udp_->OnMessage([this](const std::string& data) {
        if (data.size() < sizeof(aes_nonce_)) {
            ESP_LOGE(TAG, "Invalid audio packet size: %zu", data.size());
            return;
        }
        if (data[0] != 0x01) {
            ESP_LOGE(TAG, "Invalid audio packet type: %x", data[0]);
            return;
        }
        uint32_t sequence = ntohl(*(uint32_t*)&data[12]);
        if (sequence < remote_sequence_) {
            ESP_LOGW(TAG, "Received audio packet with old sequence: %lu, expected: %lu", sequence, remote_sequence_);
            return;
        }
        if (sequence != remote_sequence_ + 1) {
            ESP_LOGW(TAG, "Received audio packet with wrong sequence: %lu, expected: %lu", sequence, remote_sequence_ + 1);
        }

        std::vector<uint8_t> decrypted;
        size_t decrypted_size = data.size() - aes_nonce_.size();
        size_t nc_off = 0;
        uint8_t stream_block[16] = {0};
        decrypted.resize(decrypted_size);
        auto nonce = (uint8_t*)data.data();
        auto encrypted = (uint8_t*)data.data() + aes_nonce_.size();
        int ret = mbedtls_aes_crypt_ctr(&aes_ctx_, decrypted_size, &nc_off, nonce, stream_block, encrypted, (uint8_t*)decrypted.data());
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to decrypt audio data, ret: %d", ret);
            return;
        }
        if (on_incoming_audio_ != nullptr) {
            on_incoming_audio_(std::move(decrypted));
        }
        remote_sequence_ = sequence;
        last_incoming_time_ = std::chrono::steady_clock::now();
    });

    udp_->Connect(udp_server_, udp_port_);

    if (on_audio_channel_opened_ != nullptr) {
        on_audio_channel_opened_();
    }
    return true;
}

void MqttProtocol::ParseServerHello(const cJSON* root) {
    auto transport = cJSON_GetObjectItem(root, "transport");
    if (transport == nullptr || strcmp(transport->valuestring, "udp") != 0) {
        ESP_LOGE(TAG, "Unsupported transport: %s", transport->valuestring);
        return;
    }

    auto session_id = cJSON_GetObjectItem(root, "session_id");
    if (session_id != nullptr) {
        session_id_ = session_id->valuestring;
        ESP_LOGI(TAG, "Session ID: %s", session_id_.c_str());
    }

    // Get sample rate from hello message
    auto audio_params = cJSON_GetObjectItem(root, "audio_params");
    if (audio_params != NULL) {
        auto sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        if (sample_rate != NULL) {
            server_sample_rate_ = sample_rate->valueint;
        }
        auto frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
        if (frame_duration != NULL) {
            server_frame_duration_ = frame_duration->valueint;
        }
    }

    auto udp = cJSON_GetObjectItem(root, "udp");
    if (udp == nullptr) {
        ESP_LOGE(TAG, "UDP is not specified");
        return;
    }
    udp_server_ = cJSON_GetObjectItem(udp, "server")->valuestring;
    udp_port_ = cJSON_GetObjectItem(udp, "port")->valueint;
    auto key = cJSON_GetObjectItem(udp, "key")->valuestring;
    auto nonce = cJSON_GetObjectItem(udp, "nonce")->valuestring;

    // auto encryption = cJSON_GetObjectItem(udp, "encryption")->valuestring;
    // ESP_LOGI(TAG, "UDP server: %s, port: %d, encryption: %s", udp_server_.c_str(), udp_port_, encryption);
    aes_nonce_ = DecodeHexString(nonce);
    mbedtls_aes_init(&aes_ctx_);
    mbedtls_aes_setkey_enc(&aes_ctx_, (const unsigned char*)DecodeHexString(key).c_str(), 128);
    local_sequence_ = 0;
    remote_sequence_ = 0;
    xEventGroupSetBits(event_group_handle_, MQTT_PROTOCOL_SERVER_HELLO_EVENT);
}

static const char hex_chars[] = "0123456789ABCDEF";
// 辅助函数，将单个十六进制字符转换为对应的数值
static inline uint8_t CharToHex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;  // 对于无效输入，返回0
}

std::string MqttProtocol::DecodeHexString(const std::string& hex_string) {
    std::string decoded;
    decoded.reserve(hex_string.size() / 2);
    for (size_t i = 0; i < hex_string.size(); i += 2) {
        char byte = (CharToHex(hex_string[i]) << 4) | CharToHex(hex_string[i + 1]);
        decoded.push_back(byte);
    }
    return decoded;
}

bool MqttProtocol::IsAudioChannelOpened() const {
    return udp_ != nullptr && !error_occurred_ && !IsTimeout();
}
