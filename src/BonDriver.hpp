// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#pragma once

#ifdef _WIN32

#include "IBonDriver2.h"
#include "B25Decoder.hpp"
#include "DyudDevice.hpp"
#include "Settler.hpp"
#include "StreamAligner.hpp"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace dyna {

struct Config {
    int device_index = 0;
    bool enable_b25 = true;
    bool no_xor = false;
    bool no_settle = false;
    bool one_seg = false;
    uint32_t lock_timeout_ms = 3000;
};

class BonDriverDyna : public IBonDriver2 {
public:
    explicit BonDriverDyna(HMODULE module);
    ~BonDriverDyna();

    // IBonDriver
    const BOOL OpenTuner(void) override;
    void CloseTuner(void) override;

    const BOOL SetChannel(const BYTE bCh) override;
    const float GetSignalLevel(void) override;

    const DWORD WaitTsStream(const DWORD dwTimeOut = 0) override;
    const DWORD GetReadyCount(void) override;

    const BOOL GetTsStream(BYTE* pDst, DWORD* pdwSize, DWORD* pdwRemain) override;
    const BOOL GetTsStream(BYTE** ppDst, DWORD* pdwSize, DWORD* pdwRemain) override;

    void PurgeTsStream(void) override;

    // IBonDriver2
    LPCTSTR GetTunerName(void) override;
    const BOOL IsTunerOpening(void) override;

    LPCTSTR EnumTuningSpace(const DWORD dwSpace) override;
    LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel) override;

    const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel) override;

    const DWORD GetCurSpace(void) override;
    const DWORD GetCurChannel(void) override;

    void Release(void) override;

private:
    void LoadIni(HMODULE module);
    void OnRawChunk(const uint8_t* data, size_t len);

    Config cfg_;
    DyudDevice dev_;
    bool is_open_ = false;

    DWORD cur_space_ = 0;
    DWORD cur_channel_ = 0;
    std::wstring tuner_name_ = L"BonDriver_Dyna (DY-UD200)";
    std::wstring channel_name_buf_;

    // TS 処理（ストリームスレッド専用）
    XORCipher active_xor_;
    bool active_xor_enabled_ = false;
    Aligner aligner_;
    Settler settler_;
    B25Decoder b25_;
    bool b25_active_ = false;
    int last_resyncs_ = 0;

    // 出力キュー
    static constexpr size_t kMaxQueueChunks = 512;
    std::mutex queue_mu_;
    std::condition_variable queue_cv_;
    std::deque<std::vector<uint8_t>> queue_;
    std::vector<uint8_t> last_out_;
};

} // namespace dyna

#endif // _WIN32
