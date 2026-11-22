// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#ifdef _WIN32

#include "WinUsbDevice.hpp"

#include <setupapi.h>
#include <usbiodef.h>

#include <algorithm>
#include <cwctype>
#include <memory>
#include <sstream>

namespace dyna {

namespace {

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

std::wstring FormatVidPid(uint16_t vid, uint16_t pid) {
    wchar_t buf[32];
    swprintf(buf, 32, L"vid_%04x&pid_%04x", vid, pid);
    return std::wstring(buf);
}

} // namespace

WinUsbDevice::~WinUsbDevice() {
    Close();
}

std::vector<std::wstring> WinUsbDevice::Enumerate(uint16_t vid, uint16_t pid) {
    std::vector<std::wstring> result;
    const std::wstring needle = FormatVidPid(vid, pid);

    HDEVINFO dev_info = SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_USB_DEVICE,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (dev_info == INVALID_HANDLE_VALUE) {
        return result;
    }

    SP_DEVICE_INTERFACE_DATA if_data{};
    if_data.cbSize = sizeof(if_data);
    for (DWORD idx = 0; SetupDiEnumDeviceInterfaces(dev_info, nullptr, &GUID_DEVINTERFACE_USB_DEVICE, idx, &if_data); ++idx) {
        DWORD req_size = 0;
        SetupDiGetDeviceInterfaceDetailW(dev_info, &if_data, nullptr, 0, &req_size, nullptr);
        if (req_size == 0) continue;

        std::vector<uint8_t> detail_buf(req_size);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detail_buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (SetupDiGetDeviceInterfaceDetailW(dev_info, &if_data, detail, req_size, nullptr, nullptr)) {
            std::wstring path(detail->DevicePath);
            if (ToLower(path).find(needle) != std::wstring::npos) {
                result.push_back(std::move(path));
            }
        }
    }

    SetupDiDestroyDeviceInfoList(dev_info);
    std::sort(result.begin(), result.end());
    return result;
}

bool WinUsbDevice::Open(const std::wstring& device_path, std::string* err) {
    Close();

    dev_handle_ = CreateFileW(
        device_path.c_str(),
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        nullptr);
    if (dev_handle_ == INVALID_HANDLE_VALUE) {
        if (err) *err = "CreateFileW failed";
        return false;
    }

    if (!WinUsb_Initialize(dev_handle_, &winusb_)) {
        CloseHandle(dev_handle_);
        dev_handle_ = INVALID_HANDLE_VALUE;
        winusb_ = nullptr;
        if (err) *err = "WinUsb_Initialize failed (is WinUSB driver installed via Zadig?)";
        return false;
    }
    return true;
}

void WinUsbDevice::Close() {
    StopAsyncStream();
    if (winusb_ != nullptr) {
        WinUsb_Free(winusb_);
        winusb_ = nullptr;
    }
    if (dev_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(dev_handle_);
        dev_handle_ = INVALID_HANDLE_VALUE;
    }
}

bool WinUsbDevice::BulkWrite(uint8_t ep, const uint8_t* data, size_t len, uint32_t timeout_ms, size_t* written) {
    if (!winusb_) return false;
    ULONG t = timeout_ms;
    WinUsb_SetPipePolicy(winusb_, ep, PIPE_TRANSFER_TIMEOUT, sizeof(t), &t);

    ULONG transferred = 0;
    BOOL ok = WinUsb_WritePipe(winusb_, ep, const_cast<uint8_t*>(data), static_cast<ULONG>(len), &transferred, nullptr);
    if (written) *written = transferred;
    return ok != FALSE;
}

bool WinUsbDevice::BulkRead(uint8_t ep, uint8_t* data, size_t len, uint32_t timeout_ms, size_t* read_len) {
    if (!winusb_) return false;
    ULONG t = timeout_ms;
    WinUsb_SetPipePolicy(winusb_, ep, PIPE_TRANSFER_TIMEOUT, sizeof(t), &t);

    ULONG transferred = 0;
    BOOL ok = WinUsb_ReadPipe(winusb_, ep, data, static_cast<ULONG>(len), &transferred, nullptr);
    if (read_len) *read_len = transferred;
    return ok != FALSE;
}

bool WinUsbDevice::StartAsyncStream(uint8_t ep, size_t num_urbs, size_t urb_size, ChunkCallback cb) {
    if (!winusb_) return false;
    StopAsyncStream();

    UCHAR raw_io = 1;
    WinUsb_SetPipePolicy(winusb_, ep, RAW_IO, sizeof(raw_io), &raw_io);
    WinUsb_FlushPipe(winusb_, ep);

    stream_ep_ = ep;
    streaming_.store(true);

    stream_thread_ = std::thread([this, ep, num_urbs, urb_size, cb = std::move(cb)]() {
        struct UrbSlot {
            std::vector<uint8_t> buf;
            OVERLAPPED ov{};
            bool pending = false;
        };

        std::vector<UrbSlot> slots(num_urbs);
        for (auto& s : slots) {
            s.buf.resize(urb_size);
            s.ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        }

        auto submit = [&](UrbSlot& s) {
            ResetEvent(s.ov.hEvent);
            ULONG got = 0;
            BOOL ok = WinUsb_ReadPipe(winusb_, ep, s.buf.data(), static_cast<ULONG>(s.buf.size()), &got, &s.ov);
            if (ok || GetLastError() == ERROR_IO_PENDING) {
                s.pending = true;
            } else {
                s.pending = false;
            }
        };

        for (auto& s : slots) {
            submit(s);
        }

        size_t idx = 0;
        while (streaming_.load()) {
            auto& s = slots[idx];
            idx = (idx + 1) % num_urbs;
            if (!s.pending) {
                submit(s);
                continue;
            }

            DWORD w = WaitForSingleObject(s.ov.hEvent, 200);
            if (w == WAIT_TIMEOUT) {
                continue;
            }
            if (w != WAIT_OBJECT_0) {
                break;
            }

            ULONG transferred = 0;
            BOOL ok = WinUsb_GetOverlappedResult(winusb_, &s.ov, &transferred, FALSE);
            s.pending = false;
            if (ok && transferred > 0 && streaming_.load()) {
                cb(s.buf.data(), transferred);
            }
            if (streaming_.load()) {
                submit(s);
            }
        }

        WinUsb_AbortPipe(winusb_, ep);
        for (auto& s : slots) {
            if (s.pending) {
                ULONG dummy = 0;
                WinUsb_GetOverlappedResult(winusb_, &s.ov, &dummy, TRUE);
                s.pending = false;
            }
            if (s.ov.hEvent) {
                CloseHandle(s.ov.hEvent);
                s.ov.hEvent = nullptr;
            }
        }
    });

    return true;
}

void WinUsbDevice::StopAsyncStream() {
    if (streaming_.exchange(false)) {
        if (winusb_ && stream_ep_ != 0) {
            WinUsb_AbortPipe(winusb_, stream_ep_);
        }
    }
    if (stream_thread_.joinable()) {
        stream_thread_.join();
    }
}

} // namespace dyna

#endif // _WIN32
