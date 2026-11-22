// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125
#include "StreamAligner.hpp"

#include <algorithm>

namespace dyna {

Aligner::Aligner(const XORCipher* xor_cipher) : xor_(xor_cipher) {}

void Aligner::Reset(const XORCipher* xor_cipher) {
    stale_ = kStaleDiscardBytes;
    rest_.clear();
    xor_ = xor_cipher;
    resyncs_ = 0;
}

std::vector<uint8_t> Aligner::Feed(const uint8_t* chunk, size_t len) {
    if (stale_ > 0) {
        const size_t drop = std::min(len, stale_);
        stale_ -= drop;
        chunk += drop;
        len -= drop;
        if (len == 0) return {};
    }

    std::vector<uint8_t> buf;
    buf.reserve(rest_.size() + len);
    buf.insert(buf.end(), rest_.begin(), rest_.end());
    buf.insert(buf.end(), chunk, chunk + len);

    const size_t size = buf.size();
    std::vector<uint8_t> out;
    out.reserve((size / kPacketSize) * kPacketSize);

    size_t pos = 0;
    while (size - pos >= kPacketSize) {
        if (buf[pos] != 0x47) {
            if (size - pos <= kPacketSize * 3) {
                break;
            }
            const size_t start = pos;
            bool found = false;
            while (pos + kPacketSize * 3 < size) {
                if (buf[pos] == 0x47 &&
                    buf[pos + kPacketSize] == 0x47 &&
                    buf[pos + kPacketSize * 2] == 0x47 &&
                    buf[pos + kPacketSize * 3] == 0x47) {
                    found = true;
                    break;
                }
                ++pos;
            }
            if (pos != start) {
                ++resyncs_;
            }
            if (!found) {
                break;
            }
        }

        const size_t pkt_start = out.size();
        out.insert(out.end(), buf.begin() + static_cast<ptrdiff_t>(pos),
                   buf.begin() + static_cast<ptrdiff_t>(pos + kPacketSize));
        if (xor_ != nullptr) {
            xor_->DecryptPacket(out.data() + pkt_start, kPacketSize);
        }
        pos += kPacketSize;
    }

    rest_.assign(buf.begin() + static_cast<ptrdiff_t>(pos), buf.end());
    return out;
}

} // namespace dyna
