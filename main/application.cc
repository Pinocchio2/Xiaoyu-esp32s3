#include "application.h"
#include "board.h"
#include "display.h"
#include "system_info.h"
#include "ml307_ssl_transport.h"
#include "audio_codec.h"
#include "mqtt_protocol.h"
#include "websocket_protocol.h"
#include "font_awesome_symbols.h"
#include "iot/thing_manager.h"
#include "assets/lang_config.h"
#include "stdio.h"
#include <cstring>
#include <esp_log.h>
#include <cJSON.h>
#include <driver/gpio.h>
#include <arpa/inet.h>

#define TAG "Application"


static const char* const STATE_STRINGS[] = {
    "unknown",
    "starting",
    "configuring",
    "idle",
    "connecting",
    "listening",
    "speaking",
    "upgrading",
    "activating",
    "fatal_error",
    "invalid_state"
};

// [函数定义] Application 类的构造函数。
// 当通过 Application::GetInstance() 首次创建类的唯一实例时，此函数被自动调用。
// [设计思想] 这个构造函数是 RAII (Resource Acquisition Is Initialization，资源获取即初始化) 思想的绝佳体现。
// 它的核心职责是：为新创建的对象申请并配置好所有必需的系统资源。
Application::Application() {
    // [资源获取 - FreeRTOS 事件组]
    // 调用 FreeRTOS 的 API `xEventGroupCreate` 来创建一个“事件组”。
    // 事件组是 FreeRTOS 中用于任务间同步和通信的强大工具。
    // API 返回一个类型为 `EventGroupHandle_t` 的句柄(Handle)，它相当于操作系统内部该资源的“身份证”。
    // 我们将这个句柄保存在成员变量 `event_group_` 中，以便在对象的整个生命周期内使用。
    event_group_ = xEventGroupCreate();

    // [资源获取 - C++ 动态内存]
    // 使用 C++ 的 `new` 关键字在堆(heap)内存上动态地创建一个 `BackgroundTask` 对象。
    // 参数 `4096 * 8` (即 32768) 很可能是传递给 `BackgroundTask` 构造函数的、其内部封装的 FreeRTOS 任务所需的“任务栈大小”(Stack Size)。
    // 注意：`new` 返回的是一个裸指针(`BackgroundTask*`)。这意味着我们必须在析构函数 `~Application()` 中手动调用 `delete` 来释放这块内存，以防止内存泄漏。
    background_task_ = new BackgroundTask(4096 * 8);

    // [资源配置 - ESP-IDF 定时器]
    // 定义一个 `esp_timer_create_args_t` 类型的结构体，用于配置即将创建的定时器。
    // 这里使用了 C99 的“指定初始化器” (Designated Initializer) 语法（`.成员名 = 值`），
    // 这种语法在嵌入式开发中非常流行，因为它可读性强，且不依赖于结构体成员的顺序。
    esp_timer_create_args_t clock_timer_args = {
        // [核心语法点] C++11 Lambda 表达式作为 C 风格回调函数。
        // `.callback` 成员需要一个 C 语言函数指针 `void (*)(void*)`。
        // 这个无捕获 `[]` 的 Lambda 可以被隐式转换为此函数指针类型，是连接 C++ 代码和 C 库 API 的标准“桥梁模式”。
        .callback = [](void* arg) {
            // 在回调函数内部，将通用的 `void*` 参数安全地、显式地转换回其原始的 `Application*` 类型。
            Application* app = static_cast<Application*>(arg);
            // 通过转换后的指针，调用对象的成员函数。
            app->OnClockTimer();
        },
        // [核心语法点] `this` 指针作为回调参数。
        // `this` 是一个指向当前 `Application` 对象实例的指针。
        // 将它赋值给 `.arg`，意味着当定时器触发、回调函数被调用时，`this` 的值会被传入 `arg` 参数。
        // 这使得上面那个静态的 Lambda 回调函数在被触发时，能准确地知道要操作哪一个对象实例。
        .arg = this,
        // [ESP-IDF 特定配置]
        // ESP_TIMER_TASK 表示回调函数将在一个专用的高优先级“定时器任务”中执行，而不是在中断(ISR)中。
        // 这允许在回调中使用更多样的 FreeRTOS API。
        .dispatch_method = ESP_TIMER_TASK,
        // 为定时器命名，主要用于调试目的。
        .name = "clock_timer",
        // 一个优化选项，如果事件队列满了，则跳过未处理的事件。
        .skip_unhandled_events = true
    };
    
    // [资源获取 - ESP-IDF 定时器]
    // 调用 `esp_timer_create` API，传入配置结构体和用于接收句柄的变量地址。
    // API 执行后，`clock_timer_handle_` 成员变量将保存这个新创建的定时器的句柄。
    esp_timer_create(&clock_timer_args, &clock_timer_handle_);
    
    // [启动资源]
    // 调用 `esp_timer_start_periodic` API 来启动定时器。
    // `clock_timer_handle_` 是要启动的定时器。
    // `1000000` 是定时周期，单位为微秒 (microseconds)。1,000,000 微秒 = 1 秒。
    // 这行代码的效果是：每隔 1 秒，上面定义的 Lambda 回调函数就会被自动执行一次。
    esp_timer_start_periodic(clock_timer_handle_, 1000000);
} // 构造函数结束，Application 对象已完成初始化，所有资源已准备就绪。
// [函数定义] Application 类的析构函数。
// [核心职责] 它的唯一职责是“释放资源”，与构造函数中“获取资源”的行为一一对应。
// 这是 RAII (资源获取即初始化) 设计思想的后半部分，也是最关键的部分。
// 当 Application 的唯一实例在程序结束时被销毁，此函数会自动被调用，确保不会有任何资源泄漏。
Application::~Application() {
    // [资源释放 - ESP-IDF 定时器]
    // [健壮性检查] 在尝试停止或删除任何系统资源之前，必须检查其句柄是否为 `nullptr`。
    // 如果在构造函数中资源创建失败，句柄就会是 `nullptr`，这个检查可以防止程序因操作无效句柄而崩溃。
    if (clock_timer_handle_ != nullptr) {
        // 1. 调用 `esp_timer_stop` API，安全地停止定时器，确保它不再触发回调。
        esp_timer_stop(clock_timer_handle_);
        // 2. 调用 `esp_timer_delete` API，彻底删除定时器对象，释放其占用的所有内存和系统资源。
        esp_timer_delete(clock_timer_handle_);
    }

    // [资源释放 - C++ 动态内存]
    // [健壮性检查] 同样，在 `delete` 一个指针前，检查它是否为 `nullptr` 是一个好习惯。
    // (虽然对 `nullptr` 调用 `delete` 本身是安全的、无操作的，但显式检查能让代码意图更清晰)。
    if (background_task_ != nullptr) {
        // [语法点] `delete` 关键字。
        // `delete` 用于释放由 `new` 关键字在堆(heap)上分配的内存。
        // 它会首先调用 `background_task_` 所指向对象的析构函数（执行该对象的内部清理），
        // 然后再释放对象本身占用的内存。
        delete background_task_;
    }

    // [资源释放 - FreeRTOS 事件组]
    // 调用 FreeRTOS 的 API `vEventGroupDelete`，传入之前创建的事件组句柄。
    // 此函数会删除事件组，并释放其占用的内核资源。
    // 注意：这里没有 `if` 检查，意味着代码假定 `event_group_` 总是有效的。
    // 一个更健壮的实现也会在这里加上 `if (event_group_ != nullptr)` 的检查。
    vEventGroupDelete(event_group_);
} // 析构函数结束，所有 Application 对象持有的资源均已被安全释放。

// [函数定义] Application 类的成员函数 CheckNewVersion。
// [核心职责] 此函数负责在设备启动时，完成版本检查、固件升级(OTA)以及设备激活这三大关键流程。
// 这是一个阻塞式的、具有复杂流程控制的函数，是设备上线前的“看门人”。
void Application::CheckNewVersion() {
    // [初始化] 定义重试逻辑所需的常量和变量。
    const int MAX_RETRY = 10; // 最大重试次数，避免无限重试。
    int retry_count = 0;      // 当前重试次数计数器。
    int retry_delay = 10;     // 初始重试延迟时间，单位为秒。

    // [流程控制] 使用一个无限循环 `while (true)` 来包裹整个检查和激活流程。
    // 只有当所有步骤（版本检查、可选的激活）都完成后，才会通过 `break` 语句退出循环。
    while (true) {
        // [状态变更] 无论循环多少次，每次开始都先将设备状态设置为“激活中”。
        SetDeviceState(kDeviceStateActivating);
        // [语法点] `auto` 关键字
        // 编译器会自动推断 `display` 的类型为 `Display*`，让代码更简洁。
        // 这里通过单例模式获取到显示对象的指针。
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus(Lang::Strings::CHECKING_NEW_VERSION); // 更新UI，告知用户正在检查新版本。

        // [网络操作 & 重试逻辑]
        // 尝试调用 ota_ 对象的 CheckVersion 方法，这很可能是一个发起网络请求的操作。
        if (!ota_.CheckVersion()) {
            // 如果检查失败，则进入重试流程。
            retry_count++;
            if (retry_count >= MAX_RETRY) {
                // 如果达到最大重试次数，则记录错误日志并放弃，直接从函数返回。
                ESP_LOGE(TAG, "Too many retries, exit version check");
                return;
            }
            // 记录警告日志，告知当前的重试状态。
            ESP_LOGW(TAG, "Check new version failed, retry in %d seconds (%d/%d)", retry_delay, retry_count, MAX_RETRY);
            
            // [FreeRTOS API] 使用 for 循环和 `vTaskDelay` 实现非阻塞式等待。
            // `vTaskDelay` 会使当前任务进入阻塞状态，让出CPU给其他任务，非常高效。
            // `pdMS_TO_TICKS(1000)` 是一个宏，将1000毫秒转换为FreeRTOS内部的时间节拍数(ticks)。
            for (int i = 0; i < retry_delay; i++) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                // 在等待期间，如果设备状态被其他任务改变为空闲状态，则提前结束等待。
                if (device_state_ == kDeviceStateIdle) {
                    break;
                }
            }

            // [设计模式] 指数退避 (Exponential Backoff)。
            // 每次重试失败后，将等待时间加倍。这是一种非常成熟的网络编程策略，
            // 它可以避免在服务器端出现问题时，客户端以过于频繁的请求“攻击”服务器。
            retry_delay *= 2; 
            
            // `continue` 关键字会立即结束本次循环，并跳转到 `while(true)` 的下一次循环的开始处，重新尝试检查版本。
            continue;
        }
        
        // 如果 `ota_.CheckVersion()` 成功，则重置重试计数器和延迟时间，为后续可能的其他失败做准备。
        retry_count = 0;
        retry_delay = 10;

        // [OTA 升级流程]
        // 检查是否有新版本可用。
        if (ota_.HasNewVersion()) {
            // 通过声音和UI提示用户即将开始升级。
            Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "happy", Lang::Sounds::P3_UPGRADE);
            vTaskDelay(pdMS_TO_TICKS(3000)); // 等待3秒，让用户有时间看到提示。
            SetDeviceState(kDeviceStateUpgrading); // 将设备状态切换为“升级中”。
            
            // --- [关键步骤] OTA 升级前的准备与清理工作 ---
            // 为了保证 OTA 这个关键过程的稳定性，必须将系统置于一个已知的、最简单的状态。
            display->SetIcon(FONT_AWESOME_DOWNLOAD);
            std::string message = std::string(Lang::Strings::NEW_VERSION) + ota_.GetFirmwareVersion();
            display->SetChatMessage("system", message.c_str());

            auto& board = Board::GetInstance();
            // 1. 禁用省电模式，确保升级期间芯片全速运行，网络稳定。
            board.SetPowerSaveMode(false); 
#if CONFIG_USE_WAKE_WORD_DETECT
            // 2. 停止唤醒词检测任务，释放其占用的音频等资源。
            wake_word_detect_.StopDetection();
#endif
            // 3. 彻底关闭音频输入输出，避免任何音频操作干扰升级。
            auto codec = board.GetAudioCodec();
            codec->EnableInput(false);
            codec->EnableOutput(false);
            
            // 4. 清理共享数据队列，并确保后台任务已完全停止。
            {
                // [语法点] `std::lock_guard` RAII风格的互斥锁。
                // 在进入这个代码块时自动锁定 `mutex_`，在退出时（右花括号）自动解锁。
                // 保证了对共享资源 `audio_decode_queue_` 的访问是线程安全的。
                std::lock_guard<std::mutex> lock(mutex_);
                audio_decode_queue_.clear();
            }
            background_task_->WaitForCompletion(); // 等待后台任务队列中所有任务执行完毕。
            delete background_task_;              // 删除后台任务对象，释放其资源。
            background_task_ = nullptr;          // 删除后将指针设为 nullptr，防止悬挂指针。
            vTaskDelay(pdMS_TO_TICKS(1000));     // 短暂延时，确保系统状态稳定。

            // [核心操作] 启动 OTA 升级。
            // `ota_.StartUpgrade` 接受一个回调函数（Lambda表达式），用于在升级过程中报告进度。
            // [语法点] Lambda 捕获列表 `[display]`。
            // 它将外部的 `display` 变量“捕获”到 Lambda 内部使用。
            // 默认是按值捕获，相当于在 Lambda 内部拥有了一个 `display` 的副本，可以在回调中安全地更新UI。
            ota_.StartUpgrade([display](int progress, size_t speed) {
                char buffer[64];
                // `snprintf` 是一个安全的字符串格式化函数，它会防止缓冲区溢出。
                snprintf(buffer, sizeof(buffer), "%d%% %zuKB/s", progress, speed / 1024);
                display->SetChatMessage("system", buffer);
            });

            // [流程控制] 正常情况下，`ota_.StartUpgrade` 成功后会直接重启设备，代码不会执行到这里。
            // 如果执行到这里，说明升级失败了。
            display->SetStatus(Lang::Strings::UPGRADE_FAILED);
            ESP_LOGI(TAG, "Firmware upgrade failed...");
            vTaskDelay(pdMS_TO_TICKS(3000));
            Reboot(); // 重启设备。
            return;   // 结束函数。
        }

        // [设备激活流程]
        // 如果没有新版本，则标记当前运行的固件版本是有效的。
        ota_.MarkCurrentVersionValid();
        
        // 如果服务器没有返回激活码信息，说明设备无需激活。
        if (!ota_.HasActivationCode() && !ota_.HasActivationChallenge()) {
            // [FreeRTOS API] 设置事件位，通知其他可能正在等待的任务（如此处的`Start`函数）本流程已完成。
            xEventGroupSetBits(event_group_, CHECK_NEW_VERSION_DONE_EVENT);
            // `break` 退出 `while(true)` 循环，函数执行完毕。
            break;
        }

        // 如果需要激活，则更新UI并调用 `ShowActivationCode` 进行播报。
        display->SetStatus(Lang::Strings::ACTIVATION);
        if (ota_.HasActivationCode()) {
            ShowActivationCode();
        }

        // [流程控制] 轮询激活状态。
        // 使用一个 `for` 循环尝试激活设备，最多10次。
        for (int i = 0; i < 10; ++i) {
            ESP_LOGI(TAG, "Activating... %d/%d", i + 1, 10);
            esp_err_t err = ota_.Activate(); // 这是一个阻塞式的网络请求。
            if (err == ESP_OK) { // 如果成功
                xEventGroupSetBits(event_group_, CHECK_NEW_VERSION_DONE_EVENT); // 设置完成事件
                break; // 退出激活循环
            } else if (err == ESP_ERR_TIMEOUT) { // 如果是超时错误
                vTaskDelay(pdMS_TO_TICKS(3000)); // 等待3秒再试
            } else { // 其他错误
                vTaskDelay(pdMS_TO_TICKS(10000)); // 等待10秒再试
            }
            if (device_state_ == kDeviceStateIdle) { // 同样提供一个外部中断的出口
                break;
            }
        }
    } // while(true) 循环结束
} // CheckNewVersion 函数结束

// [函数定义] Application 类的成员函数 ShowActivationCode。
// [核心职责] 以用户友好的方式（UI显示 + 语音播报）来呈现设备激活码。
void Application::ShowActivationCode() {
    // [语法点] `auto&` (带引用的类型推导)
    // `auto` 会让编译器自动推断变量类型（这里是 std::string）。
    // `&` 表示 `message` 和 `code` 是“引用”(Reference)，而不是副本(Copy)。
    // 这意味着它们直接指向 ota_ 对象内部的原始字符串数据，避免了不必要的内存分配和字符串拷贝，非常高效。
    auto& message = ota_.GetActivationMessage();
    auto& code = ota_.GetActivationCode();

    // [语法点] 局部结构体 (Local Struct)
    // 在函数内部定义一个结构体，其作用域仅限于此函数。
    // 这是一种很好的封装实践，当一个数据结构只为某个特定函数服务时，可以避免污染类或全局的命名空间。
    struct digit_sound {
        char digit;
        // `std::string_view` 是一个非拥有的字符串“视图”，它只包含一个指针和长度。
        // 在这里使用 `const std::string_view&` 同样是为了效率，避免任何字符串拷贝。
        const std::string_view& sound;
    };

    // [语法点] 静态常量 `std::array` (Static Constant std::array)
    // `static` 关键字是这里的关键性能优化：它确保 `digit_sounds` 这个查找表只在程序第一次调用此函数时被创建和初始化一次。
    // 后续所有对 ShowActivationCode 的调用都会直接复用这个已存在的数组，避免了重复创建的开销。
    // `const` 确保这个查找表在初始化后不能被修改。
    // `std::array<T, N>` 是一个C++11的容器，它封装了一个固定大小的C风格数组，但提供了迭代器等现代接口，更安全易用。
    static const std::array<digit_sound, 10> digit_sounds{{
        // 使用聚合初始化(Aggregate Initialization)来填充数组。
        digit_sound{'0', Lang::Sounds::P3_0},
        digit_sound{'1', Lang::Sounds::P3_1}, 
        digit_sound{'2', Lang::Sounds::P3_2},
        digit_sound{'3', Lang::Sounds::P3_3},
        digit_sound{'4', Lang::Sounds::P3_4},
        digit_sound{'5', Lang::Sounds::P3_5},
        digit_sound{'6', Lang::Sounds::P3_6},
        digit_sound{'7', Lang::Sounds::P3_7},
        digit_sound{'8', Lang::Sounds::P3_8},
        digit_sound{'9', Lang::Sounds::P3_9}
    }};

    // 调用 Alert 函数，这个函数负责在屏幕上显示消息，并播放一个整体的提示音（如“请输入激活码”）。
    // `message.c_str()` 将 `std::string` 转换为C风格的 `const char*` 指针，以兼容可能需要C字符串的API。
    Alert(Lang::Strings::ACTIVATION, message.c_str(), "happy", Lang::Sounds::P3_ACTIVATION);

    // [语法点] 基于范围的 for 循环 (Range-based for loop)
    // 这是 C++11 引入的遍历容器的现代化、更安全的方式。它会自动处理迭代器和边界，代码更简洁。
    // `const auto& digit`：对于 `code` 字符串中的每一个字符，创建一个名为 `digit` 的常量引用。
    for (const auto& digit : code) {
        // [语法点] `std::find_if` 算法和 Lambda 谓词
        // `std::find_if` 是C++标准库中的一个算法，它在指定范围 (`digit_sounds.begin()` 到 `digit_sounds.end()`)
        // 中查找第一个满足特定条件的元素。
        // 第三个参数是一个返回布尔值的“谓词”(Predicate)，这里我们用一个 Lambda 表达式来定义它。
        auto it = std::find_if(digit_sounds.begin(), digit_sounds.end(),
            // [语法点] Lambda 捕获列表 `[digit]`
            // 这个 Lambda 需要比较来自外部 for 循环的 `digit` 变量。
            // `[digit]` 按值“捕获”了 `digit` 变量，使得它在 Lambda 函数体内部可见并可用。
            // `(const digit_sound& ds)` 是 Lambda 的参数，`ds` 代表 `digit_sounds` 中正在被检查的那个元素。
            [digit](const digit_sound& ds) { return ds.digit == digit; });
        
        // `std::find_if` 在找到元素时返回一个指向该元素的迭代器；如果没找到，则返回容器的 `end()` 迭代器。
        // 因此，`it != digit_sounds.end()` 是检查是否查找成功的标准方法。
        if (it != digit_sounds.end()) {
            // 如果找到了，`it` 就是一个有效的迭代器。
            // `it->sound` 使用箭头运算符 `->` 来访问迭代器所指向的 `digit_sound` 对象的 `sound` 成员。
            // 然后调用 PlaySound 函数，将对应的声音播放出来。
            PlaySound(it->sound);
        }
    }
} // 函数结束，激活码已显示并播报完毕。

void Application::Alert(const char* status, const char* message, const char* emotion, const std::string_view& sound) {
    ESP_LOGW(TAG, "Alert %s: %s [%s]", status, message, emotion);
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(status);
    display->SetEmotion(emotion);
    display->SetChatMessage("system", message);
    if (!sound.empty()) {
        ResetDecoder();
        PlaySound(sound);
    }
}

void Application::DismissAlert() {
    if (device_state_ == kDeviceStateIdle) {
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus(Lang::Strings::STANDBY);
        display->SetEmotion("neutral");
        display->SetChatMessage("system", "");
    }
}

void Application::PlaySound(const std::string_view& sound) {
    // Wait for the previous sound to finish
    {
        std::unique_lock<std::mutex> lock(mutex_);
        audio_decode_cv_.wait(lock, [this]() {
            return audio_decode_queue_.empty();
        });
    }
    background_task_->WaitForCompletion();

    // The assets are encoded at 16000Hz, 60ms frame duration
    SetDecodeSampleRate(16000, 60);
    const char* data = sound.data();
    size_t size = sound.size();
    for (const char* p = data; p < data + size; ) {
        auto p3 = (BinaryProtocol3*)p;
        p += sizeof(BinaryProtocol3);

        auto payload_size = ntohs(p3->payload_size);
        std::vector<uint8_t> opus;
        opus.resize(payload_size);
        memcpy(opus.data(), p3->payload, payload_size);
        p += payload_size;

        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.emplace_back(std::move(opus));
    }
}

void Application::ToggleChatState() {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    if (device_state_ == kDeviceStateIdle) {
        Schedule([this]() {
            SetDeviceState(kDeviceStateConnecting);
            if (!protocol_->OpenAudioChannel()) {
                return;
            }

            SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
        });
    } else if (device_state_ == kDeviceStateListening) {
        Schedule([this]() {
            protocol_->CloseAudioChannel();
        });
    }
}


void Application::ChangeChatState() {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    if (device_state_ == kDeviceStateIdle) {
        Schedule([this]() {
            SetDeviceState(kDeviceStateConnecting);
            if (!protocol_->OpenAudioChannel()) {
                return;
            }
            protocol_->SendWakeWordDetected("你好小鱼");
            SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonWakeWordDetected);
            
        });
        
    } else if (device_state_ == kDeviceStateListening) {
        // Schedule([this]() {

        //     //重新监听
        //     ESP_LOGI(TAG, "===>重新监听");

        //     SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
        // });
    }
}

void Application::StartListening() {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }
    
    if (device_state_ == kDeviceStateIdle) {
        Schedule([this]() {
            if (!protocol_->IsAudioChannelOpened()) {
                SetDeviceState(kDeviceStateConnecting);
                if (!protocol_->OpenAudioChannel()) {
                    return;
                }
            }

            SetListeningMode(kListeningModeManualStop);
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
            SetListeningMode(kListeningModeManualStop);
        });
    }
}

void Application::StopListening() {
    const std::array<int, 3> valid_states = {
        kDeviceStateListening,
        kDeviceStateSpeaking,
        kDeviceStateIdle,
    };
    // If not valid, do nothing
    if (std::find(valid_states.begin(), valid_states.end(), device_state_) == valid_states.end()) {
        return;
    }

    Schedule([this]() {
        if (device_state_ == kDeviceStateListening) {
            protocol_->SendStopListening();
            SetDeviceState(kDeviceStateIdle);
        }
    });
}

void Application::Start() {
    // 获取板子实例
    auto& board = Board::GetInstance();
    // 设置设备状态为启动中
    SetDeviceState(kDeviceStateStarting);

    
    // 获取显示实例
    auto display = board.GetDisplay();

   
    // 获取音频编解码器实例
    auto codec = board.GetAudioCodec();
    // 创建opus解码器实例
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(codec->output_sample_rate(), 1, OPUS_FRAME_DURATION_MS);
    // 创建opus编码器实例
    opus_encoder_ = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);
    // 如果实时聊天启用，设置opus编码器复杂度为0
    if (realtime_chat_enabled_) {
        ESP_LOGI(TAG, "Realtime chat enabled, setting opus encoder complexity to 0");
        opus_encoder_->SetComplexity(0);
    // 如果是ml307板子，设置opus编码器复杂度为5
    } else if (board.GetBoardType() == "ml307") {
        ESP_LOGI(TAG, "ML307 board detected, setting opus encoder complexity to 5");
        opus_encoder_->SetComplexity(5);
    // 否则，设置opus编码器复杂度为3
    } else {
        ESP_LOGI(TAG, "WiFi board detected, setting opus encoder complexity to 3");
        opus_encoder_->SetComplexity(3);
    }

    // 如果输入采样率不是16000，配置输入重采样器和参考重采样器
    if (codec->input_sample_rate() != 16000) {
        input_resampler_.Configure(codec->input_sample_rate(), 16000);
        reference_resampler_.Configure(codec->input_sample_rate(), 16000);
    }
    // 启动音频编解码器
    codec->Start();

    // 启动串口监听任务
    xTaskCreatePinnedToCore(
        [](void* param) {
            static_cast<Application*>(param)->UartListenTask();
        },
        "uart_listen_task",  // 任务名称
        8192,                // 栈大小
        this,                // 参数
        4,                   // 优先级
        &uart_listen_task_handle_, // 任务句柄
        1                    // 运行在核心1
    );
    // 创建音频循环任务
    xTaskCreatePinnedToCore([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioLoop();
        vTaskDelete(NULL);
    }, "audio_loop", 4096 * 2, this, 8, &audio_loop_task_handle_, realtime_chat_enabled_ ? 1 : 0);

    
    // 启动网络
    board.StartNetwork();

    
    // 检查新版本
    CheckNewVersion();

    display->SetStatus(Lang::Strings::LOADING_PROTOCOL);
#ifdef CONFIG_CONNECTION_TYPE_WEBSOCKET
    // 如果配置了WEBSOCKET连接类型，则创建一个WebsocketProtocol对象
    protocol_ = std::make_unique<WebsocketProtocol>();
#else
    // 否则创建一个MqttProtocol对象
    protocol_ = std::make_unique<MqttProtocol>();
#endif
    // 注册网络错误回调函数
    protocol_->OnNetworkError([this](const std::string& message) {
        SetDeviceState(kDeviceStateIdle);
        Alert(Lang::Strings::ERROR, message.c_str(), "sad", Lang::Sounds::P3_EXCLAMATION);
    });
    // 注册音频数据回调函数
    protocol_->OnIncomingAudio([this](std::vector<uint8_t>&& data) {
        const int max_packets_in_queue = 300 / OPUS_FRAME_DURATION_MS;
        std::lock_guard<std::mutex> lock(mutex_);
        if (audio_decode_queue_.size() < max_packets_in_queue) {
            audio_decode_queue_.emplace_back(std::move(data));
        }
    });
    // 注册音频通道打开回调函数
    protocol_->OnAudioChannelOpened([this, codec, &board]() {
        board.SetPowerSaveMode(false);
        if (protocol_->server_sample_rate() != codec->output_sample_rate()) {
            ESP_LOGW(TAG, "Server sample rate %d does not match device output sample rate %d, resampling may cause distortion",
                protocol_->server_sample_rate(), codec->output_sample_rate());
        }
        SetDecodeSampleRate(protocol_->server_sample_rate(), protocol_->server_frame_duration());
        auto& thing_manager = iot::ThingManager::GetInstance();
        protocol_->SendIotDescriptors(thing_manager.GetDescriptorsJson());
        std::string states;
        if (thing_manager.GetStatesJson(states, false)) {
            protocol_->SendIotStates(states);
        }
    

    });
    // 注册音频通道关闭回调函数
    protocol_->OnAudioChannelClosed([this, &board]() {
        board.SetPowerSaveMode(true);
        Schedule([this]() {
            auto display = Board::GetInstance().GetDisplay();
            display->SetChatMessage("system", "");
            SetDeviceState(kDeviceStateIdle);
        });
    });
    // 注册JSON数据回调函数
    protocol_->OnIncomingJson([this, display](const cJSON* root) {
        // Parse JSON data
        auto type = cJSON_GetObjectItem(root, "type");
        if (strcmp(type->valuestring, "tts") == 0) {
            auto state = cJSON_GetObjectItem(root, "state");
            if (strcmp(state->valuestring, "start") == 0) {
                Schedule([this]() {
                    aborted_ = false;
                    if (device_state_ == kDeviceStateIdle || device_state_ == kDeviceStateListening) {
                        SetDeviceState(kDeviceStateSpeaking);
                    }
                });
            } else if (strcmp(state->valuestring, "stop") == 0) {
                Schedule([this]() {
                    background_task_->WaitForCompletion();
                    if (device_state_ == kDeviceStateSpeaking) {
                        if (listening_mode_ == kListeningModeManualStop) {
                            SetDeviceState(kDeviceStateIdle);
                        } else {
                            aborted_ = false;
                            ResetDecoder();
                            PlaySound(Lang::Sounds::P3_SUCCESS);
                            vTaskDelay(pdMS_TO_TICKS(300));
                            SetDeviceState(kDeviceStateListening);
                        }
                    }
                });
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                auto text = cJSON_GetObjectItem(root, "text");
                if (text != NULL) {
                    ESP_LOGI(TAG, "<< %s", text->valuestring);
                    Schedule([this, display, message = std::string(text->valuestring)]() {
                        display->SetChatMessage("assistant", message.c_str());
                    });
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {
            auto text = cJSON_GetObjectItem(root, "text");
            if (text != NULL) {
                ESP_LOGI(TAG, ">> %s", text->valuestring);
                Schedule([this, display, message = std::string(text->valuestring)]() {
                    display->SetChatMessage("user", message.c_str());
                });
            }
        } else if (strcmp(type->valuestring, "llm") == 0) {
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (emotion != NULL) {
                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    display->SetEmotion(emotion_str.c_str());
                });
            }
        } else if (strcmp(type->valuestring, "iot") == 0) {
            auto commands = cJSON_GetObjectItem(root, "commands");
            if (commands != NULL) {
                 ESP_LOGI(TAG, "Received IoT commands, count: %d", cJSON_GetArraySize(commands));/////////
                auto& thing_manager = iot::ThingManager::GetInstance();
                for (int i = 0; i < cJSON_GetArraySize(commands); ++i) {
                    auto command = cJSON_GetArrayItem(commands, i);
                    thing_manager.Invoke(command);
                    //ESP_LOGI(TAG, "IoT command %d: %s", i, command_str);//////
                }
            }
        } else if (strcmp(type->valuestring, "system") == 0) {
            auto command = cJSON_GetObjectItem(root, "command");
            if (command != NULL) {
                ESP_LOGI(TAG, "System command: %s", command->valuestring);
                if (strcmp(command->valuestring, "reboot") == 0) {
                    // Do a reboot if user requests a OTA update
                    Schedule([this]() {
                        Reboot();
                    });
                } else {
                    ESP_LOGW(TAG, "Unknown system command: %s", command->valuestring);
                }
            }
        } else if (strcmp(type->valuestring, "alert") == 0) {
            auto status = cJSON_GetObjectItem(root, "status");
            auto message = cJSON_GetObjectItem(root, "message");
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (status != NULL && message != NULL && emotion != NULL) {
                Alert(status->valuestring, message->valuestring, emotion->valuestring, Lang::Sounds::P3_VIBRATION);
            } else {
                ESP_LOGW(TAG, "Alert command requires status, message and emotion");
            }
        }
    });
    protocol_->Start();

#if CONFIG_USE_AUDIO_PROCESSOR
    audio_processor_.Initialize(codec, realtime_chat_enabled_);
    audio_processor_.OnOutput([this](std::vector<int16_t>&& data) {
        background_task_->Schedule([this, data = std::move(data)]() mutable {
            if (protocol_->IsAudioChannelBusy()) {
                return;
            }
            opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
                Schedule([this, opus = std::move(opus)]() {
                    protocol_->SendAudio(opus);
                });
            });
        });
    });
    audio_processor_.OnVadStateChange([this](bool speaking) {
        if (device_state_ == kDeviceStateListening) {
            Schedule([this, speaking]() {
                if (speaking) {
                    voice_detected_ = true;
                } else {
                    voice_detected_ = false;
                }
                auto led = Board::GetInstance().GetLed();
                led->OnStateChanged();
            });
        }
    });
#endif

#if CONFIG_USE_WAKE_WORD_DETECT
    wake_word_detect_.Initialize(codec);
    wake_word_detect_.OnWakeWordDetected([this](const std::string& wake_word) {
        Schedule([this, &wake_word]() {
            if (device_state_ == kDeviceStateIdle) {
                SetDeviceState(kDeviceStateConnecting);
                wake_word_detect_.EncodeWakeWordData();

                if (!protocol_->OpenAudioChannel()) {
                    wake_word_detect_.StartDetection();
                    return;
                }
                
                std::vector<uint8_t> opus;
                // Encode and send the wake word data to the server
                while (wake_word_detect_.GetWakeWordOpus(opus)) {
                    protocol_->SendAudio(opus);
                }
                // Set the chat state to wake word detected
                protocol_->SendWakeWordDetected(wake_word);
                ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
                SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
            } else if (device_state_ == kDeviceStateSpeaking) {
                AbortSpeaking(kAbortReasonWakeWordDetected);
            } else if (device_state_ == kDeviceStateActivating) {
                SetDeviceState(kDeviceStateIdle);
            }
        });
    });
    wake_word_detect_.StartDetection();
#endif

    // Wait for the new version check to finish
    xEventGroupWaitBits(event_group_, CHECK_NEW_VERSION_DONE_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);
    SetDeviceState(kDeviceStateIdle);
    std::string message = std::string(Lang::Strings::VERSION) + ota_.GetCurrentVersion();
    display->ShowNotification(message.c_str());
    display->SetChatMessage("system", "");
    // Play the success sound to indicate the device is ready
    ResetDecoder();
    PlaySound(Lang::Sounds::P3_SUCCESS);
    
    // Enter the main event loop
    MainEventLoop();
}

void Application::OnClockTimer() {
    clock_ticks_++;

    // Print the debug info every 10 seconds
    if (clock_ticks_ % 10 == 0) {
        // SystemInfo::PrintRealTimeStats(pdMS_TO_TICKS(1000));

        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);

        // If we have synchronized server time, set the status to clock "HH:MM" if the device is idle
        if (ota_.HasServerTime()) {
            if (device_state_ == kDeviceStateIdle) {
                Schedule([this]() {
                    // Set status to clock "HH:MM"
                    time_t now = time(NULL);
                    char time_str[64];
                    strftime(time_str, sizeof(time_str), "%H:%M  ", localtime(&now));
                    Board::GetInstance().GetDisplay()->SetStatus(time_str);
                });
            }
        }
    }
}

// Add a async task to MainLoop
void Application::Schedule(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, SCHEDULE_EVENT);
}

// The Main Event Loop controls the chat state and websocket connection
// If other tasks need to access the websocket or chat state,
// they should use Schedule to call this function
void Application::MainEventLoop() {
    while (true) {
        auto bits = xEventGroupWaitBits(event_group_, SCHEDULE_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & SCHEDULE_EVENT) {
            std::unique_lock<std::mutex> lock(mutex_);
            std::list<std::function<void()>> tasks = std::move(main_tasks_);
            lock.unlock();
            for (auto& task : tasks) {
                task();
            }
        }
    }
}

// The Audio Loop is used to input and output audio data
void Application::AudioLoop() {
    auto codec = Board::GetInstance().GetAudioCodec();
    while (true) {
        OnAudioInput();
        if (codec->output_enabled()) {
            OnAudioOutput();
        }
    }
}

void Application::OnAudioOutput() {
    if (busy_decoding_audio_) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto codec = Board::GetInstance().GetAudioCodec();
    const int max_silence_seconds = 10;

    std::unique_lock<std::mutex> lock(mutex_);
    if (audio_decode_queue_.empty()) {
        // Disable the output if there is no audio data for a long time
        if (device_state_ == kDeviceStateIdle) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_output_time_).count();
            if (duration > max_silence_seconds) {
                codec->EnableOutput(false);
            }
        }
        return;
    }

    if (device_state_ == kDeviceStateListening) {
        audio_decode_queue_.clear();
        audio_decode_cv_.notify_all();
        return;
    }

    auto opus = std::move(audio_decode_queue_.front());
    audio_decode_queue_.pop_front();
    lock.unlock();
    audio_decode_cv_.notify_all();

    busy_decoding_audio_ = true;
    background_task_->Schedule([this, codec, opus = std::move(opus)]() mutable {
        busy_decoding_audio_ = false;
        if (aborted_) {
            return;
        }

        std::vector<int16_t> pcm;
        if (!opus_decoder_->Decode(std::move(opus), pcm)) {
            return;
        }
        // Resample if the sample rate is different
        if (opus_decoder_->sample_rate() != codec->output_sample_rate()) {
            int target_size = output_resampler_.GetOutputSamples(pcm.size());
            std::vector<int16_t> resampled(target_size);
            output_resampler_.Process(pcm.data(), pcm.size(), resampled.data());
            pcm = std::move(resampled);
        }
        codec->OutputData(pcm);
        last_output_time_ = std::chrono::steady_clock::now();
    });
}

void Application::OnAudioInput() {
#if CONFIG_USE_WAKE_WORD_DETECT
    if (wake_word_detect_.IsDetectionRunning()) {
        std::vector<int16_t> data;
        int samples = wake_word_detect_.GetFeedSize();
        if (samples > 0) {
            ReadAudio(data, 16000, samples);
            wake_word_detect_.Feed(data);
            return;
        }
    }
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
    if (audio_processor_.IsRunning()) {
        std::vector<int16_t> data;
        int samples = audio_processor_.GetFeedSize();
        if (samples > 0) {
            ReadAudio(data, 16000, samples);
            audio_processor_.Feed(data);
            return;
        }
    }
#else
    if (device_state_ == kDeviceStateListening) {
        std::vector<int16_t> data;
        ReadAudio(data, 16000, 30 * 16000 / 1000);
        background_task_->Schedule([this, data = std::move(data)]() mutable {
            if (protocol_->IsAudioChannelBusy()) {
                return;
            }
            opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
                Schedule([this, opus = std::move(opus)]() {
                    protocol_->SendAudio(opus);
                });
            });
        });
        return;
    }
#endif
    vTaskDelay(pdMS_TO_TICKS(30));
}

void Application::ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples) {
    auto codec = Board::GetInstance().GetAudioCodec();
    if (codec->input_sample_rate() != sample_rate) {
        data.resize(samples * codec->input_sample_rate() / sample_rate);
        if (!codec->InputData(data)) {
            return;
        }
        if (codec->input_channels() == 2) {
            auto mic_channel = std::vector<int16_t>(data.size() / 2);
            auto reference_channel = std::vector<int16_t>(data.size() / 2);
            for (size_t i = 0, j = 0; i < mic_channel.size(); ++i, j += 2) {
                mic_channel[i] = data[j];
                reference_channel[i] = data[j + 1];
            }
            auto resampled_mic = std::vector<int16_t>(input_resampler_.GetOutputSamples(mic_channel.size()));
            auto resampled_reference = std::vector<int16_t>(reference_resampler_.GetOutputSamples(reference_channel.size()));
            input_resampler_.Process(mic_channel.data(), mic_channel.size(), resampled_mic.data());
            reference_resampler_.Process(reference_channel.data(), reference_channel.size(), resampled_reference.data());
            data.resize(resampled_mic.size() + resampled_reference.size());
            for (size_t i = 0, j = 0; i < resampled_mic.size(); ++i, j += 2) {
                data[j] = resampled_mic[i];
                data[j + 1] = resampled_reference[i];
            }
        } else {
            auto resampled = std::vector<int16_t>(input_resampler_.GetOutputSamples(data.size()));
            input_resampler_.Process(data.data(), data.size(), resampled.data());
            data = std::move(resampled);
        }
    } else {
        data.resize(samples);
        if (!codec->InputData(data)) {
            return;
        }
    }
}

void Application::AbortSpeaking(AbortReason reason) {
    ESP_LOGI(TAG, "Abort speaking");
    aborted_ = true;
    protocol_->SendAbortSpeaking(reason);
}

void Application::SetListeningMode(ListeningMode mode) {
    listening_mode_ = mode;
    SetDeviceState(kDeviceStateListening);
}

void Application::SetDeviceState(DeviceState state) {
    if (device_state_ == state) {
        return;
    }
    
    clock_ticks_ = 0;
    auto previous_state = device_state_;
    device_state_ = state;
    ESP_LOGI(TAG, "STATE: %s", STATE_STRINGS[device_state_]);
    // The state is changed, wait for all background tasks to finish
    background_task_->WaitForCompletion();

    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    auto led = board.GetLed();
    led->OnStateChanged();
    switch (state) {
        case kDeviceStateUnknown:
        case kDeviceStateIdle:
            display->SetStatus(Lang::Strings::STANDBY);
            display->SetEmotion("neutral");
#if CONFIG_USE_AUDIO_PROCESSOR
            audio_processor_.Stop();
#endif
#if CONFIG_USE_WAKE_WORD_DETECT
            wake_word_detect_.StartDetection();
#endif
            break;
        case kDeviceStateConnecting:
            display->SetStatus(Lang::Strings::CONNECTING);
            display->SetEmotion("neutral");
            display->SetChatMessage("system", "");
            break;
        case kDeviceStateListening:
            display->SetStatus(Lang::Strings::LISTENING);
            display->SetEmotion("neutral");

            // Update the IoT states before sending the start listening command
            UpdateIotStates();

            // Make sure the audio processor is running
#if CONFIG_USE_AUDIO_PROCESSOR
            if (!audio_processor_.IsRunning()) {
#else
            if (true) {
#endif
                // Send the start listening command
                protocol_->SendStartListening(listening_mode_);
                if (listening_mode_ == kListeningModeAutoStop && previous_state == kDeviceStateSpeaking) {
                    // FIXME: Wait for the speaker to empty the buffer
                    vTaskDelay(pdMS_TO_TICKS(120));
                }
                opus_encoder_->ResetState();
#if CONFIG_USE_WAKE_WORD_DETECT
                wake_word_detect_.StopDetection();
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
                audio_processor_.Start();
#endif
            }
            break;
        case kDeviceStateSpeaking:
            display->SetStatus(Lang::Strings::SPEAKING);

            if (listening_mode_ != kListeningModeRealtime) {
#if CONFIG_USE_AUDIO_PROCESSOR
                audio_processor_.Stop();
#endif
#if CONFIG_USE_WAKE_WORD_DETECT
                wake_word_detect_.StartDetection();
#endif
            }
            ResetDecoder();
            break;
        default:
            // Do nothing
            break;
    }
}

void Application::ResetDecoder() {
    std::lock_guard<std::mutex> lock(mutex_);
    opus_decoder_->ResetState();
    audio_decode_queue_.clear();
    audio_decode_cv_.notify_all();
    last_output_time_ = std::chrono::steady_clock::now();
    
    auto codec = Board::GetInstance().GetAudioCodec();
    codec->EnableOutput(true);
}

void Application::SetDecodeSampleRate(int sample_rate, int frame_duration) {
    if (opus_decoder_->sample_rate() == sample_rate && opus_decoder_->duration_ms() == frame_duration) {
        return;
    }

    opus_decoder_.reset();
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(sample_rate, 1, frame_duration);

    auto codec = Board::GetInstance().GetAudioCodec();
    if (opus_decoder_->sample_rate() != codec->output_sample_rate()) {
        ESP_LOGI(TAG, "Resampling audio from %d to %d", opus_decoder_->sample_rate(), codec->output_sample_rate());
        output_resampler_.Configure(opus_decoder_->sample_rate(), codec->output_sample_rate());
    }
}

void Application::UpdateIotStates() {
    auto& thing_manager = iot::ThingManager::GetInstance();
    std::string states;
    if (thing_manager.GetStatesJson(states, true)) {
        protocol_->SendIotStates(states);
    }
}

void Application::Reboot() {
    ESP_LOGI(TAG, "Rebooting...");
    esp_restart();
}

void Application::WakeWordInvoke(const std::string& wake_word) {
    if (device_state_ == kDeviceStateIdle) {
        ToggleChatState();
        Schedule([this, wake_word]() {
            if (protocol_) {
                protocol_->SendWakeWordDetected(wake_word); 
            }
        }); 
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
        });
    } else if (device_state_ == kDeviceStateListening) {   
        Schedule([this]() {
            if (protocol_) {
                protocol_->CloseAudioChannel();
            }
        });
    }
}



bool Application::CanEnterSleepMode() {
    if (device_state_ != kDeviceStateIdle) {
        return false;
    }

    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        return false;
    }

    // Now it is safe to enter sleep mode
    return true;
}

void Application::UartListenTask() {
  ESP_LOGI(TAG, "UART监听任务已开始运行，任务ID: %p", xTaskGetCurrentTaskHandle());
  ESP_LOGI(TAG, "UART监听配置 - 端口: UART_NUM_2, 缓冲区大小: 1024字节");
  
  // 分配缓冲区
  const int buffer_size = 1024;
  uint8_t* buffer = (uint8_t*)malloc(buffer_size);
  
  if (buffer == nullptr) {
    ESP_LOGE(TAG, "UART监听任务内存分配失败，任务退出");
    vTaskDelete(nullptr);
    return;
  }
  
  ESP_LOGI(TAG, "UART监听任务内存分配成功，开始监听串口数据...");
  
  // 持续监听循环 - 永不停止
  while (true) {
    // 从UART读取数据，100ms超时
    int length = uart_read_bytes(UART_NUM_2, buffer, buffer_size - 1, pdMS_TO_TICKS(30));
    
    if (length > 0) {
      //ESP_LOGI(TAG, "接收到串口数据，长度: %d字节", length);
      
      // 显示原始十六进制数据用于调试
    //   ESP_LOGI(TAG, "原始数据(十六进制):");
    //   for (int i = 0; i < length; i++) {
    //     printf("%02X ", buffer[i]);
    //   }
    //   printf("\n");
      
      // 检查是否是包含JSON数据的协议包
      if (length >= 6 && buffer[0] == 0x55) {
        uint8_t frame_type = buffer[1];      // 帧类别：1=状态帧，2=数据帧
        uint8_t frame_length = buffer[2];    // 帧长度
        
        ESP_LOGI(TAG, "协议帧 - 类型: 0x%02X, 长度: %d", frame_type, frame_length);
        
        // 检查帧长度是否与实际接收长度匹配
        if (frame_length != length) {
          ESP_LOGW(TAG, "帧长度不匹配，声明长度: %d，实际接收: %d", frame_length, length);
          continue;
        }
        
        if (frame_type == 0x01) {
          // 状态帧处理 - 按之前的二进制协议处理
          if (length >= 6) {
            uint8_t event_type = buffer[3];   // 事件类型：0x01连接，0x02断开，0x00心跳
            uint8_t device_type = buffer[4];  // 设备类型：0x01血压计，0x02体温计
            
            ESP_LOGI(TAG, "状态帧 - 事件类型: 0x%02X, 设备类型: 0x%02X", event_type, device_type);
            
            // 跳过心跳数据（事件类型为0x00）
            if (event_type == 0x00) {
              ESP_LOGI(TAG, "收到心跳数据，跳过处理");
              continue;
            }
            
            // 设备名称映射（中文）
            const char* device_name_cn;
            switch (device_type) {
              case 0x01:
                device_name_cn = "血压计";
                break;
              case 0x02:
                device_name_cn = "体温计";
                break;
              case 0x03:
                device_name_cn = "血糖仪";
                break;
              case 0x04:
                device_name_cn = "血氧仪";
                break;
              default:
                device_name_cn = "未知设备";
                break;
            }
            
            // 连接状态映射（中文）
            const char* status_cn;
            switch (event_type) {
              case 0x01:
                status_cn = "蓝牙已连接";
                break;
              case 0x02:
                status_cn = "蓝牙已断开";
                break;
              default:
                status_cn = "状态未知";
                break;
            }
            
            // 构造状态包 JSON（语音播报格式）
            char json_buffer[256];
            snprintf(json_buffer, sizeof(json_buffer), 
                     "{\"type\":\"text2speech\", \"text\":\"%s%s\"}", 
                     device_name_cn, status_cn);
            
            ESP_LOGI(TAG, "状态包JSON: %s", json_buffer);
            
            // 发送状态包
            if (protocol_) {
              protocol_->SendCustomText(json_buffer);
              if (device_state_ == kDeviceStateListening) {
                Schedule([this]() {
                    aborted_ = false;
                    SetDeviceState(kDeviceStateSpeaking);
                });
             }
            }
          }
        } else if (frame_type == 0x02) {
          // 数据帧处理 - 提取JSON字符串
          
          // 查找JSON数据的开始位置（查找第一个'{'）
          int json_start = -1;
          int json_end = -1;
          
          for (int i = 3; i < length; i++) {
            if (buffer[i] == '{') {
              json_start = i;
              break;
            }
          }
          
          // 查找JSON数据的结束位置（查找最后一个'}'）
          for (int i = length - 2; i >= 0; i--) { // 跳过最后的校验字节
            if (buffer[i] == '}') {
              json_end = i;
              break;
            }
          }
          
          if (json_start != -1 && json_end != -1 && json_end > json_start) {
            // 提取JSON字符串
            int json_length = json_end - json_start + 1;
            char* json_string = (char*)malloc(json_length + 1);
            
            if (json_string) {
              memcpy(json_string, &buffer[json_start], json_length);
              json_string[json_length] = '\0';
              
              ESP_LOGI(TAG, "提取的JSON数据: %s", json_string);
              
              // 直接转发JSON数据
              if (protocol_) {
                protocol_->SendCustomText(json_string);
                ESP_LOGI(TAG, "JSON数据已转发");
              }
              
              free(json_string);
            } else {
              ESP_LOGE(TAG, "JSON字符串内存分配失败");
            }
          } else {
            ESP_LOGW(TAG, "未找到有效的JSON数据");
            
            // 如果找不到JSON格式，尝试将整个数据包转换为字符串并转发
            char* fallback_string = (char*)malloc(length + 1);
            if (fallback_string) {
              memcpy(fallback_string, buffer, length);
              fallback_string[length] = '\0';
              
              ESP_LOGI(TAG, "转发原始数据: %s", fallback_string);
              
              if (protocol_) {
                protocol_->SendCustomText(fallback_string);
              }
              
              free(fallback_string);
            }
          }
        } else {
          ESP_LOGW(TAG, "未知帧类型: 0x%02X", frame_type);
        }
      }
    //   else {
    //     ESP_LOGW(TAG, "无效的数据帧格式或帧头不正确");
        
    //     // 如果不是协议格式，尝试直接作为字符串处理
    //     buffer[length] = '\0'; // 添加字符串结束符
    //     char* data_string = (char*)buffer;
        
    //     ESP_LOGI(TAG, "非协议数据，直接转发: %s", data_string);
        
    //     if (protocol_) {
    //       protocol_->SendCustomText(data_string);
    //     }
    //   }
    }
    
    // 任务延迟，避免占用过多CPU资源
    //vTaskDelay(pdMS_TO_TICKS(10));
  }
  
  // 清理资源（实际上不会执行到这里，因为是无限循环）
  free(buffer);
}
