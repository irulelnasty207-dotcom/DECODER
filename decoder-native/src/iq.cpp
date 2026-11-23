#include "yurlaps_decoder/iq.hpp"

#include "yurlaps_decoder/amb.hpp"
#include "yurlaps_decoder/opn.hpp"
#include "yurlaps_decoder/yurlaps.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <complex>
#include <limits>
#include <numeric>
#include <optional>
#include <iterator>
#include <utility>
#include <vector>

namespace yurlaps::decoder {
namespace {

constexpr int SAMPLES_PER_SYMBOL = 2;
constexpr int PREAMBLE_SYMBOLS = 16;
constexpr int PAYLOAD_SYMBOLS = static_cast<int>(YURLAPS_PAYLOAD_SYMBOLS);
constexpr int FRAME_SAMPLES = (PREAMBLE_SYMBOLS + PAYLOAD_SYMBOLS) * SAMPLES_PER_SYMBOL;
constexpr int FRAME_WINDOW_SAMPLES = FRAME_SAMPLES + SAMPLES_PER_SYMBOL - 1;
constexpr std::uint64_t QUIET_WINDOW_US = 250'000;
constexpr float MIN_PREAMBLE_SCORE = 0.72f;
constexpr float QUICK_PREAMBLE_SCORE = 0.12f;
constexpr int QUICK_PREAMBLE_SYMBOLS = 8;
constexpr float MIN_SIGNAL_DB = -40.0f;
constexpr float MIN_SNR_DB = 4.0f;
constexpr std::size_t NOISE_WINDOW_SIZE = 4096;
constexpr std::uint16_t OPN_PREAMBLE = 0xf9a8;
constexpr std::uint16_t AMB_PREAMBLE = 0x51e4;
constexpr std::uint16_t YURLAPS_PREAMBLE = 0xa65c;
constexpr float ADC_FULL_SCALE = 1.41421356237f;

struct Candidate {
    float score = 0.0f;
    int phase = 0;
    std::complex<float> correlation{};
    float phase_at_preamble_center = 0.0f;
    float phase_per_symbol = 0.0f;
    IqProtocol protocol = IqProtocol::OPN;
};

struct FrameMetrics {
    float signal_db{};
    float mer_db{};
};

Candidate match_preamble(const std::vector<IqSample>& samples,
                         std::size_t offset,
                         std::uint16_t preamble,
                         IqProtocol protocol) {
    Candidate best;
    for (int phase = 0; phase < SAMPLES_PER_SYMBOL; ++phase) {
        std::complex<float> correlation{};
        std::complex<float> phase_correlation{};
        float energy = 0.0f;
        std::complex<float> previous{};
        int differential_state = 0;
        for (int symbol = 0; symbol < PREAMBLE_SYMBOLS; ++symbol) {
            const auto& sample = samples[offset + static_cast<std::size_t>(symbol * SAMPLES_PER_SYMBOL + phase)];
            const std::complex<float> value{sample.i, sample.q};
            const int bit = (preamble & (1u << (PREAMBLE_SYMBOLS - 1 - symbol))) ? 1 : 0;
            if (protocol == IqProtocol::YURLAPS) differential_state ^= bit;
            const float expected = ((protocol == IqProtocol::YURLAPS ? differential_state : bit) != 0) ? 1.0f : -1.0f;
            const auto aligned = value * expected;
            correlation += aligned;
            if (symbol > 0) phase_correlation += aligned * std::conj(previous);
            previous = aligned;
            energy += std::norm(value);
        }
        const float score = energy > 1e-6f
            ? std::norm(correlation) / (energy * static_cast<float>(PREAMBLE_SYMBOLS))
            : 0.0f;
        const float phase_per_symbol = std::arg(phase_correlation);
        const float phase_at_center = std::arg(correlation);
        if (score > best.score) {
            best = {score, phase, correlation, phase_at_center, phase_per_symbol, protocol};
        }
    }
    return best;
}

// Full preamble scoring is deliberately expensive because it also estimates
// carrier phase drift.  Rejecting almost all noise with a short gate keeps the
// hot scan loop practical on a Raspberry Pi without changing the acceptance
// threshold used for real frames.
bool quick_preamble_gate(const std::vector<IqSample>& samples,
                         std::size_t offset,
                         std::uint16_t preamble) {
    for (int phase = 0; phase < SAMPLES_PER_SYMBOL; ++phase) {
        float correlation_i = 0.0f;
        float correlation_q = 0.0f;
        float energy = 0.0f;
        int differential_state = 0;
        for (int symbol = 0; symbol < QUICK_PREAMBLE_SYMBOLS; ++symbol) {
            const auto& sample = samples[offset + static_cast<std::size_t>(
                symbol * SAMPLES_PER_SYMBOL + phase)];
            const int bit = (preamble & (1u << (PREAMBLE_SYMBOLS - 1 - symbol))) ? 1 : 0;
            if (preamble == YURLAPS_PREAMBLE) differential_state ^= bit;
            const float expected = ((preamble == YURLAPS_PREAMBLE ? differential_state : bit) != 0)
                ? 1.0f : -1.0f;
            correlation_i += sample.i * expected;
            correlation_q += sample.q * expected;
            energy += sample.i * sample.i + sample.q * sample.q;
        }
        const float score = energy > 1e-6f
            ? (correlation_i * correlation_i + correlation_q * correlation_q)
                / (energy * static_cast<float>(QUICK_PREAMBLE_SYMBOLS))
            : 0.0f;
        if (score >= QUICK_PREAMBLE_SCORE) return true;
    }
    return false;
}

FrameMetrics decode_metrics(const std::vector<IqSample>& samples,
                            std::size_t offset,
                            int phase,
                            float phase_at_preamble_center,
                            float phase_per_symbol,
                            IqProtocol protocol,
                            std::array<std::uint8_t, PAYLOAD_SYMBOLS>& softbits) {
    float signal = 0.0f;
    std::vector<std::complex<float>> symbols;
    symbols.reserve(PAYLOAD_SYMBOLS);
    std::complex<float> previous_symbol{1.0f, 0.0f};
    if (protocol == IqProtocol::YURLAPS) {
        const std::size_t previous_offset = offset
            + static_cast<std::size_t>((PREAMBLE_SYMBOLS - 1) * SAMPLES_PER_SYMBOL + phase);
        std::complex<float> carrier{};
        for (int sample = 0; sample < SAMPLES_PER_SYMBOL; ++sample) {
            const auto& point = samples[previous_offset + static_cast<std::size_t>(sample)];
            carrier += std::complex<float>{point.i, point.q};
        }
        if (std::abs(carrier) > 1e-6f) previous_symbol = carrier / std::abs(carrier);
    }
    for (int symbol = 0; symbol < PAYLOAD_SYMBOLS; ++symbol) {
        const std::size_t sample_offset = offset
            + static_cast<std::size_t>((PREAMBLE_SYMBOLS + symbol) * SAMPLES_PER_SYMBOL + phase);
        std::complex<float> value{};
        for (int sample = 0; sample < SAMPLES_PER_SYMBOL; ++sample) {
            const auto& point = samples[sample_offset + static_cast<std::size_t>(sample)];
            const float symbol_position = static_cast<float>(PREAMBLE_SYMBOLS + symbol)
                + static_cast<float>(sample) / static_cast<float>(SAMPLES_PER_SYMBOL)
                - (static_cast<float>(PREAMBLE_SYMBOLS) - 1.0f) / 2.0f;
            const auto correction = std::polar(1.0f,
                -(phase_at_preamble_center + phase_per_symbol * symbol_position));
            value += std::complex<float>{point.i, point.q} * correction;
        }
        value /= static_cast<float>(SAMPLES_PER_SYMBOL);
        if (protocol == IqProtocol::YURLAPS) {
            const auto carrier_symbol = value;
            value = value * std::conj(previous_symbol);
            previous_symbol = std::abs(carrier_symbol) > 1e-6f
                ? std::complex<float>{carrier_symbol / std::abs(carrier_symbol)} : previous_symbol;
        } else previous_symbol = value;
        symbols.push_back(value);
        signal += std::abs(value);
        const float normalized = std::clamp(value.real(), -1.0f, 1.0f);
        softbits[static_cast<std::size_t>(symbol)] = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(127.5f + normalized * 127.5f), 0, 255));
    }

    const float amplitude = std::max(signal / static_cast<float>(PAYLOAD_SYMBOLS), 1e-6f);
    float error_power = 0.0f;
    for (const auto& value : symbols) {
        const float ideal_sign = value.real() >= 0.0f ? 1.0f : -1.0f;
        const auto error = value - std::complex<float>{ideal_sign * amplitude, 0.0f};
        error_power += std::norm(error);
    }
    const float evm_rms = std::sqrt(error_power / static_cast<float>(symbols.size())) / amplitude;
    const float mer = evm_rms > 1e-6f
        ? std::clamp(-20.0f * std::log10(evm_rms), -20.0f, 60.0f)
        : 60.0f;
    return {
        20.0f * std::log10(amplitude / ADC_FULL_SCALE),
        mer,
    };
}

}  // namespace

IqDecoder::IqDecoder(double sample_rate)
    : sample_rate_(sample_rate),
      next_status_sample_(static_cast<std::uint64_t>(std::llround(sample_rate))) {}

void IqDecoder::set_callback(PassingCallback callback) {
    callback_ = std::move(callback);
}

void IqDecoder::set_hit_callback(HitCallback callback) {
    hit_callback_ = std::move(callback);
}

void IqDecoder::set_status_callback(StatusCallback callback) {
    status_callback_ = std::move(callback);
}

void IqDecoder::set_time_sync_callback(TimeSyncCallback callback) {
    time_sync_callback_ = std::move(callback);
}

void IqDecoder::feed(const std::uint8_t* interleaved_iq, std::size_t byte_count) {
    if (!interleaved_iq || byte_count < 2) return;
    std::vector<IqSample> samples;
    samples.reserve(byte_count / 2);
    for (std::size_t i = 0; i + 1 < byte_count; i += 2) {
        samples.push_back({
            (static_cast<float>(interleaved_iq[i]) - 127.5f) / 127.5f,
            (static_cast<float>(interleaved_iq[i + 1]) - 127.5f) / 127.5f,
        });
    }
    feed(samples);
}

void IqDecoder::feed(const std::uint8_t* interleaved_iq,
                     std::size_t byte_count,
                     std::uint64_t host_timestamp_us) {
    if (!interleaved_iq || byte_count < 2) return;
    std::vector<IqSample> samples;
    samples.reserve(byte_count / 2);
    for (std::size_t i = 0; i + 1 < byte_count; i += 2) {
        samples.push_back({
            (static_cast<float>(interleaved_iq[i]) - 127.5f) / 127.5f,
            (static_cast<float>(interleaved_iq[i + 1]) - 127.5f) / 127.5f,
        });
    }
    feed(samples, host_timestamp_us);
}

void IqDecoder::feed(const std::vector<IqSample>& samples) {
    feed_samples(samples, std::nullopt);
}

void IqDecoder::feed(const std::vector<IqSample>& samples, std::uint64_t host_timestamp_us) {
    feed_samples(samples, host_timestamp_us);
}

void IqDecoder::feed_samples(const std::vector<IqSample>& samples,
                             std::optional<std::uint64_t> host_timestamp_us) {
    if (samples.empty()) return;

    const std::uint64_t buffer_start_sample = current_sample_index();
    if (host_timestamp_us) {
        if (has_last_host_timestamp_) {
            const double expected_us = static_cast<double>(last_host_buffer_samples_)
                * 1'000'000.0 / sample_rate_;
            const double actual_us = static_cast<double>(*host_timestamp_us)
                - static_cast<double>(last_host_timestamp_us_);
            const auto error_us = static_cast<std::uint64_t>(std::llround(
                std::fabs(actual_us - expected_us)));
            jitter_history_.push_back({*host_timestamp_us, error_us});
            while (!jitter_history_.empty() && *host_timestamp_us - jitter_history_.front().first > 5'000'000) {
                jitter_history_.pop_front();
            }
        }
        host_segments_.push_back({buffer_start_sample, *host_timestamp_us});
        // Pending passings are closed after 250 ms, so retaining a few
        // seconds is enough for every timestamp that can still be emitted.
        const auto keep_samples = static_cast<std::uint64_t>(std::llround(sample_rate_ * 10.0));
        while (host_segments_.size() > 1
            && buffer_start_sample - host_segments_.front().sample_start > keep_samples) {
            host_segments_.pop_front();
        }
        last_host_timestamp_us_ = *host_timestamp_us;
        has_last_host_timestamp_ = true;
        last_host_buffer_samples_ = samples.size();
    }

    buffer_.insert(buffer_.end(), samples.begin(), samples.end());
    const bool frame_seen = scan();
    observe_noise(samples, frame_seen);
    flush_expired(current_sample_index());
    maybe_emit_status();
}

float IqDecoder::noise_floor_db() const {
    if (noise_power_window_.empty()) return -std::numeric_limits<float>::infinity();
    const double power = std::accumulate(noise_power_window_.begin(), noise_power_window_.end(), 0.0)
        / static_cast<double>(noise_power_window_.size());
    return static_cast<float>(10.0 * std::log10(std::max(power, 1e-12) /
                                                 (ADC_FULL_SCALE * ADC_FULL_SCALE)));
}

void IqDecoder::observe_noise(const std::vector<IqSample>& samples, bool frame_seen) {
    if (frame_seen) return;
    for (const auto& sample : samples) {
        noise_power_window_.push_back(std::norm(std::complex<float>{sample.i, sample.q}));
        if (noise_power_window_.size() > NOISE_WINDOW_SIZE) noise_power_window_.pop_front();
        dc_sum_i_ += sample.i;
        dc_sum_q_ += sample.q;
        ++dc_sample_count_;
    }
}

bool IqDecoder::accept_amb(std::uint64_t sample_index,
                           std::uint32_t id,
                           std::uint8_t status) {
    if (id == 0 || id >= 10'000'000u) return false;

    const std::uint8_t upper = static_cast<std::uint8_t>(status & 0xf8u);
    const std::uint8_t counter = static_cast<std::uint8_t>(status & 0x07u);
    if (upper == 0xf8u && counter != 0) {
        if (status != 0xffu) {
            amb_validation_timestamps_[upper] = sample_index;
        } else {
            const auto found = amb_validation_timestamps_.find(upper);
            const std::uint64_t quiet_samples = static_cast<std::uint64_t>(
                std::llround(QUIET_WINDOW_US * sample_rate_ / 1'000'000.0));
            if (found != amb_validation_timestamps_.end()
                && sample_index <= found->second + quiet_samples) {
                banned_amb_ids_.insert(id);
            }
        }
    }

    if (status == 0xffu && !is_amb_banned(id)) return true;
    return counter == 0 && !is_amb_banned(id);
}

bool IqDecoder::is_amb_banned(std::uint32_t id) const {
    return banned_amb_ids_.find(id) != banned_amb_ids_.end();
}

void IqDecoder::flush() {
    flush_expired(current_sample_index() + static_cast<std::uint64_t>(
        std::llround(QUIET_WINDOW_US * sample_rate_ / 1'000'000.0)));
}

std::uint64_t IqDecoder::timestamp_for_sample(std::uint64_t sample_index) const {
    if (!host_segments_.empty()) {
        const auto found = std::upper_bound(
            host_segments_.begin(), host_segments_.end(), sample_index,
            [](std::uint64_t sample, const HostSegment& segment) {
                return sample < segment.sample_start;
            });
        const auto& segment = found == host_segments_.begin()
            ? host_segments_.front() : *std::prev(found);
        const auto offset = static_cast<std::int64_t>(sample_index)
            - static_cast<std::int64_t>(segment.sample_start);
        const auto offset_us = static_cast<std::int64_t>(std::llround(
            static_cast<double>(offset) * 1'000'000.0 / sample_rate_));
        const auto timestamp = static_cast<std::int64_t>(segment.host_start_us) + offset_us;
        return timestamp > 0 ? static_cast<std::uint64_t>(timestamp) : 0;
    }
    return static_cast<std::uint64_t>(std::llround(
        static_cast<double>(sample_index) * 1'000'000.0 / sample_rate_));
}

std::uint64_t IqDecoder::current_sample_index() const noexcept {
    return absolute_samples_ + buffer_.size();
}

double IqDecoder::jitter_p95_ms() const {
    if (jitter_history_.size() < 20) return 0.0;
    std::vector<std::uint64_t> values;
    values.reserve(jitter_history_.size());
    for (const auto& entry : jitter_history_) values.push_back(entry.second);
    const std::size_t k = values.size() * 95 / 100;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(k), values.end());
    return static_cast<double>(values[k]) / 1000.0;
}

void IqDecoder::maybe_emit_status() {
    const std::uint64_t now_sample = current_sample_index();
    if (now_sample < next_status_sample_) return;
    if (status_callback_) {
        const double dc = dc_sample_count_ > 0
            ? std::hypot(dc_sum_i_ / static_cast<double>(dc_sample_count_),
                         dc_sum_q_ / static_cast<double>(dc_sample_count_)) * 127.5
            : 0.0;
        status_callback_({
            timestamp_for_sample(now_sample),
            noise_floor_db(),
            static_cast<float>(dc),
            frames_received_,
            frames_processed_,
            jitter_p95_ms(),
        });
    }
    frames_received_ = 0;
    frames_processed_ = 0;
    dc_sum_i_ = 0.0;
    dc_sum_q_ = 0.0;
    dc_sample_count_ = 0;
    while (next_status_sample_ <= now_sample) {
        next_status_sample_ += static_cast<std::uint64_t>(std::llround(sample_rate_));
    }
}

void IqDecoder::emit(Pending& pending) {
    if (pending.hits.size() < 2 || is_amb_banned(pending.id) || !callback_) return;
    const auto estimate = estimate_passing(pending.hits, sample_rate_);
    IqPassing passing{
        estimate.sample_index,
        timestamp_for_sample(estimate.sample_index),
        pending.protocol,
        pending.id,
        estimate.rssi_db,
        static_cast<std::uint32_t>(pending.hits.size()),
        estimate.duration_us,
        estimate.mer_db,
    };
    callback_(passing);
}

void IqDecoder::flush_expired(std::uint64_t now_sample) {
    const std::uint64_t quiet_samples = static_cast<std::uint64_t>(
        std::llround(QUIET_WINDOW_US * sample_rate_ / 1'000'000.0));
    std::vector<Pending> ready;
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (it->second.hits.empty()) {
            it = pending_.erase(it);
            continue;
        }
        const auto last = it->second.hits.back().sample_index;
        if (now_sample > last && now_sample - last > quiet_samples) {
            ready.push_back(std::move(it->second));
            it = pending_.erase(it);
        } else {
            ++it;
        }
    }
    std::sort(ready.begin(), ready.end(), [this](const Pending& left, const Pending& right) {
        const auto left_point = estimate_passing(left.hits, sample_rate_).sample_index;
        const auto right_point = estimate_passing(right.hits, sample_rate_).sample_index;
        return left_point < right_point;
    });
    for (auto& pending : ready) emit(pending);
}

void IqDecoder::add_hit(const IqPassing& hit) {
    const auto key = (static_cast<std::uint64_t>(hit.protocol == IqProtocol::AMB) << 32u) | hit.id;
    auto found = pending_.find(key);
    if (found == pending_.end()) {
        Pending pending{hit.protocol, hit.id, {HitSample{hit.sample_index, hit.rssi_db, hit.mer_db}}};
        pending_.emplace(key, std::move(pending));
        return;
    }
    found->second.hits.push_back({hit.sample_index, hit.rssi_db, hit.mer_db});
}

bool IqDecoder::scan() {
    std::size_t cursor = 0;
    bool frame_seen = false;
    while (buffer_.size() - cursor >= static_cast<std::size_t>(FRAME_WINDOW_SAMPLES)) {
        Candidate best;
        std::size_t best_offset = cursor;
        bool found = false;
        for (std::size_t offset = cursor;
             offset + FRAME_WINDOW_SAMPLES <= buffer_.size() && !found;
             ++offset) {
            Candidate at_offset;
            for (const auto [preamble, protocol] : {
                     std::pair<std::uint16_t, IqProtocol>{YURLAPS_PREAMBLE, IqProtocol::YURLAPS},
                     std::pair<std::uint16_t, IqProtocol>{OPN_PREAMBLE, IqProtocol::OPN},
                     std::pair<std::uint16_t, IqProtocol>{AMB_PREAMBLE, IqProtocol::AMB}}) {
                if (!quick_preamble_gate(buffer_, offset, preamble)) continue;
                const auto candidate = match_preamble(buffer_, offset, preamble, protocol);
                if (candidate.score > at_offset.score) at_offset = candidate;
            }
            if (at_offset.score >= MIN_PREAMBLE_SCORE) {
                best = at_offset;
                best_offset = offset;
                found = true;
            }
        }

        if (!found) {
            cursor = buffer_.size() - static_cast<std::size_t>(FRAME_WINDOW_SAMPLES) + 1;
            break;
        }
        frame_seen = true;
        std::array<std::uint8_t, PAYLOAD_SYMBOLS> softbits{};
        const auto metrics = decode_metrics(buffer_, best_offset, best.phase,
            best.phase_at_preamble_center, best.phase_per_symbol, best.protocol, softbits);

        ++frames_received_;
        const float noise_db = noise_floor_db();
        const bool noise_ready = std::isfinite(noise_db) && noise_power_window_.size() >= 256;
        const bool signal_above_noise = noise_ready
            ? (metrics.signal_db - noise_db >= MIN_SNR_DB)
            : (metrics.signal_db >= MIN_SIGNAL_DB);
        bool frame_processed = false;
        std::optional<std::uint32_t> id;
        std::uint8_t amb_status = 0;
        if (best.protocol == IqProtocol::YURLAPS) {
            if (const auto frame = decode_yurlaps(softbits)) id = frame->id;
            frame_processed = id.has_value() && signal_above_noise;
        } else if (best.protocol == IqProtocol::OPN) {
            id = decode_opn(softbits);
            frame_processed = id.has_value() && signal_above_noise;
        } else if (const auto frame = decode_amb_rc3(softbits)) {
            id = frame->id;
            amb_status = frame->status;
            frame_processed = signal_above_noise && *id != 0
                && (*id < 10'000'000u || (amb_status & 0x07u) == 0);
        }
        if (frame_processed) ++frames_processed_;

        const std::uint64_t preamble_sample = absolute_samples_ + best_offset;
        if (signal_above_noise && id && best.protocol == IqProtocol::YURLAPS) {
            if (hit_callback_) hit_callback_({preamble_sample, timestamp_for_sample(preamble_sample), best.protocol,
                                               *id, metrics.signal_db, metrics.mer_db, 0});
            add_hit({preamble_sample, timestamp_for_sample(preamble_sample), best.protocol,
                     *id, metrics.signal_db, 1, 0, metrics.mer_db});
        } else if (signal_above_noise && id && best.protocol == IqProtocol::OPN) {
            if (*id < 10'000'000u) {
                if (hit_callback_) {
                    hit_callback_({preamble_sample, timestamp_for_sample(preamble_sample), best.protocol,
                                   *id, metrics.signal_db, metrics.mer_db, 0});
                }
                add_hit({preamble_sample, timestamp_for_sample(preamble_sample), best.protocol,
                         *id, metrics.signal_db, 1, 0, metrics.mer_db});
            } else if ((*id & 0x00a00000u) == 0x00a00000u && time_sync_callback_) {
                time_sync_callback_({timestamp_for_sample(preamble_sample), *id,
                                      *id & 0x000fffffu});
            }
        } else if (signal_above_noise && id && best.protocol == IqProtocol::AMB
                   && accept_amb(preamble_sample, *id, amb_status)) {
            if (hit_callback_) {
                hit_callback_({preamble_sample, timestamp_for_sample(preamble_sample), best.protocol,
                               *id, metrics.signal_db, metrics.mer_db, amb_status});
            }
            add_hit({preamble_sample, timestamp_for_sample(preamble_sample), best.protocol,
                     *id, metrics.signal_db, 1, 0, metrics.mer_db});
        }
        cursor = best_offset + static_cast<std::size_t>(FRAME_SAMPLES);
    }

    if (cursor > 0) {
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(cursor));
        absolute_samples_ += cursor;
    }
    return frame_seen;
}

}  // namespace yurlaps::decoder
