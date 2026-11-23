# YurLaps PIC16F18313 firmware

This is the PIC firmware source for the first YurLaps protocol test on the RC
Hourglass PIC16F18313 hardware. It emits the YurLaps marker `0xA65C`, a 24-bit
ID, an 8-bit sequence, and an 8-bit CRC protected by K=9 rate-1/2 encoding.

The source is written for MPLAB XC8. Set the ID at build time, for example:

```text
-DYURLAPS_ID=2504260
```

Build target: `PIC16F18313`.

The project must be configured for the actual RC Hourglass board pin mapping.
The source selects the DSM, CLC, and CWG path used by the documented board
layout. Verify carrier output with an oscilloscope or RTL-SDR before applying
the HEX to another unit.

The current source default is ID `2504260`. A HEX file cannot be generated in
the current environment because XC8/PIC-AS is not installed. Generate the HEX
in MPLAB X, then test one spare transponder before production flashing.
