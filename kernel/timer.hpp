#pragma once
#include <message.hpp>
#include <queue>
#include <cstdint>

class Timer {
    public:
        Timer(unsigned long timeout, int value);
        unsigned long Timeout() const { return timeout_; }
        int Value() const { return value_; }
    private:
        unsigned long timeout_; // タイムアウト時間
        int value_; // 通知用の値
};

inline bool operator<(const Timer& lhs, const Timer& rhs) {
    return lhs.Timeout() > rhs.Timeout(); // timeoutが近いタイマを優先する
}

class TimerManager {
    public:
        TimerManager(std::deque<Message>& msg_queue);
        void AddTimer(const Timer& timer);
        void Tick();
        unsigned long CurrentTick() const { return tick_; }
    private:
        volatile unsigned long tick_{0};
        std::priority_queue<Timer> timers_{};
        std::deque<Message>& msg_queue_;
};

extern TimerManager* timer_manager;

void LAPICTimerOnInterrupt();
void InitializeLAPICTimer(std::deque<Message>& msg_queue);
void StartLAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();