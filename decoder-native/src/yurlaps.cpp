#include "yurlaps_decoder/yurlaps.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace yurlaps::decoder {
namespace {
constexpr int STATES = 256;
constexpr int PAYLOAD_BITS = static_cast<int>(YURLAPS_PAYLOAD_BITS);
constexpr std::uint16_t POLY_A = 0x1af;
constexpr std::uint16_t POLY_B = 0x11d;
int parity(std::uint16_t value) {
    int result = 0;
    while (value != 0u) {
        result ^= static_cast<int>(value & 1u);
        value >>= 1u;
    }
    return result;
}

std::pair<int, int> encoded_pair(int state, int input) {
    const auto reg = static_cast<std::uint16_t>(((state << 1) | input) & 0x1ff);
    return {parity(reg & POLY_A), parity(reg & POLY_B)};
}

std::uint16_t crc16(std::uint32_t id) {
    const std::array<std::uint8_t, 3> bytes{
        static_cast<std::uint8_t>(id >> 16u),
        static_cast<std::uint8_t>(id >> 8u),
        static_cast<std::uint8_t>(id),
    };
    std::uint16_t crc = 0xffffu;
    for (const auto byte : bytes) {
        crc ^= static_cast<std::uint16_t>(byte) << 8u;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u)
                ? static_cast<std::uint16_t>((crc << 1u) ^ 0x1021u)
                : static_cast<std::uint16_t>(crc << 1u);
        }
    }
    return crc;
}

std::array<bool, PAYLOAD_BITS> payload_bits(std::uint32_t id) {
    const auto crc = crc16(id);
    const std::array<std::uint8_t, 5> bytes{
        static_cast<std::uint8_t>(id >> 16u),
        static_cast<std::uint8_t>(id >> 8u),
        static_cast<std::uint8_t>(id),
        static_cast<std::uint8_t>(crc >> 8u),
        static_cast<std::uint8_t>(crc),
    };
    std::array<bool, PAYLOAD_BITS> bits{};
    for (std::size_t index = 0; index < bits.size(); ++index) {
        bits[index] = (bytes[index / 8] & (0x80u >> (index % 8))) != 0u;
    }
    return bits;
}
}
std::array<std::uint8_t, YURLAPS_PAYLOAD_SYMBOLS> encode_yurlaps(std::uint32_t id) {
    std::array<std::uint8_t, YURLAPS_PAYLOAD_SYMBOLS> encoded{};
    const auto bits = payload_bits(id);
    int initial = 0;
    for (int state = 0; state < STATES; ++state) {
        int final_state = state;
        for (const bool bit : bits) {
            final_state = ((final_state << 1) | (bit ? 1 : 0)) & 0xff;
        }
        if (final_state == state) {
            initial = state;
            break;
        }
    }
    int state = initial;
    std::size_t position = 0;
    for (const bool bit : bits) {
        const auto pair = encoded_pair(state, bit ? 1 : 0);
        encoded[position++] = pair.first != 0 ? 255u : 0u;
        encoded[position++] = pair.second != 0 ? 255u : 0u;
        state = ((state << 1) | (bit ? 1 : 0)) & 0xff;
    }
    return encoded;
}

std::optional<YurlapsFrame> decode_yurlaps(
    const std::array<std::uint8_t, YURLAPS_PAYLOAD_SYMBOLS>& softbits) {
    constexpr int UNREACHABLE = std::numeric_limits<int>::min() / 4;
    int best_score = UNREACHABLE;
    std::array<bool, PAYLOAD_BITS> best_bits{};

    // Every possible starting state is tried because this is tail-biting FEC.
    // The winning path must end in the same state it started in.
    for (int initial = 0; initial < STATES; ++initial) {
        std::array<std::array<int, STATES>, PAYLOAD_BITS + 1> score{};
        std::array<std::array<std::uint8_t, STATES>, PAYLOAD_BITS + 1> previous{};
        std::array<std::array<bool, STATES>, PAYLOAD_BITS + 1> decoded{};
        for (auto& row : score) row.fill(UNREACHABLE);
        score[0][initial] = 0;

        for (int step = 0; step < PAYLOAD_BITS; ++step) {
            for (int state = 0; state < STATES; ++state) {
                if (score[step][state] == UNREACHABLE) continue;
                for (int input = 0; input <= 1; ++input) {
                    const auto pair = encoded_pair(state, input);
                    const int first = static_cast<int>(softbits[step * 2]) - 127;
                    const int second = static_cast<int>(softbits[step * 2 + 1]) - 127;
                    const int next = ((state << 1) | input) & 0xff;
                    const int branch = (pair.first != 0 ? first : -first)
                        + (pair.second != 0 ? second : -second);
                    const int candidate = score[step][state] + branch;
                    if (candidate > score[step + 1][next]) {
                        score[step + 1][next] = candidate;
                        previous[step + 1][next] = static_cast<std::uint8_t>(state);
                        decoded[step + 1][next] = input != 0;
                    }
                }
            }
        }

        if (score[PAYLOAD_BITS][initial] <= best_score) continue;
        best_score = score[PAYLOAD_BITS][initial];
        int state = initial;
        for (int step = PAYLOAD_BITS; step > 0; --step) {
            best_bits[step - 1] = decoded[step][state];
            state = previous[step][state];
        }
    }

    if (best_score == UNREACHABLE) return std::nullopt;
    std::array<std::uint8_t, 5> bytes{};
    for (int bit = 0; bit < PAYLOAD_BITS; ++bit) {
        bytes[bit / 8] = static_cast<std::uint8_t>(
            (bytes[bit / 8] << 1u) | (best_bits[bit] ? 1u : 0u));
    }
    const std::uint32_t id = (static_cast<std::uint32_t>(bytes[0]) << 16u)
        | (static_cast<std::uint32_t>(bytes[1]) << 8u) | bytes[2];
    const std::uint16_t received_crc = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[3]) << 8u) | bytes[4]);
    if (id == 0u || id > 0x00ffffffu || received_crc != crc16(id)) {
        return std::nullopt;
    }
    return YurlapsFrame{id};
}
} // namespace yurlaps::decoder
