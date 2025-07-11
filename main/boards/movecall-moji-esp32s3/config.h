#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// Movecall Moji configuration

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_42  //MTMS
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_39  //MTCK
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_41  //MTDI
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_40  //MTDO
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_38

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_7
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_46
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_45
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

#define BUILTIN_LED_GPIO        GPIO_NUM_33 /////
#define BOOT_BUTTON_GPIO        GPIO_NUM_0


#define DISPLAY_TYPE_OLED
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define DISPLAY_MIRROR_X false // Adjust as needed for your OLED
#define DISPLAY_MIRROR_Y false // Adjust as needed for your OLED
#define DISPLAY_SWAP_XY false  // Adjust as needed for your OLED

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

// I2C pins for OLED (shared with audio codec)
#define DISPLAY_I2C_SDA_PIN  AUDIO_CODEC_I2C_SDA_PIN // GPIO_NUM_1
#define DISPLAY_I2C_SCL_PIN  AUDIO_CODEC_I2C_SCL_PIN // GPIO_NUM_2
#define DISPLAY_I2C_ADDR     0x3C // Common OLED I2C address, verify for your module (0x3C or 0x3D)
// #define DISPLAY_RESET_PIN    GPIO_NUM_18 // Optional: Define if your OLED has a RESET pin and you want to use it

//LIS2DH12
#define LIS2DH12_I2C_ADDRESS       0x19 // LIS2DH12TR I2C 地址
#define LIS2DH12_CTRL_REG1         0x20 // 控制寄存器 1
#define LIS2DH12_CTRL_REG3         0x22 // 控制寄存器 3
#define LIS2DH12_INT1_CFG          0x30 // INT1 配置寄存器
#define LIS2DH12_INT1_SRC          0x31 // INT1 状态寄存器
#define LIS2DH12_INT1_THS          0x32 // INT1 阈值寄存器
#define LIS2DH12_INT1_DURATION     0x33 // INT1 持续时间寄存器
#define LIS2DH12_OUT_XYZ           0xA8 // 读取 XYZ 数据
#define WAKEUP_INT_GPIO            GPIO_NUM_6  // 连接 LIS2DH12TR 的 INT1

#define ML307_RX_PIN GPIO_NUM_17    
#define ML307_TX_PIN GPIO_NUM_18


#define BT_RX_PIN GPIO_NUM_5
#define BT_TX_PIN GPIO_NUM_4

#define ENABLE_4G GPIO_NUM_21

// 新增网络切换按键定义
#define NETWORK_SWITCH_BUTTON_GPIO GPIO_NUM_16
//#define SWITCH_BUTTON_GPIO GPIO_NUM_15
#define INTERNAL_BUTTON_GPIO GPIO_NUM_1     //外接唤醒模块

#endif // _BOARD_CONFIG_H_
