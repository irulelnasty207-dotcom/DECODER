#include "yurlaps_decoder/yurlaps.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace yurlaps::decoder {
namespace {
constexpr int STATES = 256;
constexpr int PAYLOAD_BITS = 40;
constexpr std::uint16_t POLY_A = 0x1af;
constexpr std::uint16_t POLY_B = 0x11d;

int parity(std::uint16_t value) {
    int result = 0;
    while (value) { result ^= static_cast<int>(value & 1u); value >>= 1u; }
    return result;
}

std::pair<int, int> encoded_pair(int state, int input) {
    const auto reg = static_cast<std::uint16_t>(((state << 1) | input) & 0x1ff);
    return {parity(reg & POLY_A), parity(reg & POLY_B)};
}

std::uint8_t crc8(std::uint32_t id, std::uint8_t sequence) {
    const std::array<std::uint8_t, 4> bytes{
        static_cast<std::uint8_t>(id >> 16u), static_cast<std::uint8_t>(id >> 8u),
        static_cast<std::uint8_t>(id), sequence};
    std::uint8_t crc = 0;
    for (const auto byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x80u) ? static_cast<std::uint8_t>((crc << 1u) ^ 0x07u)
                                : static_cast<std::uint8_t>(crc << 1u);
    }
    return crc;
}

std::array<bool, PAYLOAD_BITS> payload_bits(std::uint32_t id, std::uint8_t sequence) {
    const std::array<std::uint8_t, 5> bytes{
        static_cast<std::uint8_t>(id >> 16u), static_cast<std::uint8_t>(id >> 8u),
        static_cast<std::uint8_t>(id), sequence, crc8(id, sequence)};
    std::array<bool, PAYLOAD_BITS> bits{};
    for (std::size_t bit = 0; bit < PAYLOAD_BITS; ++bit)
        bits[bit] = (bytes[bit / 8] & (0x80u >> (bit % 8))) != 0;
    return bits;
}
}

std::array<std::uint8_t, YURLAPS_PAYLOAD_SYMBOLS> encode_yurlaps(
    std::uint32_t id, std::uint8_t sequence) {
    std::array<std::uint8_t, YURLAPS_PAYLOAD_SYMBOLS> result{};
    std::size_t cursor = 0;
    int state = 0;
    for (const bool bit : payload_bits(id, sequence)) {
        const auto pair = encoded_pair(state, bit ? 1 : 0);
        result[cursor++] = pair.first ? 255u : 0u;
        result[cursor++] = pair.second ? 255u : 0u;
        state = ((state << 1) | (bit ? 1 : 0)) & 0xff;
    }
    return result;
}

std::optional<YurlapsFrame> decode_yurlaps(
    const std::array<std::uint8_t, YURLAPS_PAYLOAD_SYMBOLS>& softbits) {
    std::array<std::array<int, STATES>, PAYLOAD_BITS + 1> metrics{};
    std::array<std::array<std::uint8_t, STATES>, PAYLOAD_BITS + 1> previous{};
    std::array<std::array<bool, STATES>, PAYLOAD_BITS + 1> decisions{};
    for (auto& row : metrics) row.fill(std::numeric_limits<int>::min() / 4);
    metrics[0][0] = 0;
    for (int step = 0; step < PAYLOAD_BITS; ++step) {
        for (int state = 0; state < STATES; ++state) {
            if (metrics[step][state] == std::numeric_limits<int>::min() / 4) continue;
            for (int input = 0; input <= 1; ++input) {
                const auto pair = encoded_pair(state, input);
                const int first = static_cast<int>(softbits[step * 2]) - 127;
                const int second = static_cast<int>(softbits[step * 2 + 1]) - 127;
                const int score = first * (pair.first ? 1 : -1)
                    + second * (pair.second ? 1 : -1);
                const int next = ((state << 1) | input) & 0xff;
                if (metrics[step][state] + score > metrics[step + 1][next]) {
                    metrics[step + 1][next] = metrics[step][state] + score;
                    previous[step + 1][next] = static_cast<std::uint8_t>(state);
                    decisions[step + 1][next] = input != 0;
                }
            }
        }
    }
    int terminal = 0;
    for (int state = 1; state < STATES; ++state)
        if (metrics[PAYLOAD_BITS][state] > metrics[PAYLOAD_BITS][terminal]) terminal = state;
    std::array<bool, PAYLOAD_BITS> decoded{};
    int state = terminal;
    for (int step = PAYLOAD_BITS; step > 0; --step) {
        decoded[step - 1] = decisions[step][state];
        state = previous[step][state];
    }
    std::array<std::uint8_t, 5> bytes{};
    for (int bit = 0; bit < PAYLOAD_BITS; ++bit)
        bytes[bit / 8] = static_cast<std::uint8_t>((bytes[bit / 8] << 1u)
                                                    | (decoded[bit] ? 1u : 0u));
    const std::uint32_t id = (static_cast<std::uint32_t>(bytes[0]) << 16u)
        | (static_cast<std::uint32_t>(bytes[1]) << 8u) | bytes[2];
    if (id == 0 || id > 0x00ffffffu || bytes[4] != crc8(id, bytes[3])) return std::nullopt;
    return YurlapsFrame{id, bytes[3]};
}

}  // namespace yurlaps::decoder
