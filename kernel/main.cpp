#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <numeric>
#include <vector>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "mouse.hpp"
#include "logger.hpp"
#include "interrupt.hpp"
#include "asmfunc.h"
#include "queue.hpp"
#include "memory_map.hpp"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "layer.hpp"
#include "timer.hpp"
#include "message.hpp"
#include "acpi.hpp"
#include "keyboard.hpp"
#include "task.hpp"
#include "terminal.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

int printk(const char* format, ...) {
    va_list ap;
    int result;
    char s[1024];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    console->PutString(s);
    return result;
}

std::shared_ptr<ToplevelWindow> main_window;
unsigned int main_window_layer_id;
void InitializeMainWindow() {
    main_window = std::make_shared<ToplevelWindow>(
        160, 52, screen_config.pixel_format, "Hello Window");

    main_window_layer_id = layer_manager->NewLayer()
        .SetWindow(main_window)
        .SetDraggable(true)
        .Move({ 300, 100 })
        .ID();

    layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max()); // 最上部に配置
}

std::shared_ptr<ToplevelWindow> text_window;
unsigned int text_window_layer_id;
int text_window_index;
void InitializeTextWindow() {
    const int win_w = 160;
    const int win_h = 52;

    text_window = std::make_shared<ToplevelWindow>(
        win_w, win_h, screen_config.pixel_format, "Text Box Test");

    DrawTextbox(*text_window->InnerWriter(), { 0, 0 }, text_window->InnerSize());

    text_window_layer_id = layer_manager->NewLayer()
        .SetWindow(text_window)
        .SetDraggable(true)
        .Move({ 350, 200 })
        .ID();

    layer_manager->UpDown(text_window_layer_id, std::numeric_limits<int>::max());
}

void DrawTextCursor(bool visible) {
    const auto color = visible ? ToColor(0) : ToColor(0xffffff);
    const auto pos = Vector2D<int>{ 4 + 8 * text_window_index, 5 };
    FillRectangle(*text_window->InnerWriter(), pos, { 7, 15 }, color);
}

void InputTextWindow(char c) {
    if (c == 0) {
        return;
    }

    auto pos = []() { return Vector2D<int>{4 + 8 * text_window_index, 6}; };
    const int max_chars = (text_window->InnerSize().x - 8) / 8 - 1;

    if (c == '\b' && text_window_index > 0) {
        DrawTextCursor(false);
        --text_window_index;
        FillRectangle(*text_window->InnerWriter(), pos(), { 8, 16 }, ToColor(0xffffff));
        DrawTextCursor(true); // 末尾にカーソル表示
    }
    else if (c >= ' ' && text_window_index < max_chars) { // 制御文字を除外
        DrawTextCursor(false);
        WriteAscii(*text_window->InnerWriter(), pos(), c, ToColor(0));
        ++text_window_index;
        DrawTextCursor(true);
    }

    layer_manager->Draw(text_window_layer_id);
}

std::shared_ptr<ToplevelWindow> task_b_window;
unsigned int task_b_window_layer_id;
void InitializeTaskBWindow() {
    task_b_window = std::make_shared<ToplevelWindow>(
        160, 52, screen_config.pixel_format, "TaskB Window");

    task_b_window_layer_id = layer_manager->NewLayer()
        .SetWindow(task_b_window)
        .SetDraggable(true)
        .Move({ 100, 100 })
        .ID();

    layer_manager->UpDown(task_b_window_layer_id, std::numeric_limits<int>::max());
}

void TaskB(uint64_t task_id, int64_t data) {
    printk("TaskB: task_id=%d, data=%d\n", task_id, data);
    char str[128];
    int count = 0;

    __asm__("cli");
    Task& task = task_manager->CurrentTask();
    __asm__("sti");

    while (true) {
        ++count;
        sprintf(str, "%010d", count);
        FillRectangle(*task_b_window->InnerWriter(), { 20, 4 }, { 8 * 10, 16 }, { 0xc6, 0xc6, 0xc6 });
        WriteString(*task_b_window->InnerWriter(), { 20, 4 }, str, { 0,0,0 });
        Message msg{ Message::kLayer, task_id };
        msg.arg.layer.layer_id = task_b_window_layer_id;
        msg.arg.layer.op = LayerOperation::Draw;
        __asm__("cli");
        task_manager->SendMessage(1, msg);
        __asm__("sti");

        while (true) {
            __asm__("cli");
            auto msg = task.ReceiveMessage();
            if (!msg) {
                task.Sleep();
                __asm__("sti");
                continue;
            }

            if (msg->type == Message::kLayerFinish) {
                break;
            }
        }
    }
}

extern "C" void KernelMainNewStack(const struct FrameBufferConfig& frame_buffer_config_ref,
    const MemoryMap& memory_map_ref,
    const acpi::RSDP& acpi_table) {
    // Display background and Console
    InitializeGraphics(frame_buffer_config_ref);
    InitializeConsole();

    printk("Welcom to MikanOS!\n");
    SetLogLevel(kWarn);

    InitializeSegmentation();

    InitializePaging();

    InitializeMemoryManager(memory_map_ref);

    // Make Interrupt Descriptor Table(IDT) and MSI interrupt Settings.
    InitializeInterrupt();

    // Scan all PCI devices info
    InitializePCI();


    // Create background and console window, and initialize layer manager
    InitializeLayer();

    InitializeMainWindow();
    InitializeTextWindow();
    InitializeTaskBWindow();
    layer_manager->Draw({ {0, 0}, ScreenSize() });
    // active_layer->Activate(task_b_window_layer_id); この行を追加するとタスクBウィンドウが表示されない

    // initialize local APIC timer, Set a timer for cursor
    acpi::Initialize(acpi_table);
    InitializeLAPICTimer();
    const int kTextboxCursorTimer = 1;
    const int kTimer05sec = 50; // 0.5s = 10ms * 50
    __asm__("cli");
    timer_manager->AddTimer(Timer{ kTimer05sec, kTextboxCursorTimer });
    __asm__("sti");
    bool textbox_cursor_visible = false;

    // Initialize task manager
    InitializeTask(); // 現在のコンテキストを生成
    Task& main_task = task_manager->CurrentTask();

    const uint64_t taskb_id = task_manager->NewTask()
        .InitContext(TaskB, 45)
        .Wakeup()
        .ID();

    const uint64_t task_terminal_id = task_manager->NewTask().InitContext(TaskTerminal, 0).Wakeup().ID();

    // MSI interrupt settings, USB driver initialization, xhc restart
    usb::xhci::Initialize();
    InitializeMouse();
    // Register keyboard event handler with the driver
    InitializeKeyboard();

    char str[128];
    while (true) {
        __asm__("cli"); // Disable the interrupt flag for data race
        const auto tick = timer_manager->CurrentTick();
        __asm__("sti");  // Enable the interrupt

        sprintf(str, "%010lu", tick);
        FillRectangle(*main_window->InnerWriter(), { 20, 4 }, { 8 * 10, 16 }, { 0xc6, 0xc6, 0xc6 });
        WriteString(*main_window->InnerWriter(), { 20, 4 }, str, { 0,0,0 });
        layer_manager->Draw(main_window_layer_id);

        __asm__("cli");
        auto msg = main_task.ReceiveMessage();
        if (!msg) {
            main_task.Sleep();
            __asm__("sti");
            continue;
        }

        __asm__("sti");   // Enable the interrupt flag

        switch (msg->type) {
        case Message::kInterruptXHCI:
            usb::xhci::ProcessEvents();
            break;
        case Message::kTimerTimeout:
            if (msg->arg.timer.value == kTextboxCursorTimer) { // カーソル用のタイマ
                __asm__("cli");
                timer_manager->AddTimer(Timer{ msg->arg.timer.timeout + kTimer05sec, kTextboxCursorTimer });
                __asm__("sti");
                textbox_cursor_visible = !textbox_cursor_visible;
                DrawTextCursor(textbox_cursor_visible);
                layer_manager->Draw(text_window_layer_id);

                __asm__("cli");
                task_manager->SendMessage(task_terminal_id, *msg);
                __asm__("sti");
            }
            break;
        case Message::kKeyPush:
            if (auto act = active_layer->GetActive(); act == text_window_layer_id) {
                InputTextWindow(msg->arg.keyboard.ascii);
            }
            else if (act == task_b_window_layer_id) {
                if (msg->arg.keyboard.ascii == 's') {
                    printk("sleep TaskB: %s\n", task_manager->Sleep(taskb_id).Name());
                }
                else if (msg->arg.keyboard.ascii == 'w') {
                    printk("wakeup TaskB: %s\n", task_manager->Wakeup(taskb_id).Name());
                }
            }
            else {
                printk("key push not handled: keycode %02x, ascii %02x\n",
                    msg->arg.keyboard.keycode,
                    msg->arg.keyboard.ascii);
            }
            break;
        case Message::kLayer:
            ProcessLayerMessage(*msg);
            __asm__("cli");
            task_manager->SendMessage(msg->src_task, Message{ Message::kLayerFinish });
            __asm__("sti");
            break;
        default:
            Log(kError, "Unknown message type: %d\n", msg->type);
        }
    }
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}