// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#pragma once

#ifdef _WIN32

#include <windows.h>
#include <winusb.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace dyna {

constexpr uint16_t kDyudVendorId = 0x1c11;
constexpr uint16_t kDyudProductId = 0x1004;

// WinUSB 直接呼び出しによる USB デバイスラッパー（依存 DLL なし）
class WinUsbDevice {
public:
    WinUsbDevice() = default;
    ~WinUsbDevice();

    WinUsbDevice(const WinUsbDevice&) = delete;
    WinUsbDevice& operator=(const WinUsbDevice&) = delete;

    static std::vector<std::wstring> Enumerate(uint16_t vid, uint16_t pid);

    bool Open(const std::wstring& device_path, std::string* err = nullptr);
    void Close();
    bool IsOpen() const { return winusb_ != nullptr; }

    bool BulkWrite(uint8_t ep, const uint8_t* data, size_t len, uint32_t timeout_ms, size_t* written = nullptr);
    bool BulkRead(uint8_t ep, uint8_t* data, size_t len, uint32_t timeout_ms, size_t* read_len = nullptr);

    using ChunkCallback = std::function<void(const uint8_t* data, size_t len)>;
    bool StartAsyncStream(uint8_t ep, size_t num_urbs, size_t urb_size, ChunkCallback cb);
    void StopAsyncStream();

private:
    HANDLE dev_handle_ = INVALID_HANDLE_VALUE;
    WINUSB_INTERFACE_HANDLE winusb_ = nullptr;

    std::atomic<bool> streaming_{false};
    std::thread stream_thread_;
    uint8_t stream_ep_ = 0;
};

} // namespace dyna

#endif // _WIN32
