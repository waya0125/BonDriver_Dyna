// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dyna {

// DY-UD200 制御コマンド初期化・応答復号用の既定 AES-256 鍵
extern const std::array<uint8_t, 32> kDefaultAESKey;

// SetTSXorKey コマンドのペイロード暗号化用既定 XOR 鍵
extern const std::array<uint8_t, 32> kDefaultXORKey;

// 選局時に設定する 32 バイトのセッション XOR 鍵
extern const std::array<uint8_t, 32> kSessionXORKey;

// IEEE CRC32
uint32_t Crc32IEEE(const uint8_t* data, size_t len);

// 最小限の AES-128 / AES-256 ECB 実装（外部ライブラリ不要）
class AesBlock {
public:
    AesBlock() = default;
    void SetKey128(const uint8_t key[16]);
    void SetKey256(const uint8_t key[32]);

    void EncryptBlock(const uint8_t in[16], uint8_t out[16]) const;
    void DecryptBlock(const uint8_t in[16], uint8_t out[16]) const;

private:
    int nr_ = 0;
    uint32_t enc_rk_[60]{};
    uint32_t dec_rk_[60]{};
};

// DY-UD200 の USB 制御コマンド（132B 送信 / 128B 受信）の AES-ECB + CRC32 変換
class CmdCipher {
public:
    CmdCipher();

    bool EncryptCommand(const uint8_t* cmd, size_t len, std::array<uint8_t, 132>& out, std::string* err = nullptr);
    bool DecryptResponse(const uint8_t* enc, size_t len, std::array<uint8_t, 128>& out, std::string* err = nullptr) const;

private:
    AesBlock default_block_; // AES-256
    AesBlock cmd_block_;     // AES-128
};

// DY-UD200 の TS パケット XOR 変換（dyud_xor.cpp 互換）
class XORCipher {
public:
    XORCipher() = default;

    void SetKey(const uint8_t* key, size_t len);
    void Crypt(uint8_t* dst, const uint8_t* src, size_t len) const;
    void DecryptPacket(uint8_t* pkt, size_t len) const;

private:
    std::array<uint8_t, 514> lut_{};
};

// チャンネル番号から周波数（kHz）へ変換する
// UHF: 13〜62, VHF: 1〜12, CATV (catv=true): C13〜C63
bool ChannelToFrequencyKHz(int ch, bool catv, uint32_t& freq_khz);

} // namespace dyna
