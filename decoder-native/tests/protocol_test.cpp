#include "yurlaps_decoder/protocol.hpp"
#include "yurlaps_decoder/opn.hpp"
#include "yurlaps_decoder/yurlaps.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <array>

using namespace yurlaps::decoder;

int main() {
    // Build one clean OPN frame using the documented K=9 rate-1/2 code and
    // verify that the independent decoder recovers its seven-digit id.
    constexpr std::uint16_t poly_a = 0x1af;
    constexpr std::uint16_t poly_b = 0x11d;
    const auto parity = [](std::uint16_t value) {
        int result = 0;
        while (value) { result ^= static_cast<int>(value & 1u); value >>= 1u; }
        return result;
    };
    const std::uint32_t id = 1234567;
    std::array<bool, 40> input{};
    const std::uint8_t b0 = static_cast<std::uint8_t>(id >> 16u);
    const std::uint8_t b1 = static_cast<std::uint8_t>(id >> 8u);
    const std::uint8_t b2 = static_cast<std::uint8_t>(id);
    std::uint8_t crc = 0;
    for (const auto byte : {b0, b1, b2}) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) crc = (crc & 0x80u) ? static_cast<std::uint8_t>((crc << 1u) ^ 0x07u) : static_cast<std::uint8_t>(crc << 1u);
    }
    const std::array<std::uint8_t, 4> bytes{b0, b1, b2, crc};
    for (int bit = 0; bit < 32; ++bit) input[bit] = (bytes[bit / 8] >> (7 - (bit % 8))) & 1u;
    std::array<std::uint8_t, 80> encoded{};
    int state = 0;
    for (int step = 0; step < 40; ++step) {
        const int bit = input[step] ? 1 : 0;
        const auto reg = static_cast<std::uint16_t>(((state << 1) | bit) & 0x1ff);
        encoded[step * 2] = parity(reg & poly_a) ? 255 : 0;
        encoded[step * 2 + 1] = parity(reg & poly_b) ? 255 : 0;
        state = ((state << 1) | bit) & 0xff;
    }
    assert(decode_opn(encoded) && *decode_opn(encoded) == id);

    const auto p = parse_line("P 1618706341 OPN 1615544 3.50 64 89113 24.3");
    assert(p && std::holds_alternative<Passing>(*p));
    const auto& passing = std::get<Passing>(*p);
    assert(passing.type == TransponderType::OPN);
    assert(passing.id == 1615544u);
    assert(passing.mer && *passing.mer == 24.3);

    const auto old_p = parse_line("P 10 AMB 1234567 -3 2 90000");
    assert(old_p && std::holds_alternative<Passing>(*old_p));
    assert(!std::get<Passing>(*old_p).mer);

    const auto status = parse_line("S 1792039754 -41.018744 5.08 77 52 2.95");
    assert(status && std::holds_alternative<Status>(*status));
    assert(std::get<Status>(*status).jitter_p95_ms);

    const auto sync = parse_line("T 1618721816 OPN 1615544 905370");
    assert(sync && std::holds_alternative<TimeSync>(*sync));

    assert(!parse_line("L 1792039754 START -8.3"));
    assert(!parse_line("P 1 RC4 123  -3 4 90000 12"));
    assert(!parse_line("P 1 OPN 10000000 -3 4 90000 12"));
    assert(!parse_line("garbage"));

    const auto yurlaps = encode_yurlaps(7654321u);
    const auto decoded_yurlaps = decode_yurlaps(yurlaps);
    assert(decoded_yurlaps && decoded_yurlaps->id == 7654321u);
    auto damaged_yurlaps = yurlaps;
    damaged_yurlaps[10] = static_cast<std::uint8_t>(255u - damaged_yurlaps[10]);
    assert(decode_yurlaps(damaged_yurlaps));

    std::cout << "protocol tests passed\n";
}
