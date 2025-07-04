#ifndef _SYSTEM_INFO_H_
#define _SYSTEM_INFO_H_

#include <string>

#include <esp_err.h>
#include <freertos/FreeRTOS.h>

/**
 * @brief SystemInfo 类用于获取系统相关的信息，如闪存大小、堆内存大小、MAC地址和芯片型号名称等。
 * 
 * 该类提供了一系列静态方法，用于获取系统的各种硬件和内存信息。这些信息对于系统监控、调试和优化非常有用。
 * 
 * 核心功能包括：
 * - 获取闪存大小
 * - 获取最小空闲堆大小
 * - 获取当前空闲堆大小
 * - 获取MAC地址
 * - 获取芯片型号名称
 * - 打印实时统计信息
 * 
 * 使用示例：
 * 
 * 构造函数：无（所有方法均为静态方法，无需实例化对象）
 * 
 * 特殊使用限制或潜在的副作用：无
 */
class SystemInfo {
public:
    // 获取闪存大小
    static size_t GetFlashSize();
    // 获取最小空闲堆大小
    static size_t GetMinimumFreeHeapSize();
    // 获取空闲堆大小
    static size_t GetFreeHeapSize();
    // 获取MAC地址
    static std::string GetMacAddress();
    // 获取芯片型号名称
    static std::string GetChipModelName();
    // 打印实时统计信息
    static esp_err_t PrintRealTimeStats(TickType_t xTicksToWait);
};

#endif // _SYSTEM_INFO_H_
