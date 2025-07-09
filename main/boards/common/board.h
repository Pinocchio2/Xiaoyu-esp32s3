#ifndef BOARD_H
#define BOARD_H

#include <http.h>
#include <web_socket.h>
#include <mqtt.h>
#include <udp.h>
#include <string>

#include "led/led.h"
#include "backlight.h"

// 创建棋盘
void* create_board();
// 声明一个名为AudioCodec的类
class AudioCodec;
class Display;
/**
 * @brief Board 类是一个抽象基类，用于表示不同类型的硬件板。
 *
 * 该类提供了一系列的纯虚函数和虚函数，用于定义硬件板的各种功能接口。
 * 通过继承 Board 类，可以实现特定硬件板的具体功能。
 * 
 * 核心功能包括：
 * - 获取硬件板的类型
 * - 获取硬件板的唯一标识（UUID）
 * - 获取和控制硬件板的各种组件（如背光、LED、音频编解码器、显示器等）
 * - 创建网络相关的对象（如 HTTP、WebSocket、MQTT、UDP）
 * - 启动网络
 * - 获取网络状态图标
 * - 获取电池电量信息
 * - 设置省电模式
 * - 获取硬件板的 JSON 描述
 * 
 * 使用示例：
 * 
 * 构造函数参数：
 * 无（构造函数是保护的，只能通过派生类进行实例化）
 * 
 * 特殊使用限制或潜在的副作用：
 * - 禁用了拷贝构造函数和赋值操作符，因此不能进行拷贝或赋值操作。
 * - 通过 GetInstance 方法获取单例实例，确保全局只有一个 Board 实例。
 */
class Board {
private:
    Board(const Board&) = delete;// 禁用拷贝构造函数
    Board& operator=(const Board&) = delete;// 禁用赋值操作
    //virtual std::string GetBoardJson() = 0;

protected:
    Board();
    std::string GenerateUuid();

    // 软件生成的设备唯一标识
    std::string uuid_;

public:
    // 获取单例对象// 定义一个静态方法来获取 Board 类的唯一实例
    //静态对象可以通过类名直接调用，而不需要创建类的实例
static Board& GetInstance() {
    // 静态局部变量，用于存储 Board 类的唯一实例
    // 在第一次调用 GetInstance 时初始化，并且是线程安全的
    static Board* instance = static_cast<Board*>(create_board());
    
    // 返回 instance 指针指向的 Board 对象的引用
    return *instance;
}
 
    // 虚析构函数
    virtual ~Board() = default;
    // 获取板子类型
    virtual std::string GetBoardType() = 0;
    // 获取设备唯一标识
    virtual std::string GetUuid() { return uuid_; }
    // 获取背光对象
    virtual Backlight* GetBacklight() { return nullptr; }
    // 获取LED对象
    virtual Led* GetLed();
    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() = 0;
    // 获取显示对象
    virtual Display* GetDisplay();
    // 创建HTTP对象
    virtual Http* CreateHttp() = 0;
    // 创建WebSocket对象
    virtual WebSocket* CreateWebSocket() = 0;
    // 创建Mqtt对象
    virtual Mqtt* CreateMqtt() = 0;
    // 创建Udp对象
    virtual Udp* CreateUdp() = 0;
    // 启动网络
    virtual void StartNetwork() = 0;
    // 获取网络状态图标
    virtual const char* GetNetworkStateIcon() = 0;
    // 获取电池电量
    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging);
    // 获取板子JSON
    virtual std::string GetJson();
    // 设置省电模式
    virtual void SetPowerSaveMode(bool enabled) = 0;
    // 获取板子JSON
    virtual std::string GetBoardJson() = 0;///////新增////////
};

#define DECLARE_BOARD(BOARD_CLASS_NAME)\
void* create_board(){ \
    return new BOARD_CLASS_NAME();  \
}

#endif // BOARD_H
