#include "board.h"
#include "system_info.h"
#include "settings.h"
#include "display/display.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_chip_info.h>
#include <esp_random.h>

#define TAG "Board"

Board::Board() {
    Settings settings("board", true);
    uuid_ = settings.GetString("uuid");
    if (uuid_.empty()) {
        uuid_ = GenerateUuid();
        settings.SetString("uuid", uuid_);
    }
    ESP_LOGI(TAG, "UUID=%s SKU=%s", uuid_.c_str(), BOARD_NAME);
}

std::string Board::GenerateUuid() {
    // UUID v4 需要 16 字节的随机数据
    uint8_t uuid[16];
    
    // 使用 ESP32 的硬件随机数生成器
    esp_fill_random(uuid, sizeof(uuid));
    
    // 设置版本 (版本 4) 和变体位
    uuid[6] = (uuid[6] & 0x0F) | 0x40;    // 版本 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80;    // 变体 1
    
    // 将字节转换为标准的 UUID 字符串格式
    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11],
        uuid[12], uuid[13], uuid[14], uuid[15]);
    
    return std::string(uuid_str);
}

// 获取电池电量
bool Board::GetBatteryLevel(int &level, bool& charging, bool& discharging) {
    // 返回false，表示获取电池电量失败
    return false;
}

// 获取棋盘的显示对象
Display* Board::GetDisplay() {
    // 静态局部变量，只初始化一次
    static NoDisplay display;
    // 返回显示对象的指针
    return &display;
}

Led* Board::GetLed() {
    static NoLed led;
    return &led;
}

std::string Board::GetJson() {
    /* 
        {
            "version": 2,
            "flash_size": 4194304,
            "psram_size": 0,
            "minimum_free_heap_size": 123456,
            "mac_address": "00:00:00:00:00:00",
            "uuid": "00000000-0000-0000-0000-000000000000",
            "chip_model_name": "esp32s3",
            "chip_info": {
                "model": 1,
                "cores": 2,
                "revision": 0,
                "features": 0
            },
            "application": {
                "name": "my-app",
                "version": "1.0.0",
                "compile_time": "2021-01-01T00:00:00Z"
                "idf_version": "4.2-dev"
                "elf_sha256": ""
            },
            "partition_table": [
                "app": {
                    "label": "app",
                    "type": 1,
                    "subtype": 2,
                    "address": 0x10000,
                    "size": 0x100000
                }
            ],
            "ota": {
                "label": "ota_0"
            },
            "board": {
                ...
            }
        }
    */
    std::string json = "{";
    json += "\"version\":2,";
    // 获取语言代码
    json += "\"language\":\"" + std::string(Lang::CODE) + "\",";
    // 获取闪存大小
    json += "\"flash_size\":" + std::to_string(SystemInfo::GetFlashSize()) + ",";
    // 获取最小空闲堆大小
    json += "\"minimum_free_heap_size\":" + std::to_string(SystemInfo::GetMinimumFreeHeapSize()) + ",";
    // 获取MAC地址
    json += "\"mac_address\":\"" + SystemInfo::GetMacAddress() + "\",";
    // 获取UUID
    json += "\"uuid\":\"" + uuid_ + "\",";
    // 获取芯片型号名称
    json += "\"chip_model_name\":\"" + SystemInfo::GetChipModelName() + "\",";
    json += "\"chip_info\":{";

    esp_chip_info_t chip_info;
    // 获取芯片信息
    esp_chip_info(&chip_info);
    // 获取芯片型号
    json += "\"model\":" + std::to_string(chip_info.model) + ",";
    // 获取芯片核心数
    json += "\"cores\":" + std::to_string(chip_info.cores) + ",";
    // 获取芯片修订号
    json += "\"revision\":" + std::to_string(chip_info.revision) + ",";
    // 获取芯片特性
    json += "\"features\":" + std::to_string(chip_info.features);
    json += "},";

    json += "\"application\":{";
    // 获取应用程序描述
    auto app_desc = esp_app_get_description();
    // 获取应用程序名称
    json += "\"name\":\"" + std::string(app_desc->project_name) + "\",";
    // 获取应用程序版本
    json += "\"version\":\"" + std::string(app_desc->version) + "\",";
    // 获取应用程序编译时间
    json += "\"compile_time\":\"" + std::string(app_desc->date) + "T" + std::string(app_desc->time) + "Z\",";
    // 获取IDF版本
    json += "\"idf_version\":\"" + std::string(app_desc->idf_ver) + "\",";

    char sha256_str[65];
    // 获取应用程序ELF文件的SHA256哈希值
    for (int i = 0; i < 32; i++) {
        snprintf(sha256_str + i * 2, sizeof(sha256_str) - i * 2, "%02x", app_desc->app_elf_sha256[i]);
    }
    // 将SHA256哈希值转换为字符串
    json += "\"elf_sha256\":\"" + std::string(sha256_str) + "\"";
    json += "},";

    json += "\"partition_table\": [";
    // 获取分区表迭代器
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it) {
        const esp_partition_t *partition = esp_partition_get(it);
        json += "{";
        // 获取分区标签
        json += "\"label\":\"" + std::string(partition->label) + "\",";
        // 获取分区类型
        json += "\"type\":" + std::to_string(partition->type) + ",";
        // 获取分区子类型
        json += "\"subtype\":" + std::to_string(partition->subtype) + ",";
        // 获取分区地址
        json += "\"address\":" + std::to_string(partition->address) + ",";
        // 获取分区大小
        json += "\"size\":" + std::to_string(partition->size);
        json += "},";
        // 获取下一个分区
        it = esp_partition_next(it);
    }
    json.pop_back(); // Remove the last comma
    json += "],";

    json += "\"ota\":{";
    auto ota_partition = esp_ota_get_running_partition();
    json += "\"label\":\"" + std::string(ota_partition->label) + "\"";
    json += "},";

    json += "\"board\":" + GetBoardJson();

    // Close the JSON object
    json += "}";
    return json;
}