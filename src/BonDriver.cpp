// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#ifdef _WIN32

#include "BonDriver.hpp"

#include <chrono>
#include <cstring>

namespace dyna {

namespace {

constexpr DWORD kSpaceUHF = 0;
constexpr DWORD kSpaceCATV = 1;
constexpr DWORD kSpaceVHF = 2;

bool SpaceChannelToParams(DWORD space, DWORD ch_idx, int& ch_num, bool& catv) {
    if (space == kSpaceUHF && ch_idx < 50) {
        ch_num = static_cast<int>(13 + ch_idx);
        catv = false;
        return true;
    }
    if (space == kSpaceCATV && ch_idx < 51) {
        ch_num = static_cast<int>(13 + ch_idx);
        catv = true;
        return true;
    }
    if (space == kSpaceVHF && ch_idx < 12) {
        ch_num = static_cast<int>(1 + ch_idx);
        catv = false;
        return true;
    }
    return false;
}

} // namespace

BonDriverDyna::BonDriverDyna(HMODULE module) {
    LoadIni(module);
}

BonDriverDyna::~BonDriverDyna() {
    CloseTuner();
}

void BonDriverDyna::LoadIni(HMODULE module) {
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(module, path, MAX_PATH)) {
        return;
    }
    std::wstring ini_path(path);
    const size_t dot = ini_path.rfind(L'.');
    if (dot != std::wstring::npos) {
        ini_path.replace(dot, std::wstring::npos, L".ini");
    } else {
        ini_path += L".ini";
    }

    cfg_.device_index = GetPrivateProfileIntW(L"DYUD200", L"DeviceIndex", 0, ini_path.c_str());
    cfg_.no_xor = GetPrivateProfileIntW(L"DYUD200", L"NoXor", 0, ini_path.c_str()) != 0;
    cfg_.no_settle = GetPrivateProfileIntW(L"DYUD200", L"NoSettle", 0, ini_path.c_str()) != 0;
    cfg_.one_seg = GetPrivateProfileIntW(L"DYUD200", L"OneSeg", 0, ini_path.c_str()) != 0;
    cfg_.lock_timeout_ms = static_cast<uint32_t>(GetPrivateProfileIntW(L"DYUD200", L"LockTimeoutMs", 3000, ini_path.c_str()));
}

const BOOL BonDriverDyna::OpenTuner(void) {
    if (is_open_) return TRUE;
    if (!dev_.Open(cfg_.device_index)) return FALSE;
    if (!dev_.Init()) {
        dev_.Close();
        return FALSE;
    }
    is_open_ = true;
    return TRUE;
}

void BonDriverDyna::CloseTuner(void) {
    if (!is_open_) return;
    dev_.Close();
    PurgeTsStream();
    is_open_ = false;
}

const BOOL BonDriverDyna::SetChannel(const BYTE bCh) {
    return SetChannel(kSpaceUHF, bCh);
}

const BOOL BonDriverDyna::SetChannel(const DWORD dwSpace, const DWORD dwChannel) {
    if (!is_open_) return FALSE;

    int ch_num = 0;
    bool catv = false;
    if (!SpaceChannelToParams(dwSpace, dwChannel, ch_num, catv)) {
        return FALSE;
    }
    uint32_t freq_khz = 0;
    if (!ChannelToFrequencyKHz(ch_num, catv, freq_khz)) {
        return FALSE;
    }

    dev_.StopStream();
    PurgeTsStream();

    if (!dev_.Tune(freq_khz, cfg_.one_seg, !cfg_.no_xor)) {
        return FALSE;
    }
    dev_.WaitLock(cfg_.lock_timeout_ms);

    active_xor_enabled_ = dev_.GetXorCipherCopy(active_xor_);
    aligner_.Reset(active_xor_enabled_ ? &active_xor_ : nullptr);
    settler_.Reset();
    last_resyncs_ = 0;

    if (!dev_.StartStream([this](const uint8_t* data, size_t len) { OnRawChunk(data, len); })) {
        return FALSE;
    }

    cur_space_ = dwSpace;
    cur_channel_ = dwChannel;
    return TRUE;
}

void BonDriverDyna::OnRawChunk(const uint8_t* data, size_t len) {
    auto packets = aligner_.Feed(data, len);
    if (!cfg_.no_settle) {
        const int cur_resyncs = aligner_.Resyncs();
        if (cur_resyncs != last_resyncs_) {
            settler_.ResetRun();
            last_resyncs_ = cur_resyncs;
        }
        if (!packets.empty()) {
            packets = settler_.Feed(packets.data(), packets.size());
        }
    }
    if (packets.empty()) return;

    std::lock_guard<std::mutex> lk(queue_mu_);
    while (queue_.size() >= kMaxQueueChunks) {
        queue_.pop_front();
    }
    queue_.push_back(std::move(packets));
    queue_cv_.notify_one();
}

const float BonDriverDyna::GetSignalLevel(void) {
    if (!is_open_) return 0.0f;
    float lv = 0.0f;
    if (!dev_.GetSignalLevel(lv)) return 0.0f;
    return lv;
}

const DWORD BonDriverDyna::WaitTsStream(const DWORD dwTimeOut) {
    std::unique_lock<std::mutex> lk(queue_mu_);
    if (!queue_.empty()) return WAIT_OBJECT_0;
    if (dwTimeOut == 0) return WAIT_TIMEOUT;
    if (dwTimeOut == INFINITE) {
        queue_cv_.wait(lk, [this]() { return !queue_.empty() || !is_open_; });
        return !queue_.empty() ? WAIT_OBJECT_0 : WAIT_ABANDONED;
    }
    const bool ready = queue_cv_.wait_for(lk, std::chrono::milliseconds(dwTimeOut), [this]() {
        return !queue_.empty() || !is_open_;
    });
    return (ready && !queue_.empty()) ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
}

const DWORD BonDriverDyna::GetReadyCount(void) {
    std::lock_guard<std::mutex> lk(queue_mu_);
    return static_cast<DWORD>(queue_.size());
}

const BOOL BonDriverDyna::GetTsStream(BYTE* pDst, DWORD* pdwSize, DWORD* pdwRemain) {
    BYTE* src = nullptr;
    if (!GetTsStream(&src, pdwSize, pdwRemain)) return FALSE;
    if (*pdwSize > 0 && pDst != nullptr && src != nullptr) {
        std::memcpy(pDst, src, *pdwSize);
    }
    return TRUE;
}

const BOOL BonDriverDyna::GetTsStream(BYTE** ppDst, DWORD* pdwSize, DWORD* pdwRemain) {
    std::lock_guard<std::mutex> lk(queue_mu_);
    if (queue_.empty()) {
        if (pdwSize) *pdwSize = 0;
        if (pdwRemain) *pdwRemain = 0;
        return FALSE;
    }
    last_out_ = std::move(queue_.front());
    queue_.pop_front();
    if (ppDst) *ppDst = last_out_.data();
    if (pdwSize) *pdwSize = static_cast<DWORD>(last_out_.size());
    if (pdwRemain) *pdwRemain = static_cast<DWORD>(queue_.size());
    return TRUE;
}

void BonDriverDyna::PurgeTsStream(void) {
    std::lock_guard<std::mutex> lk(queue_mu_);
    queue_.clear();
}

LPCTSTR BonDriverDyna::GetTunerName(void) {
    return tuner_name_.c_str();
}

const BOOL BonDriverDyna::IsTunerOpening(void) {
    return is_open_ ? TRUE : FALSE;
}

LPCTSTR BonDriverDyna::EnumTuningSpace(const DWORD dwSpace) {
    switch (dwSpace) {
    case kSpaceUHF: return L"UHF";
    case kSpaceCATV: return L"CATV";
    case kSpaceVHF: return L"VHF";
    default: return nullptr;
    }
}

LPCTSTR BonDriverDyna::EnumChannelName(const DWORD dwSpace, const DWORD dwChannel) {
    int ch_num = 0;
    bool catv = false;
    if (!SpaceChannelToParams(dwSpace, dwChannel, ch_num, catv)) {
        return nullptr;
    }
    wchar_t buf[32];
    if (catv) {
        swprintf(buf, 32, L"C%dch", ch_num);
    } else {
        swprintf(buf, 32, L"%dch", ch_num);
    }
    channel_name_buf_ = buf;
    return channel_name_buf_.c_str();
}

const DWORD BonDriverDyna::GetCurSpace(void) {
    return cur_space_;
}

const DWORD BonDriverDyna::GetCurChannel(void) {
    return cur_channel_;
}

void BonDriverDyna::Release(void) {
    delete this;
}

} // namespace dyna

#endif // _WIN32
