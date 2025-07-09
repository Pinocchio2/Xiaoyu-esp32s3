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
} 

// [函数定义] Application 类的成员函数 Alert。
// [核心职责] 提供一个统一的、全系统的警报或通知接口。它负责在屏幕上显示状态、信息和情感，并能选择性地播放一段关联的声音。
// [参数详解]
//   - const char* status:  一个C风格字符串，用于简短的状态显示（例如在屏幕顶部的状态栏）。
//   - const char* message: 一个C风格字符串，用于更详细的主消息内容（例如在屏幕中央的对话框）。
//   - const char* emotion: 一个C风格字符串，用于控制UI的情感表现（例如一个动态头像的表情）。
//   - const std::string_view& sound: [核心语法点] 一个C++17的 `std::string_view` 引用。
//     - 它是一个“非拥有”的字符串视图，可以高效地接收任何字符串类型（如std::string, const char*, 字符串字面量）而无需发生内存分配和拷贝。
//     - 这是现代C++中用于传递只读字符串参数的最佳实践。
void Application::Alert(const char* status, const char* message, const char* emotion, const std::string_view& sound) {
    // [日志记录] 调用 ESP-IDF 的日志API，以“警告”(Warning)级别记录此次Alert的内容。
    // 使用 `%s` 格式化占位符打印C风格字符串。
    ESP_LOGW(TAG, "Alert %s: %s [%s]", status, message, emotion);
    
    // [硬件抽象] 通过单例模式获取 Board 实例，并从中获取 Display 对象的指针。
    // `auto` 关键字让编译器自动推断 `display` 的类型为 `Display*`。
    //方法链，前一个函数的返回值是一个类，第二个函数必须在这个类的方法里
    auto display = Board::GetInstance().GetDisplay();
    
    // [UI更新] 调用 Display 对象的成员函数，将信息更新到屏幕上。
    display->SetStatus(status);        // 更新状态栏
    display->SetEmotion(emotion);      // 更新情感/头像
    display->SetChatMessage("system", message); // 将主消息作为“系统”消息显示

    // [流程控制] 检查 `sound` 参数是否为空。
    // `!sound.empty()` 是一种清晰且标准的方式来判断一个字符串视图是否包含内容。
    // 这使得播放声音成为一个可选操作，增加了函数的灵活性。
    if (!sound.empty()) {
        // [音频准备] 在播放新声音之前，调用 `ResetDecoder`。
        // 这个函数非常关键，它负责重置音频解码器的状态、清空可能存在的旧音频数据队列，
        // 从而确保即将播放的新声音不会受到之前音频播放的干扰。
        ResetDecoder();
        
        // [音频播放] 调用 `PlaySound` 函数来处理实际的声音播放逻辑。
        PlaySound(sound);
    }
} // 函数结束，警报/通知已在UI上呈现，并可能已开始播放声音。

void Application::DismissAlert() {
    if (device_state_ == kDeviceStateIdle) {
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus(Lang::Strings::STANDBY);
        display->SetEmotion("neutral");
        display->SetChatMessage("system", "");
    }
}

// [函数定义] Application 类的成员函数 PlaySound。
// [核心职责] 接收一段包含预编码音频数据（如提示音）的内存块，将其解析成一个个音频包，并安全地放入待解码队列中等待播放。
// [参数] const std::string_view& sound: [语法点] 一个 `std::string_view` 的常量引用。
//        - `string_view` 是一个轻量级的“视图”，它指向一段已存在的内存数据（此处为嵌入在固件中的音效资源），而不会进行任何拷贝。
//        - 使用它作为参数，可以高效地传递大块的只读数据。
void Application::PlaySound(const std::string_view& sound) {
    // [同步点 - 1] 等待上一个声音播放完毕。
    // “播放完毕”的标志是：1. 音频解码队列为空；2. 处理最后一个音频包的后台任务已结束。
    // "// Wait for the previous sound to finish"
    
    // [语法点] 使用花括号 `{}` 创建一个局部作用域，以精确控制 `unique_lock` 的生命周期。
    {
        // [核心语法点] `std::condition_variable` - 高效的线程等待/通知机制。
        // 1. 创建一个 `std::unique_lock`。必须用 `unique_lock` 因为 `wait` 函数需要有能力对其进行解锁和加锁操作。
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 2. 调用 `wait` 函数。这是整个同步机制的核心。
        //    它会原子性地执行以下操作：
        //    a. 检查第二个参数（一个 Lambda 谓词）：`audio_decode_queue_.empty()` 是否为 `true`。
        //    b. 如果为 `false`（队列非空），则解锁 `mutex_` 并让当前线程进入“休眠”状态，不消耗CPU。
        //    c. 当其他线程（通常是 `OnAudioOutput`）消耗了队列中的数据并调用 `audio_decode_cv_.notify_all()` 时，本线程被唤醒。
        //    d. 唤醒后，本线程会重新获取锁，并再次检查谓词。如果仍为`false`（即“伪唤醒”），则继续休眠。
        //    e. 直到谓词返回 `true`（队列已空），`wait` 函数才会真正返回，程序继续向下执行。
        audio_decode_cv_.wait(lock, [this]() {
            return audio_decode_queue_.empty();
        });
    } // `unique_lock` 在此被销毁，锁被自动释放。

    // [同步点 - 2] 确保最后一个数据包的解码任务也已完成。
    // 即使解码队列为空，最后一个包的解码任务可能仍在后台运行。
    // `WaitForCompletion` 会阻塞，直到后台任务队列完全清空。
    background_task_->WaitForCompletion();

    // [解码器配置]
    // 设置音频解码器，以匹配这些内置音效资源的编码参数（16000Hz 采样率, 60ms 帧长）。
    SetDecodeSampleRate(16000, 60);
    
    // [语法点] 从 `string_view` 获取原始数据指针和大小，准备手动解析。
    const char* data = sound.data();
    size_t size = sound.size();

    // [核心逻辑] - 二进制协议解析循环。
    // 这个 `for` 循环遍历整个 `sound` 数据块，将其中的数据包逐个解析出来。
    // `p` 是一个“当前位置”指针，每次循环后都会向前移动，步长由包的实际大小决定。
    for (const char* p = data; p < data + size; ) {
        // [语法点] `reinterpret_cast` 的一种形式。
        // 将当前内存位置 `p` 的地址，强制“解释”为一个指向 `BinaryProtocol3` 结构体的指针。
        // 这样我们就可以通过 `p3->` 的形式方便地访问协议头部的各个字段。
        auto p3 = (BinaryProtocol3*)p;
        // 将位置指针 `p` 向前移动一个协议头的大小，使其指向负载(payload)的开始位置。
        p += sizeof(BinaryProtocol3);

        // [网络字节序转换]
        // `ntohs` (Network To Host Short) 是一个标准库函数，用于将一个16位的整数从“网络字节序”（大端）转换到“主机字节序”（小端，在ESP32上）。
        // 这是为了确保从二进制流中读取的多字节数据能被正确地解释。
        auto payload_size = ntohs(p3->payload_size);
        
        // 创建一个 vector 用于存放单个 Opus 音频包。
        std::vector<uint8_t> opus;
        opus.resize(payload_size); // 预先分配好足够的空间。
        
        // [内存操作] `memcpy` 是一个C标准库函数，用于高效地进行内存块拷贝。
        // 它将 `p3->payload`（即 `p` 指针当前位置）开始的 `payload_size` 个字节，拷贝到 `opus` 向量的内部缓冲区中。
        memcpy(opus.data(), p3->payload, payload_size);
        // 将位置指针 `p` 向前移动一个负载的大小，完成对一个完整数据包的解析。
        p += payload_size;

        // [线程安全] - 将解析出的音频包放入待解码队列。
        // 使用 `std::lock_guard` 来自动管理互斥锁，确保对共享队列 `audio_decode_queue_` 的访问是线程安全的。
        std::lock_guard<std::mutex> lock(mutex_);
        // [语法点] `std::move`
        // `emplace_back` 会在队列末尾直接构造新元素。
        // 配合 `std::move(opus)`，它将 `opus` 向量的内部数据缓冲区的所有权“转移”给队列中的新元素，
        // 避免了昂贵的内存拷贝，这是现代C++的关键性能优化。
        audio_decode_queue_.emplace_back(std::move(opus));
    }
}

// [函数定义] Application 类的成员函数 ToggleChatState。
// [核心职责] 作为主要的交互触发器，根据设备当前状态，循环地“切换”聊天状态。
//            其行为逻辑可以概括为：空闲 -> 开始聊天；说话 -> 打断；聆听 -> 停止聊天。
void Application::ToggleChatState() {
    // [保护与特殊情况处理] - 这是一个“守卫子句”(Guard Clause)。
    // 如果设备当前正处于“激活中”，则此操作被解释为“取消激活”，并让设备回到空闲状态。
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    // [健壮性检查] - 另一个“守卫子句”。
    // 检查 `protocol_` 协议对象是否已初始化。如果为空，则无法进行任何聊天操作。
    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    // [核心状态机逻辑] - 根据当前设备状态 `device_state_`，执行不同的“切换”操作。
    //
    // [情况一] 如果设备当前处于“空闲”(Idle)状态。
    if (device_state_ == kDeviceStateIdle) {
        // [意图] 开始一个新的聊天会话。
        // [设计模式] 异步执行。将整个启动流程打包成一个 Lambda，交给 `Schedule` 调度器在主事件循环中执行，以避免阻塞当前调用线程。
        Schedule([this]() {
            // 1. 将设备状态设为“连接中”，UI会随之更新。
            SetDeviceState(kDeviceStateConnecting);
            // 2. 尝试打开到服务器的音频通道，这是一个可能耗时的网络操作。如果失败，则中止流程。
            if (!protocol_->OpenAudioChannel()) {
                return;
            }

            // [语法点] 三元运算符 `? :`
            // 这是一个简洁的 if-else 表达式。根据 `realtime_chat_enabled_` 的值，选择不同的聆听模式。
            // 3. 成功连接后，调用 `SetListeningMode`，正式将设备切换到“聆听中”状态。
            SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
        });
    } 
    // [情况二] 如果设备当前正在“说话”(Speaking)。
    else if (device_state_ == kDeviceStateSpeaking) {
        // [意图] 用户想要打断设备的讲话。
        Schedule([this]() {
            // 调用 `AbortSpeaking` 函数立刻中止当前的播放，并通知服务器。
            // 传入 `kAbortReasonNone` 表示这是一次普通的用户打断，而非新的唤醒词等特定原因。
            AbortSpeaking(kAbortReasonNone);
        });
    } 
    // [情况三] 如果设备当前已经在“聆听中”(Listening)。
    else if (device_state_ == kDeviceStateListening) {
        // [意图] 用户想要手动结束当前的聆听会话。
        Schedule([this]() {
            // 调用协议对象的 `CloseAudioChannel` 方法。
            // 注意：这里并不直接调用 `SetDeviceState(kDeviceStateIdle)`。
            // 这是很好的解耦设计：`CloseAudioChannel` 会触发网络层去关闭通道，成功关闭后，
            // 网络层会回调我们之前注册的 `OnAudioChannelClosed` 事件处理器，
            // 由那个回调函数来负责将设备状态最终设置为空闲。
            protocol_->CloseAudioChannel();
        });
    }
}


// [函数定义] Application 类的成员函数 ChangeChatState。
// [核心职责] 根据当前的设备状态，改变或启动一个聊天会话。这通常是由一个外部事件触发的，比如用户按下一个物理按键，
//            或者由非本地的唤醒词（如蓝牙设备上的唤醒）触发。
void Application::ChangeChatState() {
    // [保护与特殊情况处理] - 这是一个“守卫子句”(Guard Clause)。
    // 如果设备当前正处于“激活中”的流程，那么这个函数的行为就不是启动聊天，而是“取消”激活，并让设备回到空闲状态。
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return; // 直接返回，不再执行后续逻辑。
    }

    // [健壮性检查] - 另一个“守卫子句”。
    // `protocol_` 是一个 `std::unique_ptr` 智能指针。`if (!protocol_)` 检查这个指针是否为空(nullptr)。
    // 如果协议对象还没有被成功初始化，那么任何聊天相关的操作都无法进行，因此记录一条错误日志并直接返回，防止程序崩溃。
    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    // [核心状态机逻辑] - 根据当前设备状态 `device_state_`，执行不同的操作。
    //
    // [情况一] 如果设备当前处于“空闲”状态。
    if (device_state_ == kDeviceStateIdle) {
        // [设计模式] 异步执行。
        // 启动一个聊天会话涉及到网络操作（打开音频通道），这可能是耗时的。
        // 为了不阻塞当前调用线程，我们将整个启动流程打包成一个 Lambda，交给 `Schedule` 调度器去主事件循环中执行。
        Schedule([this]() {
            // 1. 立即将设备状态更新为“连接中”，UI会随之改变。
            SetDeviceState(kDeviceStateConnecting);
            
            // 2. 尝试打开到服务器的音频通道。这是一个阻塞式网络操作。
            //    如果失败，则直接返回，设备状态会停留在“连接中”，等待后续处理（如超时或重试）。
            if (!protocol_->OpenAudioChannel()) {
                return;
            }
            
            // 3. 通道打开后，向服务器发送一个“唤醒词检测到”的信令。
            //    这会让服务器知道本次会话是由用户主动发起的。
            protocol_->SendWakeWordDetected("你好小鱼");

            // [语法点] 三元运算符 `? :`
            // 这是一个简洁的 if-else 表达式。根据 `realtime_chat_enabled_` 的值，选择不同的聆听模式。
            // 4. 最后，调用 `SetListeningMode`，正式将设备切换到“聆听中”状态。
            SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
        });
    } 
    // [情况二] 如果设备当前正在“说话”（播放服务器的回复）。
    else if (device_state_ == kDeviceStateSpeaking) {
        // 这种情况下，调用此函数意味着用户想要“打断”设备。
        Schedule([this]() {
            // 调用 `AbortSpeaking` 函数，并传入打断原因为“检测到唤醒词”。
            // `AbortSpeaking` 会停止当前的音频播放，并通知服务器。
            AbortSpeaking(kAbortReasonWakeWordDetected);
            
        });
        
    } 
    // [情况三] 如果设备当前已经在“聆听中”。
    else if (device_state_ == kDeviceStateListening) {
        // [代码意图分析] - 这部分代码被完全注释掉了。
        // 这表明开发者曾经考虑过或正在开发一种逻辑：当设备已经在聆听时，再次调用此函数可能会触发“重新监听”
        // (比如重置静音检测的计时器)。但目前，这个逻辑是**不活动**的。
        // 因此，在当前版本中，如果设备正在聆听，调用此函数将不执行任何操作。
        // Schedule([this]() {

        //     //重新监听
        //     ESP_LOGI(TAG, "===>重新监听");

        //     SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
        // });
    }
}

// [函数定义] Application 类的成员函数 StartListening。
// [核心职责] 强制启动一个“聆听”会话。这通常是为物理按键或手机App上的“按住说话”功能设计的接口。
//            它会根据设备当前的状态，执行不同的逻辑来进入聆听模式。
void Application::StartListening() {
    // [保护与特殊情况处理] - 这是一个“守卫子句”(Guard Clause)。
    // 如果设备当前正处于“激活中”的流程，那么此操作的意图被解释为“取消激活”，并让设备回到空闲状态。
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    // [健壮性检查] - 另一个“守卫子句”。
    // 检查 `protocol_` 协议对象是否已初始化。如果为空，则无法进行任何需要网络的操作。
    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }
    
    // [核心状态机逻辑] - 根据设备当前状态，决定如何启动聆听。
    //
    // [情况一] 如果设备当前处于“空闲”状态。
    if (device_state_ == kDeviceStateIdle) {
        // [设计模式] 异步执行。
        // 将启动聆听的整个流程打包成一个 Lambda，交给 `Schedule` 调度器去主事件循环中执行，以避免阻塞当前调用线程。
        Schedule([this]() {
            // 首先检查与服务器的音频通道是否已经打开。
            if (!protocol_->IsAudioChannelOpened()) {
                // 如果通道未打开，则执行完整的连接流程。
                // 1. 将设备状态设为“连接中”，UI会随之更新。
                SetDeviceState(kDeviceStateConnecting);
                // 2. 尝试打开音频通道。如果失败，则直接返回，放弃本次操作。
                if (!protocol_->OpenAudioChannel()) {
                    return;
                }
            }

            // [核心动作] 无论通道是原本就打开的，还是刚刚打开的，最终都调用 `SetListeningMode`。
            // 传入 `kListeningModeManualStop`，这个模式意味着设备会一直聆听，直到被明确地调用 `StopListening()` 为止，
            // 它不会因为用户不说话而自动停止。这非常适合“按住说话”的场景。
            SetListeningMode(kListeningModeManualStop);
        });
    } 
    // [情况二] 如果设备当前正在“说话”（播放服务器的回复）。
    else if (device_state_ == kDeviceStateSpeaking) {
        // 在这种情况下，调用 `StartListening` 意味着用户想要“打断”设备的讲话，并立即开始自己的输入。
        Schedule([this]() {
            // 1. 调用 `AbortSpeaking` 来立刻中止当前的播放，并通知服务器。
            //    传入 `kAbortReasonNone` 表示这是一次普通的用户打断，而非新的唤醒词触发。
            AbortSpeaking(kAbortReasonNone);
            // 2. 在中止播放后，立即将设备切换到“手动停止”的聆听模式。
            SetListeningMode(kListeningModeManualStop);
        });
    }
    // [注意] 如果设备当前已经在“聆听中”(kDeviceStateListening)，这个函数将不执行任何操作。
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
    // [函数开始] - 这是应用程序核心逻辑的入口点。
    
    // [初始化阶段] - 获取核心硬件对象的句柄/引用。
    // 获取板子实例
    // [语法点] `auto&` (带引用的类型推导)
    // `auto` 让编译器自动推断 `board` 的类型为 `Board`。
    // `&` 表示 `board` 是一个“引用”，它相当于 `Board::GetInstance()` 返回的那个全局唯一对象的“别名”。
    // 使用引用可以避免不必要的对象拷贝，既高效又可以直接操作原对象。
    auto& board = Board::GetInstance();
    // 设置设备初始状态为“启动中”，这是状态机的第一个明确状态。
    SetDeviceState(kDeviceStateStarting);

    
    // 获取显示实例
    // [语法点] `auto` - 自动类型推导。
    // 编译器会根据 `board.GetDisplay()` 的返回值（很可能是 `Display*`）来确定 `display` 变量的类型。
    auto display = board.GetDisplay();

    
    // 获取音频编解码器实例
    auto codec = board.GetAudioCodec();
    // 创建opus解码器实例
    // [语法点] `std::make_unique` (现代C++资源管理)
    // 这是 C++14 引入的、创建 `std::unique_ptr` 智能指针的首选方式。
    // `unique_ptr` 独占式地拥有一个动态分配的对象。当 `unique_ptr` (`opus_decoder_`) 被销毁时，
    // 它会自动调用 `delete` 释放其管理的对象，从而彻底避免内存泄漏。
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(codec->output_sample_rate(), 1, OPUS_FRAME_DURATION_MS);
    // 创建opus编码器实例
    opus_encoder_ = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);
    // 如果实时聊天启用，设置opus编码器复杂度为0
    // [运行时配置] - 根据系统配置和硬件类型，动态调整Opus编码器的复杂度，以平衡CPU性能和功耗。
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
    // [音频处理] - 确保音频数据在进入编码器前，其采样率是标准化的16KHz。
    if (codec->input_sample_rate() != 16000) {
        input_resampler_.Configure(codec->input_sample_rate(), 16000);
        reference_resampler_.Configure(codec->input_sample_rate(), 16000);
    }
    // 启动音频编解码器
    codec->Start();

    // [并发任务创建] - 从这里开始，应用程序将从单线程转为多线程执行。
    // 启动串口监听任务
    // [FreeRTOS API] `xTaskCreatePinnedToCore` 用于创建一个新任务（线程），并可以将其“钉”在某个特定的CPU核心上。
    xTaskCreatePinnedToCore(
        // 参数1: 任务函数 - 一个 `void (*)(void*)` 的函数指针。我们再次使用 Lambda `[]` 表达式来适配它。
        [](void* param) {
            static_cast<Application*>(param)->UartListenTask();
        },
        // 参数2: 任务名 - "uart_listen_task" (主要用于调试)。
        "uart_listen_task",  // 任务名称
        // 参数3: 栈大小 - 8192 字节，为这个任务分配的内存空间。
        8192,                // 栈大小
        // 参数4: 任务参数 - 将 `this` 指针传入，这样任务函数内部就知道要操作哪个 `Application` 实例。
        this,                // 参数
        // 参数5: 任务优先级 - 4 (数字越大，优先级通常越高)。
        4,                   // 优先级
        // 参数6: 任务句柄 - `&uart_listen_task_handle_`，一个可选的指针，用于保存任务句柄，以便将来控制该任务。
        &uart_listen_task_handle_, // 任务句柄
        // 参数7: 固定的核心ID - 1，表示这个任务将只在 CPU 核心1上运行。
        1                    // 运行在核心1
    );
    // 创建音频循环任务
    xTaskCreatePinnedToCore([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioLoop();
        // [FreeRTOS API] 如果任务函数不是一个无限循环，它在执行完毕后必须调用 `vTaskDelete(NULL)` 来安全地删除自身。
        vTaskDelete(NULL);
    }, "audio_loop", 4096 * 2, this, 8, &audio_loop_task_handle_, realtime_chat_enabled_ ? 1 : 0);

    
    // [同步阻塞式初始化] - 必须按顺序完成的重要步骤。
    // 启动网络
    board.StartNetwork();

    
    // 检查新版本
    // 调用我们之前分析过的、复杂的版本检查、OTA和激活流程。
    // 注意：`Start` 函数会在这里“卡住”，直到该流程完成并返回。
    CheckNewVersion();

    // [核心业务逻辑 - 事件驱动架构设置]
    // 初始化完成，现在开始设置当各种网络事件发生时，应该如何响应。
    display->SetStatus(Lang::Strings::LOADING_PROTOCOL);
// [语法点] 编译时条件 `#ifdef`
// 根据项目配置(KConfig)，选择性地编译代码块，以决定是使用 WebSocket 还是 MQTT 协议。
#ifdef CONFIG_CONNECTION_TYPE_WEBSOCKET
    // 如果配置了WEBSOCKET连接类型，则创建一个WebsocketProtocol对象
    protocol_ = std::make_unique<WebsocketProtocol>();
#else
    // 否则创建一个MqttProtocol对象
    protocol_ = std::make_unique<MqttProtocol>();
#endif
    // [核心设计] 注册一系列回调函数 (Callback)。
    // `Application` 类在这里扮演了“事件协调中心”的角色，它告诉 `protocol_` 对象：
    // “当某个事件发生时，请执行我为你准备的这段代码（Lambda）”。
    
    // 注册网络错误回调函数
    // [语法点] Lambda 捕获 `[this]`
    // 捕获 `this` 指针，使得 Lambda 内部可以调用 `SetDeviceState` 和 `Alert` 等成员函数。
    protocol_->OnNetworkError([this](const std::string& message) {
        SetDeviceState(kDeviceStateIdle);
        Alert(Lang::Strings::ERROR, message.c_str(), "sad", Lang::Sounds::P3_EXCLAMATION);
    });
    // 注册音频数据回调函数
    // [语法点] 右值引用 `&&` 与移动语义 (Move Semantics)
    // `std::vector<uint8_t>&& data` 表示这个 Lambda 接受一个“即将被销毁”的 `vector`。
    // 这允许网络层通过 `std::move` 将数据的所有权“转移”过来，而不是“拷贝”，极大地提升了处理大块数据的性能。
    protocol_->OnIncomingAudio([this](std::vector<uint8_t>&& data) {
        const int max_packets_in_queue = 300 / OPUS_FRAME_DURATION_MS;
        std::lock_guard<std::mutex> lock(mutex_);
        if (audio_decode_queue_.size() < max_packets_in_queue) {
            audio_decode_queue_.emplace_back(std::move(data)); // `std::move` 继续所有权转移链条，高效地将数据放入队列。
        }
    });
    // 注册音频通道打开回调函数
    // [语法点] 复杂的 Lambda 捕获列表
    // `[this, codec, &board]` 捕获了三个外部变量：
    // - `this`: Application 对象的指针，按值捕获（因为指针本身很小）。
    // - `codec`: AudioCodec 对象的指针，按值捕获。
    // - `&board`: Board 对象的引用，按引用捕获，确保访问的是原始的 board 对象。
    protocol_->OnAudioChannelOpened([this, codec, &board]() {
        // 设置电源省电模式为false
        board.SetPowerSaveMode(false);
        // 如果服务器采样率与设备输出采样率不匹配，则输出警告信息
        if (protocol_->server_sample_rate() != codec->output_sample_rate()) {
            ESP_LOGW(TAG, "Server sample rate %d does not match device output sample rate %d, resampling may cause distortion",
                protocol_->server_sample_rate(), codec->output_sample_rate());
        }
        // 设置解码采样率
        SetDecodeSampleRate(protocol_->server_sample_rate(), protocol_->server_frame_duration());
        // 获取ThingManager实例
        auto& thing_manager = iot::ThingManager::GetInstance();
        // 发送Iot描述符
        protocol_->SendIotDescriptors(thing_manager.GetDescriptorsJson());
        std::string states;
        // 获取Iot状态
        if (thing_manager.GetStatesJson(states, false)) {
            // 发送Iot状态
            protocol_->SendIotStates(states);
        }
    

    });
    // 注册音频通道关闭回调函数
    // 当音频通道关闭时，设置电源保存模式为true
    protocol_->OnAudioChannelClosed([this, &board]() {
        board.SetPowerSaveMode(true);
        // 在一段时间后执行以下操作
        Schedule([this]() {
            // 获取显示对象
            auto display = Board::GetInstance().GetDisplay();
            // 设置聊天消息为空
            display->SetChatMessage("system", "");
            // 设置设备状态为空闲
            SetDeviceState(kDeviceStateIdle);
        });
    });
    // 注册JSON数据回调函数
    // [语法点] C++14 初始化捕获 (Init Capture)
    // `message = std::string(text->valuestring)`
    // 这种捕获方式会在 Lambda 创建时，当场创建一个名为 `message` 的新变量，并用 `text->valuestring` 初始化它。
    // 这对于将一个生命周期可能很短的数据（如cJSON库解析出的临时字符串）安全地复制到需要异步执行的Lambda中，是最佳实践。
    // [核心设计] 注册“收到JSON数据”事件的回调。
    // 这是整个应用与服务器进行信息交互的核心。服务器发送的各种指令和数据都在这里被解析和分发。
    // [语法点] Lambda 捕获 `[this, display]`
    // - `this`: 捕获 `Application` 对象的指针，以便在 Lambda 内部调用 `Schedule`, `Alert`, `SetDeviceState` 等成员函数。
    // - `display`: 捕获 `display` 指针，以便直接更新 UI，避免重复调用 `Board::GetInstance().GetDisplay()`。
    protocol_->OnIncomingJson([this, display](const cJSON* root) {
        // [JSON 解析] - 使用 cJSON 库来解析服务器发来的数据。
        // 第一步：从JSON的根(root)对象中，获取键(key)为 "type" 的项。
        // "type" 字段决定了这个JSON消息的意图，是后续所有处理的分发依据。
        auto type = cJSON_GetObjectItem(root, "type");
        // [健壮性检查] 在实际应用中，这里应该检查 `type` 是否为 NULL，以及 `type->valuestring` 是否有效，以防JSON格式错误导致崩溃。

        // [分支逻辑] - 根据消息类型(type)执行不同的操作。
        // [C函数] `strcmp` 是C语言中用于比较两个字符串是否相等的函数。
        if (strcmp(type->valuestring, "tts") == 0) {
            // [处理TTS流] - Text-to-Speech，表示服务器正在发来需要合成语音的文本流。
            auto state = cJSON_GetObjectItem(root, "state");
            if (strcmp(state->valuestring, "start") == 0) {
                // TTS流开始。将这个操作调度到主循环，以保证线程安全。
                Schedule([this]() {
                    aborted_ = false; // 清除之前可能存在的“中止播放”标志。
                    // 只有在空闲或聆听状态下，才切换到“说话中”状态。
                    if (device_state_ == kDeviceStateIdle || device_state_ == kDeviceStateListening) {
                        SetDeviceState(kDeviceStateSpeaking);
                    }
                });
            } else if (strcmp(state->valuestring, "stop") == 0) {
                // TTS流结束。
                Schedule([this]() {
                    // 等待后台所有任务（主要是音频解码）完成，确保当前句子播放完毕。
                    background_task_->WaitForCompletion();
                    if (device_state_ == kDeviceStateSpeaking) {
                        // 根据当前的聆听模式，决定说完话后是回到空闲，还是继续聆听。
                        if (listening_mode_ == kListeningModeManualStop) {
                            SetDeviceState(kDeviceStateIdle);
                        } else {
                            aborted_ = false;
                            ResetDecoder(); // 重置解码器，准备播放下一个声音。
                            
                            // [链接器符号声明] - 这一行是连接 C++ 代码和编译工具链的桥梁。
                            // [关键字] extern: 告诉C++编译器，“p3_success_start”这个变量的实体是在别处定义的（在此场景下，它是在链接阶段由链接器定义的），
                            //                 你在这里只需要知道它的存在和类型即可，不要为它分配内存。
                            // [类型] const char p3_success_start[]: 在C++层面，我们将这个外部符号声明为一个“只读的char数组”。在实际使用中，它可以被当作一个指向数据起始地址的指针。
                            // [GCC/Clang扩展] asm("_binary_success_p3_start"): 这是本行代码的“魔法”所在，是GCC/Clang编译器的一个扩展功能。
                            //                      它的作用是为C++变量 `p3_success_start` 指定一个“汇编符号名”。
                            //                      `_binary_success_p3_start` 是由构建系统（如ESP-IDF的构建工具）在将一个二进制文件（比如 `success_p3.bin`）链接进固件时自动生成的符号，
                            //                      它精确地指向该二进制数据在内存中的起始地址。
                            // [总结]: 这一整行的意思是：“声明一个名为 p3_success_start 的C++变量，并让它在链接时与符号 _binary_success_p3_start 绑定，从而在代码中获得嵌入式二进制数据的起始地址。”
                            //extern const char p3_success_start[] asm("_binary_success_p3_start");

                            // [链接器符号声明] - 同上，这一行声明了指向嵌入式二进制数据“结束地址（末尾字节的下一个地址）”的变量。
                            // 它将C++变量 `p3_success_end` 与链接器生成的 `_binary_success_p3_end` 符号进行绑定。
                            //extern const char p3_success_end[] asm("_binary_success_p3_end");

                            // [常量定义] - 利用上面获取的地址信息，创建一个高效的、只读的数据视图。
                            // [关键字] static const: 定义一个静态常量。`static` 意味着 `P3_SUCCESS` 变量只在当前编译单元（.cpp文件）中可见。`const` 意味着它一旦初始化后就不可更改。
                            // [类型] std::string_view: 这是C++17引入的强大类型。它本身不持有任何数据副本，仅仅是一个“视图”，内部只包含一个指向数据开头的指针和一个长度值。
                            //                         这使得它在引用大块只读数据（如嵌入式资源）时，既轻量又高效，完全没有动态内存分配和数据拷贝的开odes。
                            //static const std::string_view P3_SUCCESS {
                            // [构造参数1: 数据起始地址]
                            // [语法点] static_cast: 这是C++的编译时类型转换。它将 `p3_success_start` (一个 `const char` 数组，可隐式转换为指针)
                            //                      安全地、显式地转换为 `const char*` 指针类型，这正是 `std::string_view` 构造函数需要的第一个参数。
                            // static_cast<const char*>(p3_success_start),
                            // [构造参数2: 数据长度]
                            // [语法点] 指针算术 (Pointer Arithmetic): 在C/C++中，两个指向同一内存块的指针相减，其结果是两个指针之间相隔的元素数量。
                            //                          因为这里的指针类型是 `char` (大小为1字节)，所以 `p3_success_end - p3_success_start` 的结果不多不少，
                            //                          正好是嵌入式二进制数据的总字节数，也就是它的长度。
                            // [语法点] static_cast: 指针相减的结果类型通常是 `ptrdiff_t`，这里通过 `static_cast` 将其安全地转换为 `size_t` 类型，
                            //                      `size_t` 是C++中用于表示大小和计数的标准无符号整数类型，也正是 `std::string_view` 构造函数需要的长度参数类型。
                            //static_cast<size_t>(p3_success_end - p3_success_start)
                            // `P3_SUCCESS` 初始化完成。现在它是一个指向固件内“成功”音效原始数据的、零拷贝的、只读的视图。



                            PlaySound(Lang::Sounds::P3_SUCCESS); // 播放一个“成功”的提示音。
                            vTaskDelay(pdMS_TO_TICKS(300)); // 短暂延时，让提示音有时间播放。
                            SetDeviceState(kDeviceStateListening); // 自动进入聆听状态。
                        }
                    }
                });
                //strcmp 比较两个 C 风格的字符串是否相等 ,如果两个字符串完全相等，函数返回整数 0。
                //如果第一个字符串按字典顺序大于第二个字符串，函数返回一个正数。
                //如果第一个字符串按字典顺序小于第二个字符串，函数返回一个负数。

            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                // TTS流中的一个新句子开始。
                auto text = cJSON_GetObjectItem(root, "text");
                if (text != NULL) {
                    ESP_LOGI(TAG, "<< %s", text->valuestring); // 打印接收到的句子内容。
                    // [语法点] C++14 初始化捕获 (Init Capture)
                    // `message = std::string(text->valuestring)` 在Lambda创建时，立即将C风格字符串复制一份并存入名为`message`的`std::string`对象中。
                    // 这可以安全地将可能被cJSON库释放的临时数据传递给异步执行的 `Schedule` 任务。
                    Schedule([this, display, message = std::string(text->valuestring)]() {
                        display->SetChatMessage("assistant", message.c_str());
                    });
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {
            // [处理STT结果] - Speech-to-Text，服务器发来的语音识别结果（用户说的话）。
            auto text = cJSON_GetObjectItem(root, "text");
            if (text != NULL) {
                ESP_LOGI(TAG, ">> %s", text->valuestring); // 打印识别出的用户语句。
                Schedule([this, display, message = std::string(text->valuestring)]() {
                    display->SetChatMessage("user", message.c_str()); // 在UI上显示用户说的话。
                });
            }
        } else if (strcmp(type->valuestring, "llm") == 0) {
            // [处理LLM特定信息] - Large Language Model，大语言模型相关的附加信息。
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (emotion != NULL) {
                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    display->SetEmotion(emotion_str.c_str()); // 根据服务器指令，更新UI的情感状态。
                });
            }
        } else if (strcmp(type->valuestring, "iot") == 0) {
            // [处理IoT命令] - Internet of Things，物联网控制指令。
            auto commands = cJSON_GetObjectItem(root, "commands");
            if (commands != NULL) {
                ESP_LOGI(TAG, "Received IoT commands, count: %d", cJSON_GetArraySize(commands));/////////
                // 获取IoT设备管理器单例。
                auto& thing_manager = iot::ThingManager::GetInstance();
                // 遍历JSON数组中的每一条命令。
                for (int i = 0; i < cJSON_GetArraySize(commands); ++i) {
                    auto command = cJSON_GetArrayItem(commands, i);
                    // 调用 `thing_manager` 来执行具体的IoT命令。
                    thing_manager.Invoke(command);
                    //ESP_LOGI(TAG, "IoT command %d: %s", i, command_str);//////
                }
            }
        } else if (strcmp(type->valuestring, "system") == 0) {
            // [处理系统命令] - 来自服务器的系统级指令。
            auto command = cJSON_GetObjectItem(root, "command");
            if (command != NULL) {
                ESP_LOGI(TAG, "System command: %s", command->valuestring);
                if (strcmp(command->valuestring, "reboot") == 0) {
                    // 如果是重启指令，则调度一个重启任务。
                    Schedule([this]() {
                        Reboot();
                    });
                } else {
                    ESP_LOGW(TAG, "Unknown system command: %s", command->valuestring);
                }
            }
        } else if (strcmp(type->valuestring, "alert") == 0) {
            // [处理服务器推送的警报] - 由服务器主动发起的通知。
            auto status = cJSON_GetObjectItem(root, "status");
            auto message = cJSON_GetObjectItem(root, "message");
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (status != NULL && message != NULL && emotion != NULL) {
                // 调用本地的 `Alert` 方法来显示这个通知。
                Alert(status->valuestring, message->valuestring, emotion->valuestring, Lang::Sounds::P3_VIBRATION);
            } else {
                ESP_LOGW(TAG, "Alert command requires status, message and emotion");
            }
        }
    });
    // 所有回调都注册好后，启动协议栈（例如，连接服务器）。
    protocol_->Start();

// [可选模块初始化] - 同样使用条件编译和回调模式进行初始化。
// [语法点] 条件编译指令 `#if`
// 下面的整个代码块，只有在项目的构建配置中定义了 `CONFIG_USE_AUDIO_PROCESSOR` 宏（通常是通过 menuconfig 工具启用）的情况下，
// 才会被编译器编译。这是一种在不修改代码的情况下，为固件添加或移除可选功能的常用方法。
#if CONFIG_USE_AUDIO_PROCESSOR
    // [初始化] - 对音频处理器对象进行初始化。
    // 这通常会传入它所需要的依赖，如此处的 `codec` 硬件句柄和 `realtime_chat_enabled_` 配置标志。
    audio_processor_.Initialize(codec, realtime_chat_enabled_);
    
    // [核心逻辑] - 注册“OnOutput”事件回调。
    // [设计模式] 事件驱动。我们告诉 `audio_processor_`：“当你处理完一段干净的音频(PCM)数据后，请调用我给你的这个Lambda函数”。
    // [语法点] `&&` 右值引用与移动语义。`data` 参数接收的是一个右值引用，允许 `audio_processor_` 高效地将数据所有权“转移”过来，避免拷贝。
    audio_processor_.OnOutput([this](std::vector<int16_t>&& data) {
        
        // [异步处理阶段1] - 将编码任务调度到后台线程。
        // 音频处理回调（`OnOutput`）通常在实时性要求高的线程中，为了不阻塞它，我们将CPU密集型的编码工作交给后台任务。
        // [语法点] C++14 初始化捕获与移动。`data = std::move(data)` 将收到的PCM数据的所有权再次“移动”到后台任务的Lambda中。
        background_task_->Schedule([this, data = std::move(data)]() mutable {
            // [语法点] `mutable` 关键字。因为这个Lambda内部需要再次 `std::move(data)`，这会“修改”Lambda自身捕获的 `data` 成员，
            // 所以必须用 `mutable` 来移除Lambda默认的 `const` 属性。
            
            // 如果网络协议繁忙（例如，上一个包还没发出去），则直接丢弃这个数据包，避免数据积压。
            if (protocol_->IsAudioChannelBusy()) {
                return;
            }
            
            // [异步处理阶段2] - 调用Opus编码器，并为其注册完成回调。
            // `Encode` 函数本身也是异步的。它接收PCM数据，并承诺在编码出 Opus 数据包后，调用它自己的回调。
            opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
                
                // [异步处理阶段3] - 将网络发送任务调度到主事件循环。
                // 这是流水线的最后一站。当收到编码好的 Opus 包后，我们创建一个最终的发送任务。
                Schedule([this, opus = std::move(opus)]() {
                    // [最终动作] 调用 `protocol_` 对象，将编码好的 Opus 数据包通过网络发送出去。
                    protocol_->SendAudio(opus);
                });
            });
        });
    });
    
    // [核心逻辑] - 注册“OnVadStateChange”事件回调。
    // VAD(Voice Activity Detection) - 语音活动检测。
    // 我们告诉 `audio_processor_`：“当你检测到用户开始说话或停止说话时，请调用我给你的这个Lambda函数”。
    audio_processor_.OnVadStateChange([this](bool speaking) {
        // 只在设备处于“聆听中”状态时，才关心VAD状态的变化。
        if (device_state_ == kDeviceStateListening) {
            // 将UI更新（如改变LED状态）调度到主事件循环，以保证线程安全。
            Schedule([this, speaking]() {
                // 根据VAD的结果，更新内部的 `voice_detected_` 状态标志。
                if (speaking) {
                    voice_detected_ = true;
                } else {
                    voice_detected_ = false;
                }
                // 通知LED模块状态已改变，LED可以根据 `voice_detected_` 的新值来更新显示效果
                // （例如，用户说话时LED点亮，不说话时熄灭）。
                auto led = Board::GetInstance().GetLed();
                led->OnStateChanged();
            });
        }
    });
#endif // 结束条件编译块

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

    // [最终阶段] - 等待初始化完成并进入主循环。
    
    // Wait for the new version check to finish
    // [FreeRTOS API] 阻塞当前任务（`Start`函数所在的任务），无限期等待，
    // 直到 `CheckNewVersion` 流程中的某处调用 `xEventGroupSetBits` 设置了 `CHECK_NEW_VERSION_DONE_EVENT` 位。
    xEventGroupWaitBits(event_group_, CHECK_NEW_VERSION_DONE_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);
    // 初始化流程全部结束，设备进入正常的空闲状态。
    SetDeviceState(kDeviceStateIdle);
    std::string message = std::string(Lang::Strings::VERSION) + ota_.GetCurrentVersion();
    display->ShowNotification(message.c_str());
    display->SetChatMessage("system", "");
    // Play the success sound to indicate the device is ready
    ResetDecoder();
    PlaySound(Lang::Sounds::P3_SUCCESS);
    
    // Enter the main event loop
    // [程序流程] 进入主事件循环。
    // `Start` 函数的使命到此结束，它将控制权完全交给 `MainEventLoop`。
    // `MainEventLoop` 是一个无限循环，从此以后，应用程序的所有行为都将由事件和回调驱动。
    MainEventLoop();
}

// [函数定义] Application 类的成员函数 OnClockTimer。
// [核心职责] 这个函数是我们在构造函数中创建的那个1秒定时器 (`esp_timer`) 的回调函数。
//            因此，它会大约每秒被系统自动调用一次，用于执行周期性的检查和更新任务。
void Application::OnClockTimer() {
    // 成员变量 `clock_ticks_` 在这里作为一个简单的秒计数器。
    clock_ticks_++;

    // [流程控制] 使用取模运算符 `%` 来实现一个低频度的周期性任务。
    // 这个 `if` 块内的代码，只会在 `clock_ticks_` 是10的倍数时才执行，也就是大约每10秒执行一次。
    if (clock_ticks_ % 10 == 0) {
        // 这行被注释掉的代码，很可能是用于打印更详细的 FreeRTOS 实时状态（如各任务的CPU占用率、栈剩余空间等），
        // 是一个非常有用的性能调试工具。
        // SystemInfo::PrintRealTimeStats(pdMS_TO_TICKS(1000));

        // [系统监控] - 检查并打印内存使用情况，这是嵌入式系统健康监控的关键部分。
        // [ESP-IDF API] `heap_caps_get_free_size` 获取当前可用的内部SRAM的字节数。
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        // [ESP-IDF API] `heap_caps_get_minimum_free_size` 获取自设备启动以来，系统可用内存的“最低水位线”。
        // 这个值对于判断系统是否存在潜在的内存泄漏或内存不足风险至关重要。
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        // [ESP-IDF API] 使用 `ESP_LOGI` 将内存信息以“信息”(Info)级别打印出来，方便开发者监控。
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);

        // [UI更新逻辑] - 只有在满足特定条件时，才更新屏幕显示时间。
        // 条件1: 设备必须已经通过网络同步到了准确的时间。
        if (ota_.HasServerTime()) {
            // 条件2: 设备当前必须处于“空闲”状态。
            // 这样做是为了避免用时钟显示覆盖掉更重要的状态信息，比如“连接中”或“正在说话”。
            if (device_state_ == kDeviceStateIdle) {
                // [设计模式] 异步UI更新。
                // 即使这个回调函数本身已经在一个独立的任务中执行，为了保证所有UI操作都在
                // 唯一的主事件循环中串行执行，这里依然使用 `Schedule` 提交一个UI更新任务。
                // 这是最安全、最稳健的做法。
                Schedule([this]() {
                    // [C标准库] - 时间处理
                    // `time(NULL)` 获取当前的Unix时间戳（自1970年1月1日以来的秒数）。
                    time_t now = time(NULL);
                    // 在栈上分配一个缓冲区，用于存放格式化后的时间字符串。
                    char time_str[64];
                    // `strftime` (string format time) 是一个强大的时间格式化函数。
                    // - time_str, sizeof(time_str): 目标缓冲区及其大小。
                    // - "%H:%M  ": 格式化指令。%H 代表24小时制的小时，%M 代表分钟。后面的空格可能是为了UI对齐。
                    // - localtime(&now): 将 `time_t` 时间戳转换为一个包含本地时区信息的、分解开的 `tm` 结构体。
                    strftime(time_str, sizeof(time_str), "%H:%M  ", localtime(&now));
                    // [链式调用] 获取显示对象，并调用其 `SetStatus` 方法来更新屏幕上的状态栏。
                    Board::GetInstance().GetDisplay()->SetStatus(time_str);
                });
            }
        }
    }
}

// Add a async task to MainLoop// [函数定义] Application 类的成员函数 Schedule。
// [核心职责] 提供一个线程安全的接口，允许应用程序的任何部分（任何线程）将一个“任务”提交到主事件循环中，以便将来执行。
// [参数详解]
//   - `std::function<void()> callback`: [核心语法点] 这是一个 `std::function` 对象，是C++中功能强大的“可调用对象包装器”。
//     - 它可以“包裹”任何可以像函数一样被调用的东西，比如普通函数、静态函数，以及我们最常用的 Lambda 表达式。
//     - `<void()>` 表示它包裹的是一个“无参数、无返回值”的函数。
//     - `callback` 这个参数，就是我们想要异步执行的那个“任务包”。
void Application::Schedule(std::function<void()> callback) {
    // [语法点] 作用域块 `{...}`
    // 这里使用一对额外的花括号，是为了精确地控制 `std::lock_guard` 对象 `lock` 的生命周期。
    // 这是 C++ 中一个常见且优雅的技巧，用于最小化互斥锁的锁定时间。
    {
        // [语法点] `std::lock_guard` - RAII风格的互斥锁。
        // 这是保证线程安全的关键。
        // 1. [加锁] 当 `lock` 对象在这里被创建时，它会自动调用 `mutex_.lock()`，锁住成员变量 `mutex_`。
        // 2. [保护] 在这个花括号作用域内，`main_tasks_` 队列被安全地保护起来，可以防止其他线程同时修改它。
        // 3. [自动解锁] 当代码执行到右花括号 `}` 时，`lock` 对象超出作用域被销毁，其析构函数会自动调用 `mutex_.unlock()`，确保锁一定会被释放。
        std::lock_guard<std::mutex> lock(mutex_);

        // 将任务添加到队列中。
        // `main_tasks_` 是一个 `std::list`，存储着所有待执行的任务。
        // `push_back` 将新任务添加到列表的末尾。
        // [语法点] `std::move` - 移动语义，提升性能。
        // `std::function` 对象可能比较“重”（如果它捕获了很多数据），拷贝它可能会有性能开销。
        // `std::move(callback)` 并不会真的移动内存，而是将 `callback` 转换为“右值引用”，
        // 允许 `push_back` “窃取”`callback` 内部的资源来构造新元素，而不是进行一次完整的深拷贝。这非常高效。
        main_tasks_.push_back(std::move(callback));
    } // `lock` 在这里被销毁，互斥锁 `mutex_` 被自动解锁。

    // [FreeRTOS API] - 发出“通知信号”。
    // `xEventGroupSetBits` 函数的作用是在我们之前创建的 `event_group_` 事件组中，设置一个或多个事件位。
    // `SCHEDULE_EVENT` 是一个宏（例如 `1 << 0`），代表“有新任务需要处理”这个特定的事件。
    // `MainEventLoop` 函数正在 `xEventGroupWaitBits` 上阻塞等待这个事件位被设置。
    // 所以，这一行代码执行后，就会立即唤醒正在休眠的 `MainEventLoop` 任务，让它开始处理我们刚刚放入队列中的任务。
    xEventGroupSetBits(event_group_, SCHEDULE_EVENT);
}

// The Main Event Loop controls the chat state and websocket connection
// If other tasks need to access the websocket or chat state,
// they should use Schedule to call this function
// [函数定义] Application 类的成员函数 MainEventLoop。
// [核心职责] 作为应用程序的主事件循环，它是一个永不退出的函数。它平时处于休眠状态，一旦被 `Schedule` 函数唤醒，
//            就负责从任务队列中取出所有待办任务，并按顺序一一执行。
void Application::MainEventLoop() {
    // [流程控制] while(true) 创建了一个无限循环，确保这个任务在程序运行期间永远作为后台服务存在。
    while (true) {
        // [FreeRTOS API] 等待事件，这是整个循环能够高效运行的关键。
        // `xEventGroupWaitBits` 会“阻塞”当前任务，使其进入休眠状态，完全不消耗CPU资源，直到指定的事件发生。
        // [参数详解]:
        //  - event_group_:    要等待的目标事件组。
        //  - SCHEDULE_EVENT: 要等待的特定事件位（一个或多个）。
        //  - pdTRUE:         表示在函数成功返回后，自动将 `SCHEDULE_EVENT` 这个事件位“清零”，以便下次可以继续等待新的事件。
        //  - pdFALSE:        表示如果指定了多个事件位，只要有一个发生就唤醒任务，不必等待所有事件位都满足。
        //  - portMAX_DELAY:  表示无限期地等待，永不超时。
        auto bits = xEventGroupWaitBits(event_group_, SCHEDULE_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);

        // [流程控制] 检查唤醒的原因。
        // [语法点] 位与操作 `&`。这是一个健壮的检查，确保我们确实是被 `SCHEDULE_EVENT` 事件唤醒的。
        if (bits & SCHEDULE_EVENT) {
            // [语法点] `std::unique_lock` - 一种比 `std::lock_guard` 更灵活的RAII风格互斥锁。
            // 我们在这里使用 `unique_lock` 是因为它支持“手动解锁”(`unlock()`)，这是下面性能优化的关键。
            // 在创建时，`lock` 对象会自动锁定 `mutex_`，保护共享的 `main_tasks_` 队列。
            std::unique_lock<std::mutex> lock(mutex_);

            // [核心性能优化] 使用 `std::move` 将整个任务队列的所有权“转移”到一个局部变量 `tasks` 中。
            // 对于 `std::list` 这样的链表容器，这个操作的成本极低（近乎于零），它只交换了几个内部指针，
            // 而完全避免了逐个元素拷贝整个列表所带来的巨大开销。
            // 执行后，成员变量 `main_tasks_` 会变为空列表。
            std::list<std::function<void()>> tasks = std::move(main_tasks_);
            
            // [核心性能优化] 立即手动解锁！
            // 这是使用 `std::unique_lock` 的主要原因。在我们将任务从共享队列安全地转移到本地变量后，
            // 共享资源就不再被访问了，因此应立刻释放锁。这使得“临界区”（即锁被持有的时间）变得极短。
            // 这样做可以极大地提高并发性能，因为在下面 for 循环执行可能耗时的任务时，
            // 其他线程仍然可以无阻塞地调用 `Schedule` 函数来提交新任务。
            lock.unlock();
            
            // [任务执行] 遍历本地的 `tasks` 列表。
            // [语法点] `auto&` - 使用引用来遍历，避免对列表中的 `std::function` 对象产生不必要的拷贝。
            for (auto& task : tasks) {
                // [语法点] 函数对象调用。
                // `task` 变量本身是一个 `std::function` 对象，可以像普通函数一样使用 `()` 操作符来调用，
                // 这会执行我们当初通过 `Schedule` 传递进来的那个 Lambda 表达式的函数体。
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

// [函数定义] Application 类的成员函数 OnAudioOutput。
// [核心职责] 作为音频播放循环的一部分，负责从音频解码队列中取出一个数据包，并调度一个异步任务来解码和播放它。
void Application::OnAudioOutput() {
    // [互斥保护] 使用一个布尔标志 `busy_decoding_audio_` 作为简单的互斥锁或“防抖”机制。
    // 如果上一个数据包的解码任务已经被调度但尚未开始执行，则直接返回，避免连续调度多个任务造成拥堵。
    if (busy_decoding_audio_) {
        return;
    }

    // [C++11 chrono] 获取当前时间点，用于后续的超时判断。
    // `steady_clock` 是一个单调时钟，不受系统时间调整的影响，最适合用来测量时间间隔。
    auto now = std::chrono::steady_clock::now();
    auto codec = Board::GetInstance().GetAudioCodec();
    const int max_silence_seconds = 10;

    // [线程安全] 使用 `std::unique_lock` 来准备对共享资源 `audio_decode_queue_` 的访问。
    // `unique_lock` 的灵活性（可手动解锁）在这里是必需的。
    std::unique_lock<std::mutex> lock(mutex_);
    
    // [核心逻辑] - 检查待解码队列是否为空。
    if (audio_decode_queue_.empty()) {
        // [节能逻辑] - 如果队列为空，意味着当前没有音频需要播放。
        // "// Disable the output if there is no audio data for a long time"
        // 只有当设备处于“空闲”状态时，才启用这个超时关闭功能。
        if (device_state_ == kDeviceStateIdle) {
            // [C++11 chrono] 计算从上次播放音频到现在，过去了多长时间。
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_output_time_).count();
            // 如果静默时间超过了设定的最大值（10秒），则关闭音频输出硬件以节省电力。
            if (duration > max_silence_seconds) {
                codec->EnableOutput(false);
            }
        }
        // 队列为空，无事可做，直接返回。
        return;
    }

    // [状态清理逻辑] - 如果设备状态已经切换到了“聆听中”。
    if (device_state_ == kDeviceStateListening) {
        // 这意味着队列中可能还残留着上一个“说话中”状态的音频包，这些包现在必须被丢弃。
        audio_decode_queue_.clear();
        // [核心同步点] - 通知所有可能正在等待的线程。
        // `notify_all()` 会唤醒所有正在 `audio_decode_cv_.wait()` 上休眠的线程（比如 `PlaySound` 函数所在的线程）。
        // 这确保了 `PlaySound` 不会因为队列被清空而永远等待下去。
        audio_decode_cv_.notify_all();
        return;
    }

    // [核心逻辑] - 从队列中取出一个音频包准备处理。
    // [语法点] `std::move` - `front()` 获取对队首元素的引用，`std::move` 将该元素的所有权高效地转移给局部变量 `opus`。
    auto opus = std::move(audio_decode_queue_.front());
    // `pop_front()` 将队列中（现已为空的）队首元素移除。
    audio_decode_queue_.pop_front();
    
    // [性能优化] 立即手动解锁！
    // 我们已经安全地从共享队列中取出了数据，应立刻释放锁，让其他线程（如网络线程）可以继续往队列里放数据。
    // 这使得互斥锁的持有时间被最小化，极大地提高了并发性能。
    lock.unlock();
    
    // [核心同步点] - 通知所有等待者。
    // 再次调用 `notify_all()`。这主要是为了通知那些可能因为队列“满”而等待的生产者（虽然本例中队列无界），
    // 或者其他需要知道队列状态已改变的线程。
    audio_decode_cv_.notify_all();

    // [异步任务调度]
    // 设置“解码忙”标志，防止在本轮解码任务完成前，`OnAudioOutput` 再次被调用时重复调度。
    busy_decoding_audio_ = true;
    // 将真正耗时的解码和重采样工作，打包成一个Lambda，交给后台任务线程去执行。
    // 这可以避免阻塞 `AudioLoop` 这个实时性要求较高的循环。
    background_task_->Schedule([this, codec, opus = std::move(opus)]() mutable {
        // [Lambda 捕获详解]
        // - `this`, `codec`: 按值捕获指针。
        // - `opus = std::move(opus)`: [C++14 初始化捕获] 高效地将 `opus` 向量的所有权从外部作用域“移动”到Lambda内部。
        // - `mutable`: [语法点] Lambda 默认是 `const` 的，其内部不能修改捕获的成员。但下面的 `std::move(opus)` 需要修改
        //              Lambda 自己的 `opus` 成员，因此需要用 `mutable` 关键字来解除这个限制。

        // 这是在后台任务线程中执行的第一行代码。立即清除标志，允许 `AudioLoop` 可以调度下一个数据包。
        busy_decoding_audio_ = false;
        // 如果在任务被调度后，播放被中止，则直接放弃解码。
        if (aborted_) {
            return;
        }

        // `pcm` 向量用于存放解码后的原始音频数据。
        std::vector<int16_t> pcm;
        // 调用解码器进行解码，`std::move` 再次用于将 `opus` 数据所有权传递给解码函数。
        if (!opus_decoder_->Decode(std::move(opus), pcm)) {
            return;
        }
        // 如果解码后的采样率与硬件输出采样率不符，则进行重采样。
        if (opus_decoder_->sample_rate() != codec->output_sample_rate()) {
            int target_size = output_resampler_.GetOutputSamples(pcm.size());
            std::vector<int16_t> resampled(target_size);
            output_resampler_.Process(pcm.data(), pcm.size(), resampled.data());
            // 再次使用 `std::move` 高效地用重采样后的数据替换掉 `pcm` 中的旧数据。
            pcm = std::move(resampled);
        }
        // 将最终的PCM数据交给硬件进行播放。
        codec->OutputData(pcm);
        // 更新最后一次成功播放音频的时间点。
        last_output_time_ = std::chrono::steady_clock::now();
    });
}

// [函数定义] Application 类的成员函数 OnAudioInput。
// [核心职责] 在每个音频循环中被调用，负责从麦克风读取音频数据，并将其“喂”给当前处于活动状态的音频消费者
//            （如唤醒词引擎、音频处理器或直接给编码器）。
void Application::OnAudioInput() {
// [编译时分支 1] - 如果项目中启用了“唤醒词检测”功能。
// 这是最高优先级的音频消费者。
#if CONFIG_USE_WAKE_WORD_DETECT
    // 首先检查唤醒词引擎当前是否正在运行。
    if (wake_word_detect_.IsDetectionRunning()) {
        // 创建一个临时的 vector 用于存放音频数据。
        std::vector<int16_t> data;
        // 向唤醒词引擎查询它下一次处理需要多少个音频样本。这是一个“拉模式”的数据供给。
        int samples = wake_word_detect_.GetFeedSize();
        if (samples > 0) {
            // 如果需要数据，就调用 ReadAudio 函数，精确地读取所需数量和采样率的音频。
            ReadAudio(data, 16000, samples);
            // 将读取到的数据“喂”给唤醒词引擎进行处理。
            wake_word_detect_.Feed(data);
            // [流程控制] 任务完成，直接返回。
            // 这确保了当唤醒词引擎活动时，音频数据不会被后续的任何其他模块处理。
            return;
        }
    }
#endif
// [编译时分支 2] - 如果项目中启用了“音频处理器”功能（但未启用更高优先级的唤醒词）。
#if CONFIG_USE_AUDIO_PROCESSOR
    // 逻辑与上面的唤醒词模块完全相同，只是数据消费者换成了 audio_processor_。
    if (audio_processor_.IsRunning()) {
        std::vector<int16_t> data;
        int samples = audio_processor_.GetFeedSize();
        if (samples > 0) {
            ReadAudio(data, 16000, samples);
            // 将数据“喂”给音频处理器（可能进行回声消除、降噪等）。
            audio_processor_.Feed(data);
            return; // 同样，处理完后直接返回。
        }
    }
// [编译时分支 3] - “基础模式”，如果上述两种高级功能都未启用。
#else
    // 在基础模式下，只有当设备明确处于“聆听中”的状态时，我们才需要录音并发送。
    if (device_state_ == kDeviceStateListening) {
        std::vector<int16_t> data;
        // 直接读取一个固定长度的音频帧。
        // `30 * 16000 / 1000` 计算出 16kHz 采样率下，30毫秒时长的样本数（即480个样本）。
        ReadAudio(data, 16000, 30 * 16000 / 1000);
        
        // [设计模式] 启动一个多阶段的异步流水线，将“编码并发送”的任务 offload 到后台。
        background_task_->Schedule([this, data = std::move(data)]() mutable {
            // [Lambda 捕获] 使用 C++14 初始化捕获，高效地将 `data` 移动到后台任务中。
            // `mutable` 关键字是必需的，因为它允许我们在 Lambda 内部再次 `std::move(data)`。
            
            // 如果网络繁忙，则丢弃这一帧数据，避免数据积压。
            if (protocol_->IsAudioChannelBusy()) {
                return;
            }
            // 调用编码器进行编码。`Encode` 函数本身也是异步的，它接收一个完成回调。
            opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
                // 当编码完成后，这个内层 Lambda 会被调用，参数 `opus` 是编码好的数据包。
                // 再次使用 `Schedule`，将最终的网络发送操作提交到主事件循环，以保证对 `protocol_` 访问的线程安全。
                Schedule([this, opus = std::move(opus)]() {
                    protocol_->SendAudio(opus);
                });
            });
        });
        // 调度完任务后，本轮 `OnAudioInput` 的工作就完成了。
        return;
    }
#endif
    // [CPU 资源释放] - 如果以上所有 `if` 条件都不满足（例如，设备处于空闲状态且无唤醒引擎在运行），
    // 那么 `AudioLoop` 在这个周期内无事可做。
    // `vTaskDelay` 会让当前任务（AudioLoop 任务）休眠一小段时间（30毫秒）。
    // 这是至关重要的！它避免了 `AudioLoop` 的 `while(true)` 循环空转，从而将CPU时间让给其他任务，并能有效降低功耗。
    vTaskDelay(pdMS_TO_TICKS(30));
}

// [函数定义] Application 类的成员函数 ReadAudio。
// [核心职责] 从音频编解码器(codec)读取原始音频数据，并根据需要进行重采样和通道处理，
//            最终将指定数量(`samples`)、指定采样率(`sample_rate`)的音频数据填充到调用者提供的 `data` 向量中。
// [参数详解]
//   - `std::vector<int16_t>& data`: [核心语法点] 一个 `std::vector` 的“引用”(`&`)。
//     这意味着函数直接操作调用者传入的原始 vector 对象，而不是它的副本。函数对 `data` 的所有修改都会在函数外部生效。
//     这是一种高效传递和返回大型数据容器的方式。
//   - `int sample_rate`: 调用者期望得到的音频数据的采样率（例如 16000 Hz）。
//   - `int samples`: 调用者期望得到的音频样本的数量。
void Application::ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples) {
    // 获取音频编解码器(codec)的单例对象指针。
    auto codec = Board::GetInstance().GetAudioCodec();

    // [核心逻辑分支] - 判断硬件输出的采样率是否与期望的采样率一致。
    if (codec->input_sample_rate() != sample_rate) {
        // [重采样路径] - 如果采样率不匹配，就需要进行复杂的重采样处理。
        
        // [核心逻辑] 计算需要从硬件读取多少原始样本，才能在重采样后得到期望数量的样本。
        // 例如，要从48KHz转成16KHz，需要的原始数据量是目标的三倍。
        data.resize(samples * codec->input_sample_rate() / sample_rate);
        
        // 调用 codec 的 InputData 方法，将原始音频数据读入 `data` 向量。
        // 如果读取失败（例如硬件故障），则直接返回。
        if (!codec->InputData(data)) {
            return;
        }

        // [核心逻辑分支] - 判断原始音频是双声道还是单声道。
        if (codec->input_channels() == 2) {
            // [双声道(Stereo)处理路径] - 通常用于带回声消除(AEC)的场景，一个声道是麦克风，另一个是参考音频。
            
            // [语法点] `auto` - 自动类型推导。编译器会自动推断出 mic_channel 的类型是 `std::vector<int16_t>`。
            // 创建两个新的 vector，分别用于存放分离出来的麦克风通道和参考通道的数据。
            auto mic_channel = std::vector<int16_t>(data.size() / 2);
            auto reference_channel = std::vector<int16_t>(data.size() / 2);

            // [核心算法] - 解交织 (De-interleaving)。
            // 原始的 `data` 向量中，双声道数据是交错存放的 (L0, R0, L1, R1, L2, R2, ...)。
            // 这个循环将它们分拆到各自独立的声道向量中。
            // `j += 2` 确保每次循环在原始数据中跳过两个样本。
            for (size_t i = 0, j = 0; i < mic_channel.size(); ++i, j += 2) {
                mic_channel[i] = data[j];       // 偶数索引是麦克风数据
                reference_channel[i] = data[j + 1]; // 奇数索引是参考音频数据
            }
            
            // 为重采样后的数据准备好目标向量。
            // GetOutputSamples 会根据输入样本数和采样率变换关系，预先计算出输出样本数，避免中途重新分配内存。
            auto resampled_mic = std::vector<int16_t>(input_resampler_.GetOutputSamples(mic_channel.size()));
            auto resampled_reference = std::vector<int16_t>(reference_resampler_.GetOutputSamples(reference_channel.size()));
            
            // [核心算法] - 对分离出的每个声道分别进行重采样。
            // [语法点] `.data()`: `std::vector` 的成员函数，返回一个指向其底层连续内存块的原始指针 (`int16_t*`)。
            // 很多C风格的库或高性能计算库（如此处的 resampler）的API需要接收原始指针作为参数。
            input_resampler_.Process(mic_channel.data(), mic_channel.size(), resampled_mic.data());
            reference_resampler_.Process(reference_channel.data(), reference_channel.size(), resampled_reference.data());
            
            // 重新调整 `data` 向量的大小，以容纳重采样后的双声道数据。
            data.resize(resampled_mic.size() + resampled_reference.size());

            // [核心算法] - 重交织 (Re-interleaving)。
            // 将已经重采样好的、分离的两个声道数据，重新合并回 `data` 向量，形成交错格式。
            for (size_t i = 0, j = 0; i < resampled_mic.size(); ++i, j += 2) {
                data[j] = resampled_mic[i];
                data[j + 1] = resampled_reference[i];
            }
        } else {
            // [单声道(Mono)处理路径] - 逻辑相对简单。
            
            // 创建一个用于存放重采样结果的临时向量。
            auto resampled = std::vector<int16_t>(input_resampler_.GetOutputSamples(data.size()));
            // 直接对原始数据进行重采样。
            input_resampler_.Process(data.data(), data.size(), resampled.data());
            
            // [语法点] `std::move` - 移动语义。
            // 这里不使用 `data = resampled;` (这会执行一次昂贵的、逐元素的深拷贝)。
            // 而是使用 `std::move`，它将 `resampled` 向量内部的内存缓冲区的所有权“转移”给 `data` 向量。
            // 这个操作本身非常快，几乎是零成本的，是现代C++中处理大型数据的关键性能优化技巧。
            data = std::move(resampled);
        }
    } else {
        // [简单路径] - 如果硬件采样率与期望采样率一致，则无需任何重采样和通道处理。
        
        // 直接将 `data` 向量的大小调整为调用者期望的大小。
        data.resize(samples);
        // 直接从 codec 读取数据填充 `data` 向量。
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

// [函数定义] Application 类的成员函数 SetDeviceState。
// [核心职责] 将应用程序的整体状态从当前状态安全地转换到指定的新状态，并执行进入新状态时所需的所有“入口动作”(Entry Actions)。
// [参数] DeviceState state: 要进入的目标新状态，是一个枚举类型。
void Application::SetDeviceState(DeviceState state) {
    // [优化与保护] 这是一个“守卫子句”(Guard Clause)。
    // 如果要设置的状态与当前状态相同，则无需执行任何操作，直接返回。
    // 这可以避免不必要的状态切换逻辑，提高效率，并防止潜在的递归或逻辑错误。
    if (device_state_ == state) {
        return;
    }
    
    // [状态更新准备]
    // 重置秒计数器，这样每个新状态都可以从0开始计时，方便实现状态内的超时等逻辑。
    clock_ticks_ = 0;
    // 保存切换前的状态，这对于某些需要根据“从哪个状态而来”决定行为的逻辑非常有用。
    auto previous_state = device_state_;
    // 正式更新成员变量，将当前状态切换为新状态。
    device_state_ = state;
    // [日志记录] 将状态切换打印到日志中，并使用我们之前定义的 `STATE_STRINGS` 数组将枚举值转换为人类可读的字符串，方便调试。
    ESP_LOGI(TAG, "STATE: %s", STATE_STRINGS[device_state_]);

    // [核心同步点] - 这是保证状态切换安全的关键步骤！
    // "The state is changed, wait for all background tasks to finish"
    // 在执行新状态的初始化动作之前，调用 `WaitForCompletion` 来阻塞当前函数，
    // 直到 `background_task_` 中所有先前被调度的任务全部执行完毕。
    // 这可以防止旧状态的残留任务与新状态的初始化动作发生冲突（例如，旧的音频解码任务与新的解码器重置操作冲突）。
    background_task_->WaitForCompletion();

    // [通用状态变更动作] - 无论进入哪个新状态，都先获取硬件对象的句柄/引用。
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    auto led = board.GetLed();
    // 通知 LED 模块主状态已变更，以便 LED 可以根据新状态更新其显示模式（例如，从“常亮”变“呼吸”）。
    led->OnStateChanged();

    // [设计模式] 状态机入口动作 (State Machine Entry Actions)。
    // 使用 `switch` 语句，根据进入的新状态，执行不同的初始化和配置代码。
    switch (state) {
        // [语法点] `case` 穿透 (Fall-through)。
        // `kDeviceStateUnknown` 后面没有 `break`，这意味着如果进入 `kDeviceStateUnknown` 状态，
        // 将会继续执行 `kDeviceStateIdle` 的代码。这是一种常见的技巧，用于将多个状态归并到同一种处理逻辑。
        case kDeviceStateUnknown:
        case kDeviceStateIdle:
            // [入口动作] - 进入“空闲”状态时：
            // 1. 更新UI，显示“待机”状态和“中性”表情。
            display->SetStatus(Lang::Strings::STANDBY);
            display->SetEmotion("neutral");
// [语法点] 条件编译 `#if ... #endif`
// 根据项目配置，决定是否编译这部分代码。
#if CONFIG_USE_AUDIO_PROCESSOR
            // 2. 如果启用了音频处理器，则停止它。
            audio_processor_.Stop();
#endif
#if CONFIG_USE_WAKE_WORD_DETECT
            // 3. 如果启用了唤醒词，则启动它。设备现在处于等待被唤醒的状态。
            wake_word_detect_.StartDetection();
#endif
            // `break` 语句用于跳出 `switch` 结构，防止代码意外地执行到下一个 `case`。
            break;
        case kDeviceStateConnecting:
            // [入口动作] - 进入“连接中”状态时：
            // 更新UI，显示“连接中”状态，并将聊天消息清空。
            display->SetStatus(Lang::Strings::CONNECTING);
            display->SetEmotion("neutral");
            display->SetChatMessage("system", "");
            break;
        case kDeviceStateListening:
            // [入口动作] - 进入“聆听中”状态时：
            display->SetStatus(Lang::Strings::LISTENING);
            display->SetEmotion("neutral");

            // 在通知服务器开始聆听之前，先将最新的物联网(IoT)设备状态上报。
            UpdateIotStates();

            // 确保音频处理器正在运行。
#if CONFIG_USE_AUDIO_PROCESSOR
            if (!audio_processor_.IsRunning()) {
#else
            // 这个 `#else` 分支中的 `if (true)` 是一种技巧，确保即使 `CONFIG_USE_AUDIO_PROCESSOR` 未定义，
            // 内部的代码块也总能被编译和执行。
            if (true) {
#endif
                // 通知服务器，设备已开始聆听，并告知聆听模式。
                protocol_->SendStartListening(listening_mode_);
                // 如果是从“说话中”状态切换过来的，并且是自动停止模式，则需要一个短暂延时。
                // 这个 `FIXME` 注释表明，这是一个临时的解决方案，可能需要更优雅的方式来等待扬声器缓冲区清空。
                if (listening_mode_ == kListeningModeAutoStop && previous_state == kDeviceStateSpeaking) {
                    // FIXME: Wait for the speaker to empty the buffer
                    vTaskDelay(pdMS_TO_TICKS(120));
                }
                // 重置 Opus 编码器状态，准备对新的录音进行编码。
                opus_encoder_->ResetState();
#if CONFIG_USE_WAKE_WORD_DETECT
                // 聆听时，关闭唤醒词检测，避免冲突。
                wake_word_detect_.StopDetection();
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
                // 启动音频处理器，开始处理麦克风数据。
                audio_processor_.Start();
#endif
            }
            break;
        case kDeviceStateSpeaking:
            // [入口动作] - 进入“说话中”（播放服务器音频）状态时：
            display->SetStatus(Lang::Strings::SPEAKING);

            // 如果不是“实时聊天”模式，那么在说话时，可以停止本地音频处理，并重新开启唤醒词检测，以支持打断。
            if (listening_mode_ != kListeningModeRealtime) {
#if CONFIG_USE_AUDIO_PROCESSOR
                audio_processor_.Stop();
#endif
#if CONFIG_USE_WAKE_WORD_DETECT
                wake_word_detect_.StartDetection();
#endif
            }
            // 重置 Opus 解码器，准备解码即将从服务器收到的音频流。
            ResetDecoder();
            break;
        // [语法点] `default` case。
        // 如果传入的 `state` 是一个未在上面任何 `case` 中列出的值，`default` 分支将被执行。
        default:
            // 在这里不执行任何操作，是一种明确的设计选择，表示对于未知的状态转换，我们选择忽略。
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
