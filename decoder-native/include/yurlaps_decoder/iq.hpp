#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <utility>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "yurlaps_decoder/passing.hpp"

namespace yurlaps::decoder {

enum class IqProtocol { YURLAPS, OPN, AMB };

struct IqSample {
    float i{};
    float q{};
};

struct IqPassing {
    std::uint64_t sample_index{};
    std::uint64_t timestamp_us{};
    IqProtocol protocol{IqProtocol::OPN};
    std::uint32_t id{};
    float rssi_db{};
    std::uint32_t hits{};
    std::uint64_t duration_us{};
    float mer_db{};
};

using PassingCallback = std::function<void(const IqPassing&)>;

struct IqHit {
    std::uint64_t sample_index{};
    std::uint64_t timestamp_us{};
    IqProtocol protocol{IqProtocol::OPN};
    std::uint32_t id{};
    float rssi_db{};
    float mer_db{};
    std::uint8_t status{};
};

using HitCallback = std::function<void(const IqHit&)>;

struct IqStatus {
    std::uint64_t timestamp_us{};
    float noise_floor_db{};
    float dc_offset{};
    std::uint64_t frames_received{};
    std::uint64_t frames_processed{};
    double jitter_p95_ms{};
};

using StatusCallback = std::function<void(const IqStatus&)>;

struct IqTimeSync {
    std::uint64_t timestamp_us{};
    std::uint32_t transponder_id{};
    std::uint32_t transponder_timecode{};
};

using TimeSyncCallback = std::function<void(const IqTimeSync&)>;

// Multi-protocol baseband detector for YurLaps, OPN, and AMB RC3 on the 5 MHz
// loop carrier. No external decoder library is linked.
class IqDecoder {
public:
    explicit IqDecoder(double sample_rate = 2'500'000.0);

    void set_callback(PassingCallback callback);
    void set_hit_callback(HitCallback callback);
    void set_status_callback(StatusCallback callback);
    void set_time_sync_callback(TimeSyncCallback callback);
    void feed(const std::uint8_t* interleaved_iq, std::size_t byte_count);
    void feed(const std::uint8_t* interleaved_iq, std::size_t byte_count,
              std::uint64_t host_timestamp_us);
    void feed(const std::vector<IqSample>& samples);
    void feed(const std::vector<IqSample>& samples, std::uint64_t host_timestamp_us);
    void flush();
    std::uint64_t samples_seen() const noexcept { return absolute_samples_; }
    double sample_rate() const noexcept { return sample_rate_; }

private:
    double sample_rate_;
    std::vector<IqSample> buffer_;
    std::deque<float> noise_power_window_;
    double dc_sum_i_ = 0.0;
    double dc_sum_q_ = 0.0;
    std::uint64_t dc_sample_count_ = 0;
    std::uint64_t absolute_samples_ = 0;
    PassingCallback callback_;
    HitCallback hit_callback_;
    StatusCallback status_callback_;
    TimeSyncCallback time_sync_callback_;

    struct HostSegment {
        std::uint64_t sample_start{};
        std::uint64_t host_start_us{};
    };
    // Each RTL callback gets its own anchor.  This keeps timestamps tied to
    // the actual arrival of a buffer when USB delivery has a gap.
    std::deque<HostSegment> host_segments_;
    std::uint64_t last_host_timestamp_us_ = 0;
    bool has_last_host_timestamp_ = false;
    std::uint64_t last_host_buffer_samples_ = 0;
    std::deque<std::pair<std::uint64_t, std::uint64_t>> jitter_history_;
    std::uint64_t next_status_sample_{};
    std::uint64_t frames_received_ = 0;
    std::uint64_t frames_processed_ = 0;

    std::unordered_map<std::uint8_t, std::uint64_t> amb_validation_timestamps_;
    std::unordered_set<std::uint32_t> banned_amb_ids_;

    struct Pending {
        IqProtocol protocol{IqProtocol::OPN};
        std::uint32_t id{};
        std::vector<HitSample> hits;
    };
    std::unordered_map<std::uint64_t, Pending> pending_;

    void feed_samples(const std::vector<IqSample>& samples,
                      std::optional<std::uint64_t> host_timestamp_us);
    bool scan();
    void observe_noise(const std::vector<IqSample>& samples, bool frame_seen);
    float noise_floor_db() const;
    bool accept_amb(std::uint64_t sample_index, std::uint32_t id, std::uint8_t status);
    bool is_amb_banned(std::uint32_t id) const;
    void add_hit(const IqPassing& hit);
    void flush_expired(std::uint64_t now_sample);
    void emit(Pending& pending);
    std::uint64_t timestamp_for_sample(std::uint64_t sample_index) const;
    std::uint64_t current_sample_index() const noexcept;
    void maybe_emit_status();
    double jitter_p95_ms() const;
};

}  // namespace yurlaps::decoder
