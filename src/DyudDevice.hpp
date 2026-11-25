// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#pragma once

#include <cstdint>

namespace dyna {

struct FirmwareVersion {
    uint8_t major = 0;
    uint8_t minor1 = 0;
    uint8_t minor2 = 0;
    uint8_t minor3 = 0;

    uint32_t Uint32() const {
        return (uint32_t(major) << 24) | (uint32_t(minor1) << 16) | (uint32_t(minor2) << 8) | uint32_t(minor3);
    }

    bool NeedsNewInit() const {
        return major == 1 && minor1 == 0 && minor2 == 5 && minor3 >= 2;
    }

    bool IsXorBypassed() const {
        return Uint32() == 0x01000503u;
    }
};

} // namespace dyna

#ifdef _WIN32

#include "Crypto.hpp"
#include "WinUsbDevice.hpp"

#include <mutex>
#include <string>
#include <vector>

namespace dyna {

class DyudDevice {
public:
    static constexpr uint8_t kEpCmdOut = 0x02;
    static constexpr uint8_t kEpResIn = 0x84;
    static constexpr uint8_t kEpTsIn = 0x86;
    static constexpr uint32_t kBulkTimeoutMs = 2000;
    static constexpr uint32_t kSegModeFull = 0x00020000;
    static constexpr uint32_t kSegModeOne = 0x00010000;
    static constexpr size_t kStreamUrbCount = 16;
    static constexpr size_t kStreamUrbSize = 24064; // 47 * 512 = 128 * 188

    DyudDevice() = default;
    ~DyudDevice() { Close(); }

    bool Open(int device_index, std::string* err = nullptr);
    bool Init(std::string* err = nullptr);
    void Close();
    bool IsOpen() const { return usb_.IsOpen(); }

    bool Tune(uint32_t freq_khz, bool one_seg, bool enable_xor, std::string* err = nullptr);
    bool WaitLock(uint32_t timeout_ms, uint8_t* last_status = nullptr);
    bool GetSignalLevel(float& level_out);
    bool StartTS();
    bool StopTS();

    bool StartStream(WinUsbDevice::ChunkCallback cb);
    void StopStream();

    // 内蔵 B-CAS スロット制御（0xf0 APDU 中継）
    bool ResetBcas();
    bool TransmitBcas(const uint8_t* apdu, size_t len, std::vector<uint8_t>& resp_out);

    bool GetXorCipherCopy(XORCipher& out) const;
    const FirmwareVersion& Firmware() const { return fw_; }
    const std::string& Serial() const { return serial_; }

private:
    bool SendCommand(const uint8_t* cmd, size_t cmd_len, std::vector<uint8_t>& res_out, size_t res_len, std::string* err = nullptr);
    bool GetFirmwareVersion(FirmwareVersion& out);
    bool GetSerial(std::string& out);
    bool SetSegMode(uint32_t mode);
    bool SetTSXorKey(const uint8_t* key, size_t len);
    bool GetLockStatus(uint8_t& status_out);

    WinUsbDevice usb_;
    mutable std::mutex io_mu_;
    CmdCipher cipher_;

    mutable std::mutex xor_mu_;
    bool xor_enabled_ = false;
    XORCipher xor_cipher_;

    std::mutex bcas_mu_;
    uint8_t bcas_cycle_ = 0;

    FirmwareVersion fw_;
    std::string serial_;
};

} // namespace dyna

#endif // _WIN32
