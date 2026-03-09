/**
 * @file fec.hpp
 * @brief Simple parity-based FEC for datagram groups
 */

#pragma once

#include <vector>
#include <cstdint>

namespace tienet {
namespace core {

class FECEncoder {
public:
    // Produces one parity packet (XOR across all data packets)
    static std::vector<uint8_t> xorParity(const std::vector<std::vector<uint8_t>>& packets);
};

class FECDecoder {
public:
    // Attempts to recover one lost packet using parity and remaining packets
    static bool recoverXor(
        const std::vector<std::vector<uint8_t>>& receivedPackets,
        const std::vector<uint8_t>& parity,
        size_t totalPackets,
        std::vector<uint8_t>& outRecovered
    );
};

} // namespace core
} // namespace tienet


