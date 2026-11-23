#include "yurlaps_decoder/protocol.hpp"
#include "yurlaps_decoder/iq.hpp"
#include "yurlaps_decoder/rtl.hpp"
#include "yurlaps_decoder/zmq.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

int run_iq(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "cannot open IQ capture: " << path << '\n';
        return 2;
    }
    yurlaps::decoder::IqDecoder decoder;
    decoder.set_callback([](const yurlaps::decoder::IqPassing& passing) {
        std::cout << "P " << passing.timestamp_us / 1000 << ' '
                  << (passing.protocol == yurlaps::decoder::IqProtocol::YURLAPS ? "YUL" : passing.protocol == yurlaps::decoder::IqProtocol::OPN ? "OPN" : "AMB")
                  << ' ' << passing.id
                  << ' ' << std::fixed << std::setprecision(2) << passing.rssi_db
                  << ' ' << passing.hits << ' ' << passing.duration_us
                  << ' ' << std::setprecision(1) << passing.mer_db << '\n'
                  << std::flush;
    });
    decoder.set_status_callback([](const yurlaps::decoder::IqStatus& status) {
        std::cout << "S " << status.timestamp_us / 1000 << ' '
                  << std::fixed << std::setprecision(2) << status.noise_floor_db
                  << ' ' << status.dc_offset << ' ' << status.frames_received
                  << ' ' << status.frames_processed << ' '
                  << std::setprecision(2) << status.jitter_p95_ms << '\n'
                  << std::flush;
    });
    decoder.set_time_sync_callback([](const yurlaps::decoder::IqTimeSync& sync) {
        std::cout << "T " << sync.timestamp_us / 1000 << " OPN " << sync.transponder_id
                  << ' ' << sync.transponder_timecode << '\n' << std::flush;
    });
    std::vector<std::uint8_t> chunk(64 * 1024);
    while (input) {
        input.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
        const auto count = input.gcount();
        if (count > 0) decoder.feed(chunk.data(), static_cast<std::size_t>(count));
    }
    decoder.flush();
    return 0;
}

void enable_debug(yurlaps::decoder::IqDecoder& decoder) {
    decoder.set_hit_callback([](const yurlaps::decoder::IqHit& hit) {
        std::cout << "H " << hit.timestamp_us / 1000 << ' '
                  << (hit.protocol == yurlaps::decoder::IqProtocol::YURLAPS ? "YUL" : hit.protocol == yurlaps::decoder::IqProtocol::OPN ? "OPN" : "AMB")
                  << ' ' << hit.id << ' ' << std::fixed << std::setprecision(2)
                  << hit.rssi_db << ' ' << std::setprecision(1) << hit.mer_db
                  << ' ' << static_cast<unsigned>(hit.status) << '\n' << std::flush;
    });
}

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[1]) == "--iq") {
        if (argc >= 4 && std::string(argv[3]) == "--debug") {
            std::ifstream input(argv[2], std::ios::binary);
            if (!input) {
                std::cerr << "cannot open IQ capture: " << argv[2] << '\n';
                return 2;
            }
            yurlaps::decoder::IqDecoder decoder;
            enable_debug(decoder);
            decoder.set_callback([](const yurlaps::decoder::IqPassing& passing) {
                std::cout << "P " << passing.timestamp_us / 1000 << ' '
                          << (passing.protocol == yurlaps::decoder::IqProtocol::YURLAPS ? "YUL" : passing.protocol == yurlaps::decoder::IqProtocol::OPN ? "OPN" : "AMB")
                          << ' ' << passing.id << ' ' << std::fixed << std::setprecision(2)
                          << passing.rssi_db << ' ' << passing.hits << ' ' << passing.duration_us
                          << ' ' << std::setprecision(1) << passing.mer_db << '\n' << std::flush;
            });
            decoder.set_status_callback([](const yurlaps::decoder::IqStatus& status) {
                std::cout << "S " << status.timestamp_us / 1000 << ' '
                          << std::fixed << std::setprecision(2) << status.noise_floor_db
                          << ' ' << status.dc_offset << ' ' << status.frames_received
                          << ' ' << status.frames_processed << ' '
                          << std::setprecision(2) << status.jitter_p95_ms << '\n' << std::flush;
            });
            decoder.set_time_sync_callback([](const yurlaps::decoder::IqTimeSync& sync) {
                std::cout << "T " << sync.timestamp_us / 1000 << " OPN " << sync.transponder_id
                          << ' ' << sync.transponder_timecode << '\n' << std::flush;
            });
            std::vector<std::uint8_t> chunk(64 * 1024);
            while (input) {
                input.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
                const auto count = input.gcount();
                if (count > 0) decoder.feed(chunk.data(), static_cast<std::size_t>(count));
            }
            decoder.flush();
            return 0;
        }
        return run_iq(argv[2]);
    }
    if (argc >= 2 && std::string(argv[1]) == "--rtl") {
        yurlaps::decoder::RtlOptions options;
        int next = 2;
        if (next < argc && std::string(argv[next]) != "--debug") options.serial = argv[next++];
        if (next < argc && std::string(argv[next]) != "--debug") options.gain_db = std::stoi(argv[next++]);
        const bool debug = next < argc && std::string(argv[next]) == "--debug";
        yurlaps::decoder::IqDecoder decoder;
        yurlaps::decoder::Publisher publisher;
        std::string publisher_error;
        publisher.bind("tcp://127.0.0.1:5556", publisher_error);
        decoder.set_callback([&publisher](const yurlaps::decoder::IqPassing& passing) {
            std::ostringstream line;
            line << "P " << passing.timestamp_us / 1000 << ' '
                      << (passing.protocol == yurlaps::decoder::IqProtocol::YURLAPS ? "YUL" : passing.protocol == yurlaps::decoder::IqProtocol::OPN ? "OPN" : "AMB")
                      << ' ' << passing.id << ' ' << std::fixed << std::setprecision(2)
                      << passing.rssi_db << ' ' << passing.hits << ' ' << passing.duration_us
                      << ' ' << std::setprecision(1) << passing.mer_db
                      ;
            std::cout << line.str() << '\n' << std::flush;
            publisher.send(line.str());
        });
        if (debug) enable_debug(decoder);
        decoder.set_status_callback([&publisher](const yurlaps::decoder::IqStatus& status) {
            std::ostringstream line;
            line << "S " << status.timestamp_us / 1000 << ' '
                 << std::fixed << std::setprecision(2) << status.noise_floor_db
                 << ' ' << status.dc_offset << ' ' << status.frames_received
                 << ' ' << status.frames_processed << ' '
                 << std::setprecision(2) << status.jitter_p95_ms;
            std::cout << line.str() << '\n' << std::flush;
            publisher.send(line.str());
        });
        decoder.set_time_sync_callback([&publisher](const yurlaps::decoder::IqTimeSync& sync) {
            std::ostringstream line;
            line << "T " << sync.timestamp_us / 1000 << " OPN " << sync.transponder_id
                 << ' ' << sync.transponder_timecode;
            std::cout << line.str() << '\n' << std::flush;
            publisher.send(line.str());
        });
        std::string error;
        const int result = yurlaps::decoder::run_rtl_sdr(options, decoder, error);
        decoder.flush();
        if (result != 0) std::cerr << error << '\n';
        return result;
    }
    for (std::string line; std::getline(std::cin, line);) {
        if (const auto event = yurlaps::decoder::parse_line(line)) {
            std::cout << yurlaps::decoder::format_event(*event) << '\n' << std::flush;
        }
    }
    return 0;
}
