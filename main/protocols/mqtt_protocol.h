#ifndef MQTT_PROTOCOL_H
#define MQTT_PROTOCOL_H


#include "protocol.h"
#include <mqtt.h>
#include <udp.h>
#include <cJSON.h>
#include <mbedtls/aes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <functional>
#include <string>
#include <map>
#include <mutex>

#define MQTT_PING_INTERVAL_SECONDS 90
#define MQTT_RECONNECT_INTERVAL_MS 10000

#define MQTT_PROTOCOL_SERVER_HELLO_EVENT (1 << 0)

/**
 * @class MqttProtocol
 * @brief 实现基于MQTT协议的音频通信协议类。
 *
 * MqttProtocol类继承自Protocol类，用于通过MQTT协议进行音频数据的发送和接收。
 * 该类提供了启动、发送音频数据、打开和关闭音频通道等功能。
 * 
 * 核心功能包括：
 * - 启动MQTT客户端（StartMqttClient）
 * - 发送音频数据（SendAudio）
 * - 打开和关闭音频通道（OpenAudioChannel, CloseAudioChannel）
 * - 检查音频通道是否打开（IsAudioChannelOpened）
 * 
 * 使用示例：
 * 
 * 构造函数参数：
 * 无参数构造函数，初始化MqttProtocol对象。
 * 
 * 特殊使用限制或潜在的副作用：
 * - 在使用前需要确保MQTT服务器可用。
 * - 在发送音频数据前需要先打开音频通道。
 * - 类内部使用了互斥锁来保护音频通道的状态，确保线程安全。
 */
class MqttProtocol : public Protocol {
public:
    // 构造函数
    MqttProtocol();
    // 析构函数
    ~MqttProtocol();

    // 启动协议
    void Start() override;
    // 发送音频数据
    void SendAudio(const std::vector<uint8_t>& data) override;
    // 打开音频通道
    bool OpenAudioChannel() override;
    // 关闭音频通道
    void CloseAudioChannel() override;
    // 判断音频通道是否已打开
    bool IsAudioChannelOpened() const override;

private:
    // 事件组句柄
    EventGroupHandle_t event_group_handle_;

    // 服务器地址
    std::string endpoint_;
    // 客户端ID
    std::string client_id_;
    // 用户名
    std::string username_;
    // 密码
    std::string password_;
    // 发布主题
    std::string publish_topic_;

    // 通道互斥锁
    std::mutex channel_mutex_;
    // Mqtt对象指针
    Mqtt* mqtt_ = nullptr;
    // Udp对象指针
    Udp* udp_ = nullptr;
    // AES加密上下文
    mbedtls_aes_context aes_ctx_;
    // AES随机数
    std::string aes_nonce_;
    // Udp服务器地址
    std::string udp_server_;
    // Udp服务器端口
    int udp_port_;
    // 本地序列号
    uint32_t local_sequence_;
    // 远程序列号
    uint32_t remote_sequence_;

    // 启动Mqtt客户端
    bool StartMqttClient(bool report_error=false);
    // 解析服务器Hello消息
    void ParseServerHello(const cJSON* root);
    // 将十六进制字符串解码为字符串
    std::string DecodeHexString(const std::string& hex_string);

    // 发送文本
    bool SendText(const std::string& text) override;
};


#endif // MQTT_PROTOCOL_H
