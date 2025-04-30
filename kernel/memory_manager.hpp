#pragma once
#include <limits>
#include <array>
#include <sys/types.h>
#include "error.hpp"
#include "logger.hpp"
#include "memory_map.hpp"

namespace {
    constexpr unsigned long long operator"" _KiB(unsigned long long kib) {
        return kib * 1024;
    }
    constexpr unsigned long long operator"" _MiB(unsigned long long mib) {
        return mib * 1024_KiB;
    }
    constexpr unsigned long long operator"" _GiB(unsigned long long gib) {
        return gib * 1024_MiB;
    }
}

static const auto kBytesPerFrame{4_KiB};

class FrameID {
    public:
        explicit FrameID(size_t id) : id_{id} {};
        size_t ID() const {return id_; }
        void* Frame() const { return reinterpret_cast<void*>(id_ * kBytesPerFrame); }
    
    private:
        size_t id_;
};

class BitmapMemoryManager {
    public:
        static const auto kMaxPhysicalMemoryBytes{128_GiB};
        static const auto kFrameCount{kMaxPhysicalMemoryBytes / kBytesPerFrame};

        using MapLineType = unsigned long;
        static const size_t kBitsPerMapLine{8 * sizeof(MapLineType)};

        BitmapMemoryManager();

        // Allocate a space for specified number of frames and return first frame id.
        WithError<FrameID> Allocate(size_t num_frames);
        Error Free(FrameID start_frame, size_t num_frames);
        void MarkAllocated(FrameID start_frame, size_t num_frames);

        // Set the memory range that this memory manager handls.
        void SetMemoryRange(FrameID range_begin, FrameID range_end);

        private:
            // 1bit == 1 page frame 
            std::array<MapLineType, kFrameCount / kBitsPerMapLine> alloc_map_;
            FrameID range_begin_;
            FrameID range_end_;
            bool GetBit(FrameID) const;
            void SetBit(FrameID frame, bool allocated);
};

static const FrameID kNullFrame{std::numeric_limits<size_t>::max()};

void InitializeMemoryManager(const MemoryMap& memory_map);