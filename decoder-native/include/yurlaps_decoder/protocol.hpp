#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace yurlaps::decoder {

enum class TransponderType { YURLAPS, OPN, AMB };

struct Passing {
    std::uint64_t timestamp_ms{};
    TransponderType type{};
    std::uint32_t id{};
    double rssi{};
    std::uint32_t hits{};
    std::uint64_t duration_us{};
    std::optional<double> mer;
};

struct Status {
    std::uint64_t timestamp_ms{};
    double noise_db{};
    double dc_offset{};
    std::uint64_t frames_received{};
    std::uint64_t frames_processed{};
    std::optional<double> jitter_p95_ms;
};

struct TimeSync {
    std::uint64_t timestamp_ms{};
    TransponderType type{};
    std::uint32_t id{};
    std::uint32_t transponder_timecode{};
};

using Event = std::variant<Passing, Status, TimeSync>;

// Parse one documented decoder line. Unknown protocols and RC4 learning
// messages return nullopt so they cannot enter the race engine.
std::optional<Event> parse_line(std::string_view line);

std::string format_event(const Event& event);

}  // namespace yurlaps::decoder
