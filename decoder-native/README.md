# YurLaps Decoder Native

This repository contains an independent native decoder for the YurLaps YLP-1 protocol and the supported legacy formats.

- `YUL` is the coherent-BPSK YLP-1 frame with CRC16 and tail-biting FEC.
- `OPN` is the existing OpenStint-compatible frame.
- `AMB` is the legacy RC3 frame.

RC4, RC4 Hybrid, Vostok, and learning messages are rejected. RTL-SDR and ZeroMQ are optional build features.

## Build without hardware

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## IQ capture

```sh
YurLapsDecoder.exe --iq passing.iq
```

## RTL-SDR

```sh
YurLapsDecoder.exe --rtl YD0001 28
```

The decoder estimates the timestamp from the sample index. It scans both ADC sample phases, demodulates coherent BPSK after carrier-phase estimation, validates YLP-1 with CRC16, and groups repeated hits for 40 ms before emitting a passing.

Output format:

```text
P <timestamp_ms> <YUL|OPN|AMB> <id> <rssi_dbfs> <hits> <duration_us> <mer_db>
S <timestamp_ms> <noise_dbfs> <dc_offset> <frames_received> <frames_processed> <jitter_p95_ms>
T <timestamp_ms> OPN <id> <transponder_timecode>
```
