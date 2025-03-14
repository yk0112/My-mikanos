#pragma once

#include <cstdint>
#include <array>
#include "error.hpp"

namespace pci {
    const uint16_t kConfigAddress = 0x0cf8;
    const uint16_t kConfigData = 0x0cfc;

    struct  ClassCode {
        uint8_t base, sub, interface;
        bool Match(uint8_t b) { return b == base; }
        bool Match(uint8_t b, uint8_t s) { return Match(b) && s == sub; }
        bool Match(uint8_t b, uint8_t s, uint8_t i) {
            return Match(b, s) && i == interface;
        }
    };
    
    struct Device {
        uint8_t bus, device, function, header_type;
        ClassCode class_code;
    };

    inline std::array<Device, 32> devices;  // Information of each PCI device
    inline int num_device;      // Number of PCI devices

    void WriteAddress(uint32_t address);
    void WriteData(uint32_t value);

    uint32_t ReadData();
    uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function);
    uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function);
    uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function);
    ClassCode ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);
    uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function);
    
    inline uint16_t ReadVendorId(const Device& dev) {
        return ReadVendorId(dev.bus, dev.device, dev.function);
    }
    
    constexpr uint8_t CalcBarAddress(unsigned int bar_index) {
        return 0x10 + 4 * bar_index;
    }
    void WriteConfReg(const Device& dev, uint8_t reg_addr, uint32_t value);
    bool isSingleFunctionDevice(uint8_t header_type);
    Error AddDevice(const Device& device);
    Error ScanFunction(uint8_t bus, uint8_t device, uint8_t function);
    Error ScanDevice(uint8_t bus, uint8_t device);
    Error ScanBus(uint8_t bus);
    Error ScanAllBus();
    withError<uint64_t> ReadBar(Device& device, unsigned int bar_index);
    uint32_t ReadConfReg(const Device& dev, uint8_t reg_addr);
}