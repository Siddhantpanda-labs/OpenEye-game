/**
 * @file fec.cpp
 */

#include "tienet/core/fec.hpp"
#include <algorithm>

namespace tienet {
namespace core {

std::vector<uint8_t> FECEncoder::xorParity(const std::vector<std::vector<uint8_t>>& packets) {
    size_t maxSize = 0;
    for (const auto& p : packets) maxSize = std::max(maxSize, p.size());
    std::vector<uint8_t> parity(maxSize, 0);
    for (const auto& p : packets) {
        for (size_t i = 0; i < p.size(); ++i) parity[i] ^= p[i];
    }
    return parity;
}

bool FECDecoder::recoverXor(
    const std::vector<std::vector<uint8_t>>& receivedPackets,
    const std::vector<uint8_t>& parity,
    size_t totalPackets,
    std::vector<uint8_t>& outRecovered
) {
    if (receivedPackets.size() + 1 != totalPackets) return false; // only one loss supported
    size_t maxSize = parity.size();
    outRecovered.assign(maxSize, 0);
    for (size_t i = 0; i < maxSize; ++i) {
        uint8_t v = parity[i];
        for (const auto& p : receivedPackets) {
            if (i < p.size()) v ^= p[i];
        }
        outRecovered[i] = v;
    }
    return true;
}

} // namespace core
} // namespace tienet


