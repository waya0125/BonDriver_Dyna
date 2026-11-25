// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#ifdef _WIN32

#include "DyudDevice.hpp"

#include <chrono>
#include <cstring>
#include <thread>

namespace dyna {

namespace {

constexpr uint8_t kCmdInit[] = {
    0x0f, 0xf0, 0xe0, 0x20, 0x00, 0x00, 0x00, 0x00, 0x23, 0x8d, 0xc0, 0xd0, 0x0e, 0xd0, 0x38, 0x75,
    0xe8, 0x38, 0xdf, 0xeb, 0x33, 0x7d, 0x39, 0x52, 0xe3, 0x78, 0xeb, 0xd1, 0xb7, 0x84, 0xef, 0x65,
    0x00, 0x9a,
};
constexpr uint8_t kCmdInitNew[] = {
    0x0f, 0xf0, 0xe0, 0x20, 0x00, 0x00, 0x00, 0x00, 0x7b, 0x36, 0x51, 0xe5, 0x21, 0x7c, 0x7b, 0x1c,
    0xe8, 0x38, 0xdf, 0xeb, 0x33, 0x7d, 0x39, 0x52, 0xe3, 0x78, 0xeb, 0xd1, 0xb7, 0x84, 0xef, 0x65,
    0x00, 0x00,
};
constexpr uint8_t kCmdInitNF[] = {0x1f, 0xf0, 0xe3, 0x04, 0x00, 0x00, 0x00, 0x00};
constexpr uint8_t kCmdClose[]  = {0x1f, 0xf0, 0xe2, 0x04, 0x00, 0x00, 0x00, 0x00};
constexpr uint8_t kCmdFWVer[]  = {0x1f, 0xf0, 0x9f, 0x04, 0x00, 0x00, 0x00, 0x00};
constexpr uint8_t kCmdSerial[] = {0x1f, 0xf0, 0x95, 0x04, 0x00, 0x00, 0x00, 0x00};
constexpr uint8_t kCmdLock[]   = {0x1f, 0xf0, 0xb1, 0x04, 0x26, 0x0a, 0x00, 0x00};
constexpr uint8_t kCmdSigLv[]  = {0x1f, 0xf0, 0xd5, 0x04, 0x00, 0x00, 0x00, 0x00};
constexpr uint8_t kCmdTSOn[]   = {0x1f, 0xf0, 0xb4, 0x04, 0x00, 0x00, 0x00, 0x00};
constexpr uint8_t kCmdTSOff[]  = {0x1f, 0xf0, 0xb5, 0x04, 0x00, 0x00, 0x00, 0x00};

inline void PutBE32(uint8_t* p, uint32_t v) {
    p[0] = uint8_t(v >> 24);
    p[1] = uint8_t(v >> 16);
    p[2] = uint8_t(v >> 8);
    p[3] = uint8_t(v);
}

} // namespace

bool DyudDevice::Open(int device_index, std::string* err) {
    auto paths = WinUsbDevice::Enumerate(kDyudVendorId, kDyudProductId);
    if (paths.empty()) {
        if (err) *err = "DY-UD200 not found";
        return false;
    }
    if (device_index < 0 || static_cast<size_t>(device_index) >= paths.size()) {
        if (err) *err = "device index out of range";
        return false;
    }
    return usb_.Open(paths[static_cast<size_t>(device_index)], err);
}

bool DyudDevice::SendCommand(const uint8_t* cmd, size_t cmd_len, std::vector<uint8_t>& res_out, size_t res_len, std::string* err) {
    std::lock_guard<std::mutex> lk(io_mu_);

    std::array<uint8_t, 132> enc{};
    if (!cipher_.EncryptCommand(cmd, cmd_len, enc, err)) {
        return false;
    }

    size_t written = 0;
    if (!usb_.BulkWrite(kEpCmdOut, enc.data(), enc.size(), kBulkTimeoutMs, &written) || written != enc.size()) {
        if (err) *err = "BulkWrite failed";
        return false;
    }

    std::array<uint8_t, 128> res_enc{};
    bool recv_ok = false;
    for (int retry = 0; retry < 10; ++retry) {
        size_t rn = 0;
        if (usb_.BulkRead(kEpResIn, res_enc.data(), res_enc.size(), kBulkTimeoutMs, &rn) && rn == res_enc.size()) {
            recv_ok = true;
            break;
        }
        if (retry < 9) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    if (!recv_ok) {
        if (err) *err = "BulkRead response failed";
        return false;
    }

    std::array<uint8_t, 128> plain{};
    if (!cipher_.DecryptResponse(res_enc.data(), res_enc.size(), plain, err)) {
        return false;
    }
    if (res_len == 0 || res_len > plain.size()) {
        res_len = plain.size();
    }
    res_out.assign(plain.begin(), plain.begin() + static_cast<ptrdiff_t>(res_len));
    return true;
}

bool DyudDevice::Init(std::string* err) {
    std::vector<uint8_t> res;
    if (!SendCommand(kCmdInit, sizeof(kCmdInit), res, 128, err)) return false;
    if (!StopTS()) return false;
    if (!SendCommand(kCmdInitNF, sizeof(kCmdInitNF), res, 2, err)) return false;

    if (!GetFirmwareVersion(fw_)) return false;

    // FW 1.0.5-2 / 1.0.5-3 は新ファーム用の初期化列でやり直す
    if (fw_.NeedsNewInit()) {
        if (!SendCommand(kCmdClose, sizeof(kCmdClose), res, 2, err)) return false;
        if (!SendCommand(kCmdInitNew, sizeof(kCmdInitNew), res, 128, err)) return false;
        if (!StopTS()) return false;
        if (!SendCommand(kCmdInitNF, sizeof(kCmdInitNF), res, 2, err)) return false;
    }

    if (!GetSerial(serial_)) return false;
    if (!SetSegMode(kSegModeFull)) return false;
    return true;
}

bool DyudDevice::GetFirmwareVersion(FirmwareVersion& out) {
    std::vector<uint8_t> res;
    if (!SendCommand(kCmdFWVer, sizeof(kCmdFWVer), res, 6)) return false;
    out.major = res[2];
    out.minor1 = res[3];
    out.minor2 = res[4];
    out.minor3 = res[5];
    return true;
}

bool DyudDevice::GetSerial(std::string& out) {
    std::vector<uint8_t> res;
    if (!SendCommand(kCmdSerial, sizeof(kCmdSerial), res, 18)) return false;
    std::string s;
    for (size_t i = 2; i < 18; ++i) {
        if (res[i] == 0) break;
        if (res[i] >= 0x20 && res[i] <= 0x7e) {
            s.push_back(static_cast<char>(res[i]));
        }
    }
    out = s;
    return true;
}

bool DyudDevice::SetSegMode(uint32_t mode) {
    uint8_t cmd[8] = {0x1f, 0xf0, 0xd1, 0x04, 0, 0, 0, 0};
    PutBE32(cmd + 4, mode);
    std::vector<uint8_t> res;
    return SendCommand(cmd, sizeof(cmd), res, 1) && !res.empty() && res[0] == 0;
}

bool DyudDevice::SetTSXorKey(const uint8_t* key, size_t len) {
    if (len > 32) len = 32;
    uint8_t cmd[36]{};
    cmd[0] = 0x1f;
    cmd[1] = 0xf0;
    cmd[2] = 0x30;
    cmd[3] = 0x20;

    XORCipher tmp;
    tmp.SetKey(kDefaultXORKey.data(), kDefaultXORKey.size());
    tmp.Crypt(cmd + 4, key, len);

    std::vector<uint8_t> res;
    if (!SendCommand(cmd, sizeof(cmd), res, 1) || res.empty() || res[0] != 0) {
        return false;
    }

    std::lock_guard<std::mutex> lk(xor_mu_);
    xor_cipher_.SetKey(key, len);
    xor_enabled_ = true;
    return true;
}

bool DyudDevice::Tune(uint32_t freq_khz, bool one_seg, bool enable_xor, std::string* err) {
    if (!StopTS()) return false;
    if (!SetSegMode(one_seg ? kSegModeOne : kSegModeFull)) return false;

    if (enable_xor) {
        if (!SetTSXorKey(kSessionXORKey.data(), kSessionXORKey.size())) return false;
    } else {
        std::lock_guard<std::mutex> lk(xor_mu_);
        xor_enabled_ = false;
    }

    uint8_t cmd[8] = {0x1f, 0xf0, 0xd2, 0x06, 0, 0, 0, 0};
    PutBE32(cmd + 4, freq_khz);
    std::vector<uint8_t> res;
    if (!SendCommand(cmd, sizeof(cmd), res, 1, err) || res.empty() || res[0] != 0) {
        return false;
    }
    return StartTS();
}

bool DyudDevice::GetLockStatus(uint8_t& status_out) {
    std::vector<uint8_t> res;
    if (!SendCommand(kCmdLock, sizeof(kCmdLock), res, 4) || res.size() < 4 || res[0] != 0) {
        return false;
    }
    status_out = res[3];
    return true;
}

bool DyudDevice::WaitLock(uint32_t timeout_ms, uint8_t* last_status) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    uint8_t last = 0;
    while (true) {
        uint8_t st = 0;
        if (GetLockStatus(st)) {
            last = st;
            if (st > 8) {
                if (last_status) *last_status = st;
                return true;
            }
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            if (last_status) *last_status = last;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool DyudDevice::GetSignalLevel(float& level_out) {
    std::vector<uint8_t> res;
    if (!SendCommand(kCmdSigLv, sizeof(kCmdSigLv), res, 6) || res.size() < 6 || res[0] != 0) {
        return false;
    }
    if (res[2] == 0) {
        level_out = 0.0f;
        return true;
    }
    const uint32_t val = (uint32_t(res[3]) << 16) | (uint32_t(res[4]) << 8) | uint32_t(res[5]);
    level_out = static_cast<float>(double(val) / 65536.0 * 20.0);
    return true;
}

bool DyudDevice::StartTS() {
    std::vector<uint8_t> res;
    return SendCommand(kCmdTSOn, sizeof(kCmdTSOn), res, 1) && !res.empty() && res[0] == 0;
}

bool DyudDevice::StopTS() {
    std::vector<uint8_t> res;
    return SendCommand(kCmdTSOff, sizeof(kCmdTSOff), res, 1) && !res.empty() && res[0] == 0;
}

bool DyudDevice::StartStream(WinUsbDevice::ChunkCallback cb) {
    return usb_.StartAsyncStream(kEpTsIn, kStreamUrbCount, kStreamUrbSize, std::move(cb));
}

void DyudDevice::StopStream() {
    usb_.StopAsyncStream();
}

bool DyudDevice::ResetBcas() {
    std::lock_guard<std::mutex> lk(bcas_mu_);
    bcas_cycle_ = 0;
    constexpr uint8_t cmdA[] = {0x1f, 0xf0, 0xf0, 0x05, 0x02, 0x00, 0x42, 0x00, 0x00};
    constexpr uint8_t cmdB[] = {0x1f, 0xf0, 0xf0, 0x05, 0x02, 0x00, 0x41, 0x00, 0x00};
    std::vector<uint8_t> res;
    if (!SendCommand(cmdA, sizeof(cmdA), res, 128)) return false;
    if (!SendCommand(cmdB, sizeof(cmdB), res, 128)) return false;
    return true;
}

bool DyudDevice::TransmitBcas(const uint8_t* apdu, size_t len, std::vector<uint8_t>& resp_out) {
    std::lock_guard<std::mutex> lk(bcas_mu_);
    if (len == 0 || len > 115) return false;

    uint8_t cmd[128]{};
    cmd[0] = 0x1f;
    cmd[1] = 0xf0;
    cmd[2] = 0xf0;
    cmd[3] = static_cast<uint8_t>(len + 9);
    cmd[4] = 0x02;
    cmd[5] = 0x00;
    cmd[6] = 0x31;
    cmd[7] = 0x00;
    cmd[8] = static_cast<uint8_t>(len + 4);
    cmd[9] = 0x00;
    cmd[11] = static_cast<uint8_t>(len);
    std::memcpy(cmd + 12, apdu, len);

    for (int attempt = 0; attempt <= 3; ++attempt) {
        cmd[10] = bcas_cycle_;
        bcas_cycle_ ^= 0x40;

        uint8_t parity = 0;
        for (size_t i = 10; i < 12 + len; ++i) parity ^= cmd[i];
        cmd[12 + len] = parity;

        std::vector<uint8_t> res;
        if (!SendCommand(cmd, 13 + len, res, 128)) continue;
        if (res.size() < 7 || res[0] != 0 || res[1] < 6) continue;
        const size_t resp_len = res[6];
        if (7 + resp_len > res.size()) continue;
        resp_out.assign(res.begin() + 7, res.begin() + 7 + static_cast<ptrdiff_t>(resp_len));
        return true;
    }
    return false;
}

bool DyudDevice::GetXorCipherCopy(XORCipher& out) const {
    std::lock_guard<std::mutex> lk(xor_mu_);
    if (!xor_enabled_) return false;
    out = xor_cipher_;
    return true;
}

void DyudDevice::Close() {
    if (!usb_.IsOpen()) return;
    StopStream();
    StopTS();
    std::vector<uint8_t> res;
    SendCommand(kCmdClose, sizeof(kCmdClose), res, 2);
    usb_.Close();
}

} // namespace dyna

#endif // _WIN32
