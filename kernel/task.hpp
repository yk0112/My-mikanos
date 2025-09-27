#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <deque>
#include <optional>
#include "error.hpp"
#include "message.hpp"

struct TaskContext {
    uint64_t cr3, rip, rflags, reserved1;
    uint64_t cs, ss, fs, gs;
    uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    std::array<uint8_t, 512> fxsave_area;
} __attribute__((packed));

extern TaskContext task_a_ctx, task_b_ctx;

using TaskFunc = void (uint64_t, int64_t);

class TaskManager;

class Task {
    public:
        static const int kDefaultLevel = 1;
        static const size_t kDefaultStackBytes = 8 * 4096;
        Task(uint64_t id);
        Task& InitContext(TaskFunc* f, int64_t data);
        TaskContext& Context();
        Task& Sleep();
        Task& Wakeup();
        uint64_t ID() const;
        void SendMessage(const Message& msg);
        std::optional<Message> ReceiveMessage(); 
        int Level() const { return level_; }
        bool Running() const { return running_; }
    private:
        uint64_t id_;
        std::vector<uint64_t> stack_;
        alignas(16) TaskContext context_;
        std::deque<Message> msgs_;
        unsigned int level_{kDefaultLevel};
        bool running_{false};

        Task& SetLevel(int level) { level_ = level; return *this;}
        Task& SetRunning(bool running) { running_ = running; return *this;}

        friend TaskManager;
};

class TaskManager {
    public:
        static const int kMaxLevel = 3;
        TaskManager();
        Task& NewTask();
        void SwitchTask(bool current_sleep = false);
        void Sleep(Task* task);
        Error Sleep(uint64_t id);
        void Wakeup(Task* task, int level = -1);
        Error Wakeup(uint64_t id, int level = -1);
        Task& CurrentTask();
        Error SendMessage(uint64_t id, const Message& msg);
    private:
        std::vector<std::unique_ptr<Task>> tasks_{};
        uint64_t latest_id_{0};
        std::array<std::deque<Task*>, kMaxLevel + 1> running_{}; // 実行待ちのタスクを保持
        int current_level_{kMaxLevel};
        bool level_changed_{false};
        void ChangeLevelRunning(Task* task, int level);
};

extern TaskManager* task_manager;

void InitializeTask();