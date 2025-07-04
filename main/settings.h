#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <nvs_flash.h>

/**
 * @brief 管理非易失性存储（NVS）中的设置。
 *
 * 该类用于简化对非易失性存储（NVS）的操作，提供了一系列方法来获取和设置字符串和整数类型的键值对。
 * 支持读取和写入模式，并且可以清除特定的键或所有键。
 *
 * 使用示例：
 *
 * @param ns 命名空间，用于区分不同的设置集合。
 * @param read_write 是否以读写模式打开NVS，默认为false（只读模式）。
 */
class Settings {
public:
    // 构造函数，初始化命名空间和读写权限
    Settings(const std::string& ns, bool read_write = false);
    // 析构函数，释放资源
    ~Settings();

    // 获取指定键的字符串值，如果不存在则返回默认值
    std::string GetString(const std::string& key, const std::string& default_value = "");
    // 设置指定键的字符串值
    void SetString(const std::string& key, const std::string& value);
    // 获取指定键的整数值，如果不存在则返回默认值
    int32_t GetInt(const std::string& key, int32_t default_value = 0);
    // 设置指定键的整数值
    void SetInt(const std::string& key, int32_t value);
    // 删除指定键
    void EraseKey(const std::string& key);
    // 删除所有键
    void EraseAll();

private:
    // 命名空间
    std::string ns_;
    // nvs句柄
    nvs_handle_t nvs_handle_ = 0;
    // 读写权限
    bool read_write_ = false;
    // 是否有修改
    bool dirty_ = false;
};

#endif
