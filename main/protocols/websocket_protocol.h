#ifndef _WEBSOCKET_PROTOCOL_H_
#define _WEBSOCKET_PROTOCOL_H_


#include "protocol.h"

#include <web_socket.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#define WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT (1 << 0)

/**
 * @class WebsocketProtocol
 * @brief 实现基于WebSocket的音频通信协议。
 *
 * WebsocketProtocol类继承自Protocol类，用于处理基于WebSocket的音频通信。
 * 该类提供了启动、发送音频数据、打开和关闭音频通道等功能。
 *
 * 核心功能包括：
 * - 启动WebSocket通信（Start）
 * - 发送音频数据（SendAudio）
 * - 打开音频通道（OpenAudioChannel）
 * - 关闭音频通道（CloseAudioChannel）
 * - 检查音频通道是否打开（IsAudioChannelOpened）
 *
 * 使用示例：
 * @code
 * WebsocketProtocol websocketProtocol;
 * websocketProtocol.Start();
 * if (websocketProtocol.OpenAudioChannel()) {
 *     std::vector<uint8_t> audioData = ...; // 音频数据
 *     websocketProtocol.SendAudio(audioData);
 *     websocketProtocol.CloseAudioChannel();
 * }
 * @endcode
 *
 * 构造函数参数：
 * 无
 *
 * 注意事项：
 * - 确保在调用其他方法之前先调用Start方法启动WebSocket通信。
 * - 在发送音频数据之前需要先打开音频通道。
 * - 关闭音频通道后不应再发送音频数据。
 */
class WebsocketProtocol : public Protocol {
public:
    // 构造函数
    WebsocketProtocol();
    // 析构函数
    ~WebsocketProtocol();

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
    // WebSocket指针
    WebSocket* websocket_ = nullptr;

    // 解析服务器问候
    void ParseServerHello(const cJSON* root);
    // 发送文本
    bool SendText(const std::string& text) override;
};

#endif
