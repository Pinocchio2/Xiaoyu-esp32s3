#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cJSON.h>
#include <string>
#include <functional>
#include <chrono>

struct BinaryProtocol3 {
    uint8_t type;
    uint8_t reserved;
    uint16_t payload_size;
    uint8_t payload[];
} __attribute__((packed));

// 枚举类型，表示中止原因
enum AbortReason {
    // 无中止原因
    kAbortReasonNone,
    // 唤醒词检测到
    kAbortReasonWakeWordDetected
};

enum ListeningMode {
    /// 监听模式枚举，用于指定监听的行为和特性。
    ///
    /// 该枚举定义了三种不同的监听模式，每种模式都有其特定的用途和行为：
    /// - kListeningModeAutoStop: 自动停止监听模式。在这种模式下，监听会在检测到特定条件时自动停止。
    /// - kListeningModeManualStop: 手动停止监听模式。在这种模式下，监听需要用户手动停止。
    /// - kListeningModeRealtime: 实时监听模式。在这种模式下，监听会实时进行，并且需要 AEC（回声消除）支持。
    ///
    /// 使用示例：
    ///
    /// 构造函数参数：无
    ///
    /// 特殊使用限制或潜在的副作用：使用 kListeningModeRealtime 模式时，需要确保系统支持 AEC 功能，否则可能导致性能问题或回声现象。
    kListeningModeAutoStop,
    kListeningModeManualStop,
    kListeningModeRealtime // 需要 AEC 支持
};

/**
 * @brief 协议类，用于处理音频和JSON数据的传输和接收。
 *
 * 该类定义了一系列虚拟函数和回调函数，用于启动协议、打开和关闭音频通道、发送和接收音频数据、处理网络错误等。
 * 通过继承该类，可以实现具体的协议逻辑。
 *
 * 核心功能包括：
 * - 启动协议（Start）
 * - 打开和关闭音频通道（OpenAudioChannel, CloseAudioChannel）
 * - 发送和接收音频数据（SendAudio, OnIncomingAudio）
 * - 发送和接收JSON数据（SendStartListening, OnIncomingJson）
 * - 处理网络错误（OnNetworkError）
 * - 发送自定义文本消息（SendCustomText, SendCustomMessage）
 *
 * 使用示例：
 *
 * 构造函数参数：
 * 无
 *
 * 特殊使用限制或潜在的副作用：
 * - 在使用回调函数时，确保回调函数的实现是线程安全的。
 * - 在发送音频数据时，确保音频数据的格式和采样率与服务器的要求一致。
 */
class Protocol {
public:
    // 析构函数
    virtual ~Protocol() = default;

    // 获取服务器采样率
    inline int server_sample_rate() const {
        return server_sample_rate_;
    }
    // 获取服务器帧持续时间
    inline int server_frame_duration() const {
        return server_frame_duration_;
    }
    // 获取会话ID
    inline const std::string& session_id() const {
        return session_id_;
    }

    // 设置音频回调函数
    void OnIncomingAudio(std::function<void(std::vector<uint8_t>&& data)> callback);
    // 设置JSON回调函数
    void OnIncomingJson(std::function<void(const cJSON* root)> callback);
    // 设置音频通道打开回调函数
    void OnAudioChannelOpened(std::function<void()> callback);
    // 设置音频通道关闭回调函数
    void OnAudioChannelClosed(std::function<void()> callback);
    // 设置网络错误回调函数
    void OnNetworkError(std::function<void(const std::string& message)> callback);

    // 启动协议
    virtual void Start() = 0;
    // 打开音频通道
    virtual bool OpenAudioChannel() = 0;
    // 关闭音频通道
    virtual void CloseAudioChannel() = 0;
    // 判断音频通道是否打开
    virtual bool IsAudioChannelOpened() const = 0;
    // 判断音频通道是否忙碌
    virtual bool IsAudioChannelBusy() const;
    // 发送音频数据
    virtual void SendAudio(const std::vector<uint8_t>& data) = 0;
    // 发送唤醒词检测
    virtual void SendWakeWordDetected(const std::string& wake_word);
    // 发送开始监听
    virtual void SendStartListening(ListeningMode mode);
    // 发送停止监听
    virtual void SendStopListening();
    // 发送中止说话
    virtual void SendAbortSpeaking(AbortReason reason);
    // 发送IoT描述符
    virtual void SendIotDescriptors(const std::string& descriptors);
    // 发送IoT状态
    virtual void SendIotStates(const std::string& states);
    // 新增：直接发送文本消息
    virtual bool SendCustomText(const std::string& text);/////////////////////////
    // 发送带类型标识的自定义消息
    virtual bool SendCustomMessage(const std::string& type, const std::string& data);
  
    

protected:
    // JSON回调函数
    std::function<void(const cJSON* root)> on_incoming_json_;
    // 音频回调函数
    std::function<void(std::vector<uint8_t>&& data)> on_incoming_audio_;
    // 音频通道打开回调函数
    std::function<void()> on_audio_channel_opened_;
    // 音频通道关闭回调函数
    std::function<void()> on_audio_channel_closed_;
    // 网络错误回调函数
    std::function<void(const std::string& message)> on_network_error_;

    // 服务器采样率
    int server_sample_rate_ = 24000;
    // 服务器帧持续时间
    int server_frame_duration_ = 60;
    // 错误发生标志
    bool error_occurred_ = false;
    // 音频发送忙碌标志
    bool busy_sending_audio_ = false;
    // 会话ID
    std::string session_id_;
    // 最后一次收到数据的时间点
    std::chrono::time_point<std::chrono::steady_clock> last_incoming_time_;

    // 发送文本数据
    virtual bool SendText(const std::string& text) = 0;
    // 设置错误信息
    virtual void SetError(const std::string& message);
    // 判断是否超时
    virtual bool IsTimeout() const;
};

#endif // PROTOCOL_H

