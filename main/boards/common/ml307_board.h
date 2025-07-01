#ifndef ML307_BOARD_H
#define ML307_BOARD_H

#include "board.h"
#include <ml307_at_modem.h>

class Ml307Board : public Board {
protected:
    // 定义一个Ml307AtModem类型的成员变量
    Ml307AtModem modem_;

    // 获取板卡的JSON格式数据
    virtual std::string GetBoardJson() override;
    // 等待网络准备好
    void WaitForNetworkReady();

public:
    // 构造函数，初始化tx_pin和rx_pin，rx_buffer_size默认为4096
    Ml307Board(gpio_num_t tx_pin, gpio_num_t rx_pin, size_t rx_buffer_size = 4096);
    // 获取板卡类型
    virtual std::string GetBoardType() override;
    // 启动网络
    virtual void StartNetwork() override;
    // 创建Http对象
    virtual Http* CreateHttp() override;
    // 创建WebSocket对象
    virtual WebSocket* CreateWebSocket() override;
    // 创建Mqtt对象
    virtual Mqtt* CreateMqtt() override;
    // 创建Udp对象
    virtual Udp* CreateUdp() override;
    // 获取网络状态图标
    virtual const char* GetNetworkStateIcon() override;
    // 设置省电模式
    virtual void SetPowerSaveMode(bool enabled) override;

     virtual AudioCodec* GetAudioCodec() override { return nullptr; }///////
};

#endif // ML307_BOARD_H
