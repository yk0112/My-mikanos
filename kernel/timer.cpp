#include <limits>
#include "timer.hpp"
#include "interrupt.hpp"
#include "acpi.hpp"

namespace {
    const uint32_t kCountMax = 0xffffffffu;
    volatile uint32_t& lvt_timer = *reinterpret_cast<uint32_t*>(0xfee00320);
    volatile uint32_t& initial_count = *reinterpret_cast<uint32_t*>(0xfee00380);
    volatile uint32_t& current_count = *reinterpret_cast<uint32_t*>(0xfee00390);
    volatile uint32_t& divide_config = *reinterpret_cast<uint32_t*>(0xfee003e0);
}

TimerManager* timer_manager;
unsigned long lapic_timer_freq; 

void TimerManager::Tick() {
    ++tick_;
    // 割り込みのたびにtime outしたタイマがないか調べる
    while(true) {
        const auto& t = timers_.top();
        if(t.Timeout() > tick_) {
            break;
        }
        Message m{Message::kTimerTimeout};
        m.arg.timer.timeout = t.Timeout();
        m.arg.timer.value = t.Value();
        msg_queue_.push_back(m);
        timers_.pop();
    }
}

TimerManager::TimerManager(std::deque<Message>& msg_deque) : msg_queue_{msg_deque} {
    timers_.push(Timer{std::numeric_limits<unsigned long>::max(), -1}); 
}

void TimerManager::AddTimer(const Timer& timer) {
    timers_.push(timer);
}

Timer::Timer(unsigned long timeout, int value) : timeout_{timeout}, value_{value} {
}

void LAPICTimerOnInterrupt() {
    timer_manager->Tick();
}

void InitializeLAPICTimer(std::deque<Message>& msg_queue) {
    timer_manager = new TimerManager{msg_queue};
    divide_config = 0b1011;
    lvt_timer = (0b001 << 16); // 単発モード、割り込み禁止
 
    // APICタイマの周波数を計測
    StartLAPICTimer();
    acpi::WaitMillseconds(100);
    const auto elapsed = LAPICTimerElapsed();
    StopLAPICTimer();
    
    lapic_timer_freq = static_cast<unsigned long>(elapsed) * 10;

    divide_config = 0b1011;
    lvt_timer = (0b010 << 16) | InterruptVector::kLAPICTimer;  // 周期モード、割り込み許可
    initial_count = lapic_timer_freq / kTimerFreq; // 10ミリ秒毎に割り込みが発生するように設定
}

void StartLAPICTimer() {
    initial_count = kCountMax; 
}

uint32_t LAPICTimerElapsed() {
    return kCountMax - current_count;
}

void StopLAPICTimer() {
    initial_count = 0;
}
