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
        TimerManager();
        void AddTimer(const Timer& timer);
        bool Tick();
        unsigned long CurrentTick() const { return tick_; }
    private:
        volatile unsigned long tick_{0};
        std::priority_queue<Timer> timers_{};
};

extern TimerManager* timer_manager;
extern unsigned long lapic_timer_freq;  // APICタイマの1秒間あたりのカウント数(周波数)
const int kTimerFreq = 100;
const int kTaskTimerPeriod = static_cast<int>(kTimerFreq * 0.02);  // タスクの切り替え周期
const int kTaskTimerValue = std::numeric_limits<int>::min();


void LAPICTimerOnInterrupt();
void InitializeLAPICTimer();
void StartLAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();