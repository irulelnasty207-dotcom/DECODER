# YurLaps Decoder and Transponder Roadmap

1. Freeze this protocol draft and assign a version number.
2. Build a software-only encoder and decoder pair with generated IQ fixtures.
3. Add a hardware transmitter prototype that can emit the same frame.
4. Capture the prototype on RTL-SDR and compare decoded time with a reference
   sensor.
5. Add the new protocol to `IqDecoder` behind an explicit protocol enum.
6. Run two-car and three-car tests before changing the production default.
7. Keep OPN and AMB compatibility until every deployed YurLaps transponder has
   migrated.

The current repository does not contain transponder firmware or a transmitter
schematic, so a working new transponder cannot be produced from decoder source
alone.

## RC Hourglass prototype findings

The public RC Hourglass repository identifies the transponder MCU as
PIC16F18313. Its assembly config uses an external high-speed clock with the
4x PLL, exposes a 5 MHz clock reference, and drives a Digital Signal Modulator.
The firmware stores fixed message tables and shifts their bits through the
modulator. This is enough hardware information to plan a YurLaps transmitter
prototype.

The repository's license text requires its notices in copies or substantial
portions, requires derivative source/design to be public under its terms, and
requests contact for commercial production. A YurLaps production firmware must
therefore be a clean-room implementation from the new protocol specification,
with the hardware facts independently verified, or it needs written permission
for a derivative implementation. Removing the original notices would not
resolve those obligations.

The next safe prototype is a new PIC16F18313 project that implements only the
YurLaps frame defined in `YURLAPS-PROTOCOL-DRAFT.md`. It must be compared on an
IQ capture against a software encoder before any production flashing.
