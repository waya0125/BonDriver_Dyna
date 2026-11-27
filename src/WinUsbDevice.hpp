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

    // DEVICE_SPEED（WinUsb_QueryDeviceInformation）。LowSpeed=1, FullSpeed=2, HighSpeed=3。
    // WinUSB は Full/Low を区別せず LowSpeed を返すことがあり、SuperSpeed 接続も HighSpeed として返る。
    bool QueryDeviceSpeed(uint8_t& speed_out) const;

    struct PipeInfo {
        uint8_t pipe_id = 0;
        int pipe_type = 0;                  // USBD_PIPE_TYPE（0=Control, 1=Isochronous, 2=Bulk, 3=Interrupt）
        uint16_t max_packet_size = 0;
        uint8_t interval = 0;
        uint32_t max_bytes_per_interval = 0; // WinUsb_QueryPipeEx が使えたときのみ有効
        bool from_ex = false;                // WinUsb_QueryPipeEx で取得したか
    };
    // 代替設定 0 のエンドポイント ep の情報を取得する。
    // WinUsb_QueryPipeEx（Windows 8.1 以降）は動的に解決し、無い環境では WinUsb_QueryPipe を使う。
    bool QueryPipeInfo(uint8_t ep, PipeInfo& out) const;

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
