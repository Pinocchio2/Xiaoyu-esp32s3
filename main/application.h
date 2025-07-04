// [语法点] 头文件保护 (Header Guard)
// 这是防止头文件被重复包含的预处理指令。如果 _APPLICATION_H_ 这个宏尚未定义，
// 那么就定义它，并处理到 #endif 之间的代码。如果下次再 #include 这个文件，
// 由于 _APPLICATION_H_ 已被定义，#ifndef 的条件为假，中间的代码会被直接跳过。
// 这可以防止因重复包含导致的类、函数重定义等编译错误。
#ifndef _APPLICATION_H_
#define _APPLICATION_H_

// [语法点] #include <...> vs "..."
// 使用尖括号 <...> 告诉编译器在系统标准库路径下查找头文件。
// 这是用于包含像 FreeRTOS、C++标准库等外部库的。
#include <freertos/FreeRTOS.h>      // FreeRTOS 实时操作系统的核心功能
#include <freertos/event_groups.h> // FreeRTOS 事件组 API，一种强大的任务同步机制
#include <freertos/task.h>           // FreeRTOS 任务创建、删除、控制等 API

#include <esp_timer.h> // ESP-IDF 框架提供的高精度定时器组件

// 包含 C++ 标准模板库 (STL) 和其他标准功能
#include <string>               // [C++] `std::string` 类，用于处理可变长度的字符串
#include <mutex>                // [C++] `std::mutex` 类，用于实现互斥锁，保护多任务访问的共享资源
#include <list>                 // [C++] `std::list` 容器，一个双向链表，适合频繁插入和删除操作
#include <vector>               // [C++] `std::vector` 容器，一个动态数组，支持快速随机访问
#include <condition_variable>   // [C++] `std::condition_variable`，与互斥锁配合，实现等待/通知的线程同步模式

#include <opus_encoder.h>       // Opus 音频编码库
#include <opus_decoder.h>       // Opus 音频解码库
#include <opus_resampler.h>     // Opus 音频重采样库

// 使用双引号 "..." 告诉编译器首先在当前项目目录（或指定的相对路径）下查找头文件。
// 这是用于包含项目内部的自定义模块的。
#include "protocol.h"           // 项目自定义的网络通信协议模块
#include "ota.h"                // 项目自定义的 OTA (Over-the-Air) 空中升级模块
#include "background_task.h"    // 项目自定义的后台任务模块

// [语法点] 条件编译 (Conditional Compilation)
// #if 是预处理指令，它会检查后面的宏是否为真（非零）。
// 这里的 CONFIG_... 宏通常是在 ESP-IDF 的 menuconfig 工具中配置的。
// 这使得固件可以按需裁剪，只编译和链接需要的功能，节省 Flash 和 RAM。
#if CONFIG_USE_WAKE_WORD_DETECT
#include "wake_word_detect.h" // 如果启用了唤醒词功能，则包含此模块
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
#include "audio_processor.h"  // 如果启用了音频处理，则包含此模块
#endif

// [语法点] 宏定义 (Macros)
// 使用 #define 创建一个文本替换的宏。在预处理阶段，代码中所有出现的宏名都会被替换为其定义的内容。
// 这里使用位移操作 `(1 << n)` 创建用作事件标志的位掩码 (bitmask)，每个标志占据一个独立的二进制位，
// 这样就可以通过位或 `|` 操作将多个事件合并，通过位与 `&` 操作检查特定事件。
#define SCHEDULE_EVENT               (1 << 0) // -> 二进制 0001
#define AUDIO_INPUT_READY_EVENT      (1 << 1) // -> 二进制 0010
#define AUDIO_OUTPUT_READY_EVENT     (1 << 2) // -> 二进制 0100
#define CHECK_NEW_VERSION_DONE_EVENT (1 << 3) // -> 二进制 1000

// [语法点] 枚举 (Enumeration)
// `enum` 用于定义一组有名称的整型常量，构成一个枚举类型。
// 它比用一组 #define 宏更具优势：
// 1. 类型安全：`DeviceState` 是一个独立的类型，不能随意和其他整数类型混用。
// 2. 作用域：枚举成员属于 `DeviceState` 类型。
// 3. 调试友好：调试器可以显示枚举的名称（如 kDeviceStateIdle）而不是原始数字。
// 注意：这是C风格的enum，更现代的C++推荐使用 `enum class`，它提供了更强的类型安全和作用域限制。
enum DeviceState {
    kDeviceStateUnknown,        // 未知状态
    kDeviceStateStarting,       // 启动中
     kDeviceStateWifiConfiguring, // WiFi配置中
    kDeviceStateIdle,           // 空闲
    kDeviceStateConnecting,     // 连接中
    kDeviceStateListening,      // 监听中
    kDeviceStateSpeaking,       // 说话中
    kDeviceStateUpgrading,      // 升级中
    kDeviceStateActivating,     // 激活中
    kDeviceStateFatalError      // 致命错误
};

#define OPUS_FRAME_DURATION_MS 60 // 定义一个常量宏

/**
 * @class Application
 * @brief 应用程序主类，采用单例模式。
 */
class Application {
public: // 公共访问区：类的外部可以直接访问这里的成员

    // [语法点] 静态成员函数 (Static Member Function) 与单例模式
    // `static` 关键字表明 `GetInstance` 函数属于 `Application` 类本身，而不是类的某个特定实例。
    // 因此，你可以直接通过 `Application::GetInstance()` 来调用它，而无需先创建一个 Application 对象。
    // 返回类型 `Application&` 是对 Application 对象的引用，这意味着调用者得到的是原始实例本身，
    // 而不是它的一个副本，并且可以修改它。
    static Application& GetInstance() {
        // [语法点] 静态局部变量 (Static Local Variable)
        // 这个 `instance` 变量也用 `static` 修饰，但它是一个局部变量。
        // 这意味着：
        // 1. 生命周期：它在程序第一次执行到这行代码时被创建，并会一直存活到程序结束。
        // 2. 初始化：它只会被初始化一次。从 C++11 开始，这种初始化是线程安全的。
        // 这两点是实现单例模式的关键。
        static Application instance; 
        return instance;
    }

    // [语法点] 删除特殊成员函数 (`= delete`)
    // 为了严格保证单例，我们必须禁止任何形式的实例复制。
    // `Application(const Application&) = delete;` 这行代码删除了拷贝构造函数。
    // `Application& operator=(const Application&) = delete;` 这行代码删除了拷贝赋值运算符。
    // 任何试图拷贝 Application 实例的代码都会在编译时直接报错。
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // --- 公共接口 (Public API) ---
    void Start();

    // [语法点] `const` 成员函数
    // 函数声明末尾的 `const` 是一个承诺：这个函数不会修改任何类的成员变量。
    // 这是一种良好的编程实践，提高了代码的可读性和安全性。
    // 只有 `const` 成员函数才能被一个 `const` 对象调用。
    DeviceState GetDeviceState() const { return device_state_; }
    bool IsVoiceDetected() const { return voice_detected_; }

    // [语法点] `std::function`
    // `Schedule` 方法接受一个 `std::function<void()>` 类型的参数。
    // `std::function` 是一个极其灵活的工具，它可以包装任何“可调用”的东西，
    // 比如普通函数指针、Lambda表达式、类的成员函数等。
    // 这里它用于实现一个回调机制，让其他模块可以“安排”一个任务到主事件循环中执行。
    void Schedule(std::function<void()> callback);
    void SetDeviceState(DeviceState state);

    // [语法点] 默认参数 和 `std::string_view`
    // `emotion` 和 `sound` 参数有默认值，调用时可以不提供它们。
    // `std::string_view` (C++17) 是一个非拥有的字符串“视图”。它只包含一个指向字符串的指针和长度。
    // 当你传递一个字符串字面量（如 "doorbell.wav"）给它时，不会发生内存分配，效率远高于 `const std::string&`。
    void Alert(const char* status, const char* message, const char* emotion = "", const std::string_view& sound = "");
    // 关闭提示框
    void DismissAlert();
    // 中止语音输出
    void AbortSpeaking(AbortReason reason);
    // 切换聊天状态
    void ToggleChatState();
    // 改变聊天状态
    void ChangeChatState();
    // 开始监听
    void StartListening();
    // 停止监听
    void StopListening();
    // 更新物联网状态
    void UpdateIotStates();
    // 重启设备
    void Reboot();
    // 调用唤醒词
    void WakeWordInvoke(const std::string& wake_word);
    // 播放声音
    void PlaySound(const std::string_view& sound);
    // 是否可以进入睡眠模式
    bool CanEnterSleepMode();

private: // 私有访问区：只有 Application 类的成员函数才能访问这里的成员
    
    // 私有的构造函数和析构函数是单例模式的另一个关键。
    // 因为构造函数是私有的，所以外部无法通过 `Application myApp;` 或 `new Application();` 创建实例。
    Application();
    ~Application();

    // --- 成员变量 (Member Variables) ---
    // 这些变量构成了 Application 类的内部状态。
#if CONFIG_USE_WAKE_WORD_DETECT
    WakeWordDetect wake_word_detect_; // 作为成员变量的对象，其生命周期与 Application 对象绑定。
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
    AudioProcessor audio_processor_; 
#endif
    Ota ota_;
    std::mutex mutex_;
    std::list<std::function<void()>> main_tasks_;

    // [语法点] 智能指针 `std::unique_ptr`
    // `unique_ptr` 独占式地“拥有”它所指向的对象。当 `unique_ptr` 本身被销毁时
    //（例如，当 `Application` 实例被销毁时），它会自动调用 `delete` 来释放其指向的 `Protocol` 对象。
    // 这就避免了手动管理内存（`new`/`delete`），极大地减少了内存泄漏的风险。
    std::unique_ptr<Protocol> protocol_;
    
    // FreeRTOS 和 ESP-IDF 的句柄类型。初始化为 `nullptr` 是一个好习惯，
    // 可以方便地检查它们是否已经被成功创建。
    EventGroupHandle_t event_group_ = nullptr;
    esp_timer_handle_t clock_timer_handle_ = nullptr;

    // [语法点] `volatile` 关键字
    // `volatile` 告诉编译器，这个变量的值可能会在意想不到的时候被改变（比如被另一个线程或中断服务程序修改）。
    // 这会阻止编译器对该变量的访问进行优化（例如，将值缓存到CPU寄存器中），
    // 确保每次访问 `device_state_` 都是直接从主内存中读取。
    volatile DeviceState device_state_ = kDeviceStateUnknown;
    ListeningMode listening_mode_ = kListeningModeAutoStop;

#if CONFIG_USE_REALTIME_CHAT
    bool realtime_chat_enabled_ = true;
#else
    bool realtime_chat_enabled_ = false;
#endif
    bool aborted_ = false;
    bool voice_detected_ = false;
    bool busy_decoding_audio_ = false;
    int clock_ticks_ = 0;
    TaskHandle_t check_new_version_task_handle_ = nullptr;

    TaskHandle_t audio_loop_task_handle_ = nullptr;
    TaskHandle_t uart_listen_task_handle_ = nullptr;

    BackgroundTask* background_task_ = nullptr; // 这是一个裸指针，需要手动管理其生命周期（创建和删除）。

    // [语法点] `std::chrono` 时间库
    // `std::chrono` 是 C++11 引入的现代时间处理库。
    // `steady_clock` 提供了一个单调递增的时钟，不受系统时间调整的影响，最适合用来测量时间间隔。
    std::chrono::steady_clock::time_point last_output_time_; 

    std::list<std::vector<uint8_t>> audio_decode_queue_;
    std::condition_variable audio_decode_cv_;

    std::unique_ptr<OpusEncoderWrapper> opus_encoder_;
    std::unique_ptr<OpusDecoderWrapper> opus_decoder_;

    OpusResampler input_resampler_;
    OpusResampler reference_resampler_;
    OpusResampler output_resampler_;

    // --- 私有方法 (Private Methods) ---
    // 这些是类的内部实现细节，外部调用者不需要知道它们的存在。
    // 主事件循环
    void MainEventLoop();
    // UART监听任务
    void UartListenTask();
    // 音频输入
     void OnAudioInput();
    // 音频输出
    void OnAudioOutput();
    // 读取音频数据
    void ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples);
    // 重置解码器
    void ResetDecoder();
    // 设置解码采样率
    void SetDecodeSampleRate(int sample_rate, int frame_duration);
    // 检查新版本
    void CheckNewVersion();
    // 显示激活码
    void ShowActivationCode();
    // 时钟定时器
    void OnClockTimer();
    // 设置监听模式
    void SetListeningMode(ListeningMode mode);
    // 音频循环
    void AudioLoop();
};

#endif // _APPLICATION_H_