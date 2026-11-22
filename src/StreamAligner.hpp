// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#pragma once

#include "Crypto.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dyna {

constexpr size_t kPacketSize = 188;
constexpr size_t kStaleDiscardBytes = 2048 * kPacketSize;

class Aligner {
public:
    explicit Aligner(const XORCipher* xor_cipher = nullptr);

    void Reset(const XORCipher* xor_cipher);
    std::vector<uint8_t> Feed(const uint8_t* chunk, size_t len);

    int Resyncs() const { return resyncs_; }

private:
    size_t stale_ = kStaleDiscardBytes;
    std::vector<uint8_t> rest_;
    const XORCipher* xor_ = nullptr;
    int resyncs_ = 0;
};

} // namespace dyna
