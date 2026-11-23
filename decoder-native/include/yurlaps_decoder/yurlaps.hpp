#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace yurlaps::decoder {

struct YurlapsFrame {
    std::uint32_t id{};
};

constexpr std::size_t YURLAPS_INIT_SYMBOLS = 4;
constexpr std::size_t YURLAPS_PREAMBLE_SYMBOLS = 16;
constexpr std::size_t YURLAPS_PAYLOAD_BITS = 40;
constexpr std::size_t YURLAPS_PAYLOAD_SYMBOLS = YURLAPS_PAYLOAD_BITS * 2;
constexpr std::size_t YURLAPS_TAIL_SYMBOLS = 4;
constexpr std::size_t YURLAPS_FRAME_SYMBOLS = YURLAPS_INIT_SYMBOLS
    + YURLAPS_PREAMBLE_SYMBOLS + YURLAPS_PAYLOAD_SYMBOLS + YURLAPS_TAIL_SYMBOLS;
constexpr std::uint16_t YURLAPS_PREAMBLE = 0xf9a8;

// YLP-1 carries a 24-bit ID and CRC16. K=9 rate-1/2 tail-biting coding
// produces 80 soft symbols. The complete frame is 4 init + Barker-13 padded
// to a 16-symbol preamble + 80 payload + 4 tail symbols.
// The input uses the same 0..255 confidence scale as the OPN decoder.
std::optional<YurlapsFrame> decode_yurlaps(
    const std::array<std::uint8_t, YURLAPS_PAYLOAD_SYMBOLS>& softbits);

// Encoder used by the firmware prototype and deterministic IQ fixtures.
std::array<std::uint8_t, YURLAPS_PAYLOAD_SYMBOLS> encode_yurlaps(std::uint32_t id);

}  // namespace yurlaps::decoder
