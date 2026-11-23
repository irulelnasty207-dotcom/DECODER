# YurLaps PIC16F18313 firmware

This firmware emits the YLP-1 coherent-BPSK frame on the PIC16F18313 RF path used by the RC Hourglass test board. The frame has four init symbols, the `0xF9A8` Barker-13 marker, a 24-bit ID, CRC16, K=9 rate-1/2 tail-biting FEC, and four tail symbols.

Use `yurlaps_pic16f18313.asm` for the fixed-timing MPLAB MPASM build. The file contains a test vector for ID `2504260`. Its frame bytes are:

```text
0F 9A 81 18 80 FE 2B 27 F5 00 C2 55 90
```

The C source describes the same protocol and is useful for an XC8 prototype. Its instruction timing is not as deterministic as the assembly build.

Build target: `PIC16F18313`. Verify the actual RF output with an oscilloscope or RTL-SDR before flashing another transponder.
