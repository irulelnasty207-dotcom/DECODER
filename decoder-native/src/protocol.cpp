#include "yurlaps_decoder/protocol.hpp"

#include <charconv>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace yurlaps::decoder {
namespace {

std::vector<std::string> split(std::string_view line) {
    std::istringstream in{std::string(line)};
    std::vector<std::string> parts;
    for (std::string part; in >> part;) parts.push_back(std::move(part));
    return parts;
}

template <typename T>
std::optional<T> number(const std::string& value) {
    T out{};
    const char* begin = value.data();
    const char* end = begin + value.size();
    if constexpr (std::is_floating_point_v<T>) {
        char* parsed = nullptr;
        out = static_cast<T>(std::strtod(begin, &parsed));
        if (parsed != end) return std::nullopt;
        return out;
    } else {
        auto [ptr, error] = std::from_chars(begin, end, out);
        if (error != std::errc{} || ptr != end) return std::nullopt;
        return out;
    }
}

std::optional<TransponderType> type_of(const std::string& value) {
    if (value == "YUL" || value == "YURLAPS") return TransponderType::YURLAPS;
    if (value == "OPN") return TransponderType::OPN;
    if (value == "AMB") return TransponderType::AMB;
    return std::nullopt;
}

}  // namespace

std::optional<Event> parse_line(std::string_view line) {
    const auto p = split(line);
    if (p.empty()) return std::nullopt;

    if (p[0] == "P") {
        if (p.size() < 7) return std::nullopt;
        const auto ts = number<std::uint64_t>(p[1]);
        const auto type = type_of(p[2]);
        const auto id = number<std::uint32_t>(p[3]);
        const auto rssi = number<double>(p[4]);
        const auto hits = number<std::uint32_t>(p[5]);
        const auto duration = number<std::uint64_t>(p[6]);
        if (!ts || !type || !id || !rssi || !hits || !duration || *id > 9999999u) {
            return std::nullopt;
        }
        Passing event{*ts, *type, *id, *rssi, *hits, *duration, std::nullopt};
        if (p.size() >= 8) event.mer = number<double>(p[7]);
        return event;
    }

    if (p[0] == "S") {
        if (p.size() < 6) return std::nullopt;
        const auto ts = number<std::uint64_t>(p[1]);
        const auto noise = number<double>(p[2]);
        const auto dc = number<double>(p[3]);
        const auto received = number<std::uint64_t>(p[4]);
        const auto processed = number<std::uint64_t>(p[5]);
        if (!ts || !noise || !dc || !received || !processed) return std::nullopt;
        Status event{*ts, *noise, *dc, *received, *processed, std::nullopt};
        if (p.size() >= 7) event.jitter_p95_ms = number<double>(p[6]);
        return event;
    }

    if (p[0] == "T") {
        if (p.size() < 5) return std::nullopt;
        const auto ts = number<std::uint64_t>(p[1]);
        const auto type = type_of(p[2]);
        const auto id = number<std::uint32_t>(p[3]);
        const auto tc = number<std::uint32_t>(p[4]);
        if (!ts || !type || !id || !tc || *id > 9999999u) return std::nullopt;
        return TimeSync{*ts, *type, *id, *tc};
    }

    // In particular, L (RC4 learning) is deliberately ignored.
    return std::nullopt;
}

std::string format_event(const Event& event) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    std::visit([&out](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Passing>) {
            out << "P " << value.timestamp_ms << ' '
                << (value.type == TransponderType::YURLAPS ? "YUL" : value.type == TransponderType::OPN ? "OPN" : "AMB") << ' '
                << value.id << ' ' << value.rssi << ' ' << value.hits << ' '
                << value.duration_us;
            if (value.mer) out << ' ' << *value.mer;
        } else if constexpr (std::is_same_v<T, Status>) {
            out << "S " << value.timestamp_ms << ' ' << value.noise_db << ' '
                << value.dc_offset << ' ' << value.frames_received << ' '
                << value.frames_processed;
            if (value.jitter_p95_ms) out << ' ' << *value.jitter_p95_ms;
        } else {
            out << "T " << value.timestamp_ms << ' '
                << (value.type == TransponderType::YURLAPS ? "YUL" : value.type == TransponderType::OPN ? "OPN" : "AMB") << ' '
                << value.id << ' ' << value.transponder_timecode;
        }
    }, event);
    return out.str();
}

}  // namespace yurlaps::decoder
