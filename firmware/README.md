# YurLaps transponder firmware prototype

`yurlaps_attiny816_prototype.c` contains the protocol encoder used to align the
ATtiny816 firmware with the native decoder. It is intentionally a bench-test
skeleton. The carrier timer, pin routing, clock source, RF output stage, ID
storage, and repeat jitter must be filled from the final schematic.

The current test platform is an RC Hourglass transponder. The existing
RC Hourglass firmware is not modified by this file.
