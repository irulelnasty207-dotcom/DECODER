# YurLaps Protocol Draft 0.2

This design is for a YurLaps transponder firmware prototype and decoder. The
current RC Hourglass unit is used only as the physical test platform. Its
firmware remains unchanged during this protocol test.

## Physical layer

- carrier: 5 MHz loop system
- receive rate: 2.5 MS/s IQ
- modulation: coherent BPSK aligned to the carrier
- symbol rate: 1.25 MHz for the first prototype
- preamble: YurLaps marker `0xA65C`, preceded by an energy detector init
- repeat interval: average 1.5 ms, bounded random jitter, with at least 750 us
  silence between frames

## Frame

The payload contains 40 bits. K=9 rate-1/2 convolutional coding expands it to
80 transmitted symbols.

| Field | Bits | Purpose |
|---|---:|---|
| transponder ID | 24 | stable race identity |
| sequence | 8 | duplicate rejection and repeat tracking |
| CRC-8 | 8 | damaged-frame rejection |

The decoder reports a frame only after trellis decoding, CRC validation, and a
minimum signal confidence check. It uses the waveform crossing estimate for
passing order.

## Passing timing

The decoder stores accepted frames during the candidate window. It estimates
the crossing from the waveform envelope and calibrated loop geometry, sorts by
that estimate, and emits one passing event per accepted sequence. USB callback
arrival order does not decide the race order.

The initial candidate window is 20 ms, with a 75 ms maximum while a frame is
still being decoded. These values require testing with two and three
transponders at 60 km/h before production use.

## Protocol comparison

| Property | OPN | YurLaps draft |
|---|---|---|
| Carrier | 5 MHz | 5 MHz |
| Modulation | coherent BPSK | coherent BPSK |
| Payload | 24-bit ID, CRC-8, trellis tail | 24-bit ID, sequence, CRC-8 |
| FEC | K=9, rate 1/2 | K=9, rate 1/2 |
| Preamble | OPN marker | separate YurLaps marker |
| Repeat handling | grouped by decoder timing | sequence-aware grouping |
| Passing order | waveform timestamp | waveform timestamp |
| Existing hardware | OPN transponder | new firmware required |

Sequence tracking is the first protocol improvement. A delayed duplicate is
not counted as a new hit, and the decoder can measure repeat gaps. RF range and
collision performance require IQ tests under equal antenna and SNR conditions.

## Compatibility

The decoder supports YurLaps, OPN, and legacy AMB RC3. RC4, learning frames,
and unrelated markers are rejected.

This draft supports a patentability review. It does not establish novelty,
inventorship, ownership, or patent protection.
