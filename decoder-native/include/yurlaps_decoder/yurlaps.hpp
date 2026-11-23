#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace yurlaps::decoder {

struct YurlapsFrame {
    std::uint32_t id{};
    std::uint8_t sequence{};
};

constexpr std::size_t YURLAPS_PAYLOAD_SYMBOLS = 80;

// YurLaps carries a 24-bit ID, an 8-bit sequence, and an 8-bit CRC. K=9
// rate-1/2 coding produces 80 soft symbols. A distinct preamble identifies it.
// The input uses the same 0..255 confidence scale as the OPN decoder.
std::optional<YurlapsFrame> decode_yurlaps(
    const std::array<std::uint8_t, YURLAPS_PAYLOAD_SYMBOLS>& softbits);

// Encoder used by the firmware prototype and deterministic IQ fixtures.
std::array<std::uint8_t, YURLAPS_PAYLOAD_SYMBOLS> encode_yurlaps(
    std::uint32_t id, std::uint8_t sequence);

}  // namespace yurlaps::decoder
