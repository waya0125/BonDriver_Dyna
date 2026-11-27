// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#pragma once

#include <cstddef>
#include <cstdint>
#include <cwchar>

namespace dyna {

// INI の DeviceIndex を解釈する。
//   0 以上 : 列挙順（デバイスパスのソート順）のインデックスを固定で開く
//   負の値 : 空きデバイス自動選択（-1 を推奨）
//   空/不正: def を返す
inline int ParseDeviceIndex(const wchar_t* s, int def) {
    if (s == nullptr) return def;
    while (*s == L' ' || *s == L'\t') ++s;
    if (*s == L'\0') return def;
    wchar_t* end = nullptr;
    const long v = std::wcstol(s, &end, 10);
    if (end == s) return def;
    if (v < 0) return -1;
    if (v > 255) return def;
    return static_cast<int>(v);
}

// Bulk IN の 1 回あたりの転送サイズを決める。
// TS パケット（188 バイト）とエンドポイントの MaximumPacketSize の両方の倍数で、
// target 以下の最大値を返す（最低でも両者の最小公倍数 1 個分）。
// max_packet が 0（取得失敗）のときは target をそのまま返す。
inline size_t ChooseStreamUrbSize(uint32_t max_packet, size_t target) {
    constexpr size_t kTsPacket = 188;
    if (max_packet == 0) return target;
    size_t a = kTsPacket, b = max_packet;
    while (b != 0) {
        const size_t t = a % b;
        a = b;
        b = t;
    }
    const size_t lcm = kTsPacket / a * max_packet;
    if (lcm >= target) return lcm;
    return target / lcm * lcm;
}

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
    // 既定の Bulk 転送サイズ。実際の値は Open 時に取得した MaximumPacketSize から
    // ChooseStreamUrbSize で決める（HighSpeed/512 バイトならこの値のまま）。
    static constexpr size_t kStreamUrbSize = 24064; // 47 * 512 = 128 * 188

    DyudDevice() = default;
    ~DyudDevice() { Close(); }

    // device_index >= 0: 列挙インデックス固定。使用中なら失敗する。
    // device_index <  0: 列挙順に開いてみて、最初に開けた（他で使われていない）デバイスを使う。
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

    // Open 時に WinUSB から取得した接続速度と TS 用パイプの情報
    struct LinkInfo {
        int opened_index = -1;
        bool speed_ok = false;
        uint8_t speed = 0;       // WinUSB の DEVICE_SPEED 値（LowSpeed=1, FullSpeed=2, HighSpeed=3）
        bool pipe_ok = false;
        WinUsbDevice::PipeInfo ts_pipe;
        size_t stream_urb_size = kStreamUrbSize;
    };
    const LinkInfo& Link() const { return link_; }

private:
    void QueryLinkInfo();
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
    LinkInfo link_;
};

} // namespace dyna

#endif // _WIN32
