#include "interrupt.hpp"
#include "timer.hpp"
#include "asmfunc.h"

std::array<InterruptDescriptor, 256> idt;

void NotifyEndOfInterrupt() {
    // Write to the address of the EOI (End Of Interrupt) register
    volatile auto end_of_interrupt = reinterpret_cast<uint32_t*>(0xfee000b0);
    *end_of_interrupt = 0;
}

void SetIDTEntry(InterruptDescriptor& desc, InterruptDescriptorAttribute attr, 
                uint64_t offset, uint16_t segment_selector) {
    desc.attr = attr;
    desc.offset_low = offset & 0xffffu;
    desc.offset_middle = (offset >> 16) & 0xffffu;
    desc.offset_high = offset >> 32;
    desc.segment_selector = segment_selector;
}

namespace {
    std::deque<Message>* msg_queue;
  
    __attribute__((interrupt))
    void IntHandlerXHCI(InterruptFrame* frame) {
      msg_queue->push_back(Message{Message::kInterruptXHCI});
      NotifyEndOfInterrupt();
    }
    
    __attribute__((interrupt))
    void IntHandlerAPICTimer(InterruptFrame* frame) {
      LAPICTimerOnInterrupt();
    }
}

// 割り込み記述子テーブルの設定
void InitializeInterrupt(std::deque<Message>* msg_queue) {
    ::msg_queue = msg_queue;
    // xHCI用の割り込みハンドラ設定(マウスやキーボード操作時に呼び出される)
    SetIDTEntry(idt[InterruptVector::kXHCI], MakeIDTAttr(DescriptorType::kInterruptGate,0),
                reinterpret_cast<uint64_t>(IntHandlerXHCI), kKernelCS);

    // local apicタイマ用の割り込みハンドラ設定
    SetIDTEntry(idt[InterruptVector::kLAPICTimer], MakeIDTAttr(DescriptorType::kInterruptGate,0),
                reinterpret_cast<uint64_t>(IntHandlerAPICTimer), kKernelCS);

    LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
}
