# YurLaps Protocol Draft 0.3

YLP-1 is the native YurLaps transponder protocol. The PIC16F18313 firmware uses the RC Hourglass RF path only as a physical test platform. The message format and decoder are separate from the legacy OPN and AMB formats.

## Physical layer

- Carrier: 5 MHz loop system
- Receive rate: 2.5 MS/s IQ
- Modulation: coherent BPSK with the absolute symbol phase selected by each bit
- Symbol rate: 1.25 Msymbol/s
- Init: four zero symbols
- Preamble: Barker-13 followed by three zero pad symbols, represented as `0xF9A8`
- Repeat interval: 750 to 2250 microseconds of silence after each frame

The decoder estimates carrier phase from the preamble, then demodulates the payload with that phase. It also evaluates both ADC sample phases while scanning.

## Frame

The complete frame is 104 symbols.

| Field | Symbols or bits | Purpose |
|---|---:|---|
| Init | 4 symbols | Establishes the YLP frame boundary |
| Preamble | 16 symbols | Barker-13 marker plus three pad symbols |
| Transponder ID | 24 bits | Stable race identity |
| CRC16 | 16 bits | Rejects damaged payloads |
| FEC payload | 80 symbols | K=9 rate-1/2 convolutional code with tail-biting |
| Tail | 4 symbols | Completes the fixed-length frame |

The 40 payload bits are encoded as 80 symbols. Tail-biting selects the encoder state so the final state equals the initial state. There is no sequence field, RC4 learning message, or status message in YLP-1. Repeated frames are grouped by protocol and transponder ID in the decoder quiet window.

## Passing timing

The decoder records the sample index of each accepted preamble. The race layer can use the waveform crossing estimate for passing order and can hold accepted hits for the quiet window before announcing a result. USB callback arrival order does not determine race order.

The current native decoder uses a 40 ms YLP grouping window. It requires at least two accepted frames for a passing, which rejects isolated noise detections while keeping the delay below the legacy 250 ms window.

## Compatibility

The decoder accepts YLP-1, OPN, and legacy AMB RC3. RC4, RC4 Hybrid, Vostok, learning frames, and unrelated markers are rejected. OPN remains available for existing OpenStint-compatible transponders.

YLP-1 uses the same Barker-13 marker value as OPN, so the scanner keeps symbol alignment and validates the YLP init, CRC16, and tail-biting payload before reporting `YUL`. This prevents an OPN payload from being reported as YLP-1.

RF range, collision performance, and high-speed passing accuracy still require IQ captures from the target PCB and loop.
