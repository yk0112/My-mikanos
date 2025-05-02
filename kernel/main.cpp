#include <cstdio>
#include <cstdint>
#include <cstddef>
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
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"

std::deque<Message>* main_queue;
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

std::shared_ptr<Window> main_window;
unsigned int main_window_layer_id;
void InitializeMainWindow() {
    main_window = std::make_shared<Window>(160, 52, screen_config.pixel_format);
    DrawWindow(*main_window->Writer(), "Hello Window");
    main_window_layer_id = layer_manager->NewLayer()
        .SetWindow(main_window)
        .SetDraggable(true)
        .Move({300, 100})
        .ID();
    layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max()); // 最上部に配置
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
    ::main_queue = new std::deque<Message>(32);
    InitializeInterrupt(main_queue);

    // Scan all PCI devices info
    InitializePCI();  
   
    // MSI interrupt settings, USB driver initialization, xhc restart
    usb::xhci::Initialize();
    
    // Create background and console window, and initialize layer manager
    InitializeLayer();

    InitializeMainWindow();
    InitializeMouse(); 

    layer_manager->Draw({{0, 0}, ScreenSize()});
    
    acpi::Initialize(acpi_table);
    InitializeLAPICTimer(*main_queue);
    timer_manager->AddTimer(Timer{200, 2});
    timer_manager->AddTimer(Timer{600, -1});

    char str[128];

    while(true) {
        __asm__("cli"); // Disable the interrupt flag for data race
        const auto tick = timer_manager->CurrentTick();
        __asm__("sti");  // Wait until the interrupt occur

        sprintf(str, "%010lu", tick);
        FillRectangle(*main_window->Writer(), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
        WriteString(*main_window->Writer(), {24, 28}, str, {0,0,0});
        layer_manager->Draw(main_window_layer_id);

        __asm__("cli"); 
        if(main_queue->size() ==  0) {
            __asm__("sti\n\thlt");  // Wait until the interrupt occur
            continue;
        }
        
        Message msg = main_queue->front();
        main_queue->pop_front();
        __asm__("sti");   // Enable the interrupt flag

        switch (msg.type) {
        case Message::kInterruptXHCI:
            usb::xhci::ProcessEvents();
            break;
        case Message::kTimerTimeout:
            printk("Timer: timeout = %lu, value = %d\n", msg.arg.timer.timeout, msg.arg.timer.value);
            if(msg.arg.timer.value > 0) {
                timer_manager->AddTimer(Timer(
                    msg.arg.timer.timeout + 100, msg.arg.timer.value + 1));
            }
            break;
        default:
            Log(kError, "Unknown message type: %d\n", msg.type);
        }
    }
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}