#include "es8311_audio_codec.h"

#include <esp_log.h>

static const char TAG[] = "Es8311AudioCodec";

Es8311AudioCodec::Es8311AudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
    gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
    gpio_num_t pa_pin, uint8_t es8311_addr, bool use_mclk) {
    duplex_ = true; // 是否双工
    input_reference_ = false; // 是否使用参考输入，实现回声消除
    input_channels_ = 1; // 输入通道数
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    pa_pin_ = pa_pin;
    CreateDuplexChannels(mclk, bclk, ws, dout, din);

    // Do initialize of related interface: data_if, ctrl_if and gpio_if
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);

    // Output
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = i2c_port,
        .addr = es8311_addr,
        .bus_handle = i2c_master_handle,
    };
    ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(ctrl_if_ != NULL);

    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);

    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = ctrl_if_;
    es8311_cfg.gpio_if = gpio_if_;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8311_cfg.pa_pin = pa_pin;
    es8311_cfg.use_mclk = use_mclk;
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
    codec_if_ = es8311_codec_new(&es8311_cfg);
    assert(codec_if_ != NULL);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if_,
        .data_if = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(output_dev_ != NULL);
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    input_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(input_dev_ != NULL);
    esp_codec_set_disable_when_closed(output_dev_, false);
    esp_codec_set_disable_when_closed(input_dev_, false);
    ESP_LOGI(TAG, "Es8311AudioCodec initialized");
}

Es8311AudioCodec::~Es8311AudioCodec() {
    // 关闭输出设备
    ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    // 删除输出设备
    esp_codec_dev_delete(output_dev_);
    // 关闭输入设备
    ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    // 删除输入设备
    esp_codec_dev_delete(input_dev_);

    // 删除编解码接口
    audio_codec_delete_codec_if(codec_if_);
    // 删除控制接口
    audio_codec_delete_ctrl_if(ctrl_if_);
    // 删除GPIO接口
    audio_codec_delete_gpio_if(gpio_if_);
    // 删除数据接口
    audio_codec_delete_data_if(data_if_);
}

void Es8311AudioCodec::CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
    // 断言输入采样率和输出采样率相等
    assert(input_sample_rate_ == output_sample_rate_);

    // 配置I2S通道
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0, // I2S通道编号
        .role = I2S_ROLE_MASTER, // I2S角色为主机
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM, // DMA描述符数量
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM, // DMA帧数量
        .auto_clear_after_cb = true, // 在回调后自动清除
        .auto_clear_before_cb = false, // 在回调前不自动清除
        .intr_priority = 0, // 中断优先级
    };
    // 创建I2S通道
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    // 配置I2S标准模式
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_, // 采样率
            .clk_src = I2S_CLK_SRC_DEFAULT, // 时钟源
            .mclk_multiple = I2S_MCLK_MULTIPLE_256, // MCLK倍数
			#ifdef   I2S_HW_VERSION_2    
				.ext_clk_freq_hz = 0, // 外部时钟频率
			#endif
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT, // 数据位宽
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, // 插槽位宽
            .slot_mode = I2S_SLOT_MODE_STEREO, // 插槽模式
            .slot_mask = I2S_STD_SLOT_BOTH, // 插槽掩码
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT, // WS位宽
            .ws_pol = false, // WS极性
            .bit_shift = true, // 位移
            #ifdef   I2S_HW_VERSION_2   
                .left_align = true, // 左对齐
                .big_endian = false, // 大端模式
                .bit_order_lsb = false // 位序LSB
            #endif
        },
        .gpio_cfg = {
            .mclk = mclk, // MCLK引脚
            .bclk = bclk, // BCLK引脚
            .ws = ws, // WS引脚
            .dout = dout, // DOUT引脚
            .din = din, // DIN引脚
            .invert_flags = {
                .mclk_inv = false, // MCLK反转
                .bclk_inv = false, // BCLK反转
                .ws_inv = false // WS反转
            }
        }
    };

    // 初始化I2S通道
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
    ESP_LOGI(TAG, "Duplex channels created");
}

// 设置Es8311音频编解码器的输出音量
void Es8311AudioCodec::SetOutputVolume(int volume) {
    // 检查esp_codec_dev_set_out_vol函数是否执行成功
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
    // 调用父类的SetOutputVolume函数，设置输出音量
    AudioCodec::SetOutputVolume(volume);
}

void Es8311AudioCodec::EnableInput(bool enable) {
    // 如果输入使能状态与当前状态相同，则直接返回
    if (enable == input_enabled_) {
        return;
    }
    // 如果需要使能输入
    if (enable) {
        // 设置输入采样信息
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16, // 采样位深
            .channel = 1, // 通道数
            .channel_mask = 0, // 通道掩码
            .sample_rate = (uint32_t)input_sample_rate_, // 采样率
            .mclk_multiple = 0, // MCLK倍数
        };
        // 打开输入设备
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
        // 设置输入增益
        ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(input_dev_, 40.0));
    } else {
        // 关闭输入设备
        ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    }
    // 调用父类的EnableInput函数
    AudioCodec::EnableInput(enable);
}

void Es8311AudioCodec::EnableOutput(bool enable) {
    // 如果输出状态与传入的参数相同，则直接返回
    if (enable == output_enabled_) {
        return;
    }
    if (enable) {
        // Play 16bit 1 channel
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16, // 设置采样位数为16位
            .channel = 1, // 设置声道数为1
            .channel_mask = 0, // 设置声道掩码为0
            .sample_rate = (uint32_t)output_sample_rate_, // 设置采样率为output_sample_rate_
            .mclk_multiple = 0, // 设置MCLK倍数为0
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs)); // 打开输出设备
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_)); // 设置输出音量
        
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 1); // 设置PA引脚电平为高
        }
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_)); // 关闭输出设备
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 0); // 设置PA引脚电平为低
        }
    }
    AudioCodec::EnableOutput(enable); // 设置输出设备是否启用
}

// 读取音频数据
int Es8311AudioCodec::Read(int16_t* dest, int samples) {
    // 如果输入设备已启用
    if (input_enabled_) {
        // 从输入设备读取音频数据
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(input_dev_, (void*)dest, samples * sizeof(int16_t)));
    }
    // 返回读取的样本数
    return samples;
}

int Es8311AudioCodec::Write(const int16_t* data, int samples) {
    // 如果输出设备已启用
    if (output_enabled_) {
        // 调用esp_codec_dev_write函数，将数据写入输出设备
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(output_dev_, (void*)data, samples * sizeof(int16_t)));
    }
    // 返回写入的样本数
    return samples;
}