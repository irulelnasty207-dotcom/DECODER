/*
 * YurLaps ATtiny816 transmitter prototype.
 *
 * This is an encoder skeleton for bench testing. Pin routing, clock setup,
 * carrier timer, and power control must be matched to the final PCB schematic.
 * It emits the same 5-byte payload and K=9 rate-1/2 bits as the native decoder.
 */
#include <stdint.h>

#define YURLAPS_VERSION_MARKER 0x59u
#define YURLAPS_ID 1234567u

static uint8_t crc8(uint32_t id, uint8_t sequence) {
    uint8_t bytes[4] = {(uint8_t)(id >> 16), (uint8_t)(id >> 8), (uint8_t)id, sequence};
    uint8_t crc = 0;
    for (uint8_t index = 0; index < 4; ++index) {
        crc ^= bytes[index];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
    }
    return crc;
}

static uint8_t parity(uint16_t value) {
    uint8_t result = 0;
    while (value) { result ^= (uint8_t)(value & 1u); value >>= 1; }
    return result;
}

static void emit_symbol(uint8_t one) {
    /* TODO: connect this to the ATtiny816 waveform output timer. */
    (void)one;
}

static void send_frame(uint32_t id, uint8_t sequence) {
    const uint8_t bytes[5] = {
        (uint8_t)(id >> 16), (uint8_t)(id >> 8), (uint8_t)id,
        sequence, crc8(id, sequence),
    };
    uint16_t state = 0;

    /* YurLaps marker 0xA65C, followed by 4-bit init energy symbols. */
    for (int bit = 3; bit >= 0; --bit) emit_symbol((uint8_t)((0xAu >> bit) & 1u));
    for (int bit = 15; bit >= 0; --bit) emit_symbol((uint8_t)((0xA65Cu >> bit) & 1u));

    for (uint8_t byte = 0; byte < 5; ++byte) {
        for (int bit = 7; bit >= 0; --bit) {
            const uint8_t input = (uint8_t)((bytes[byte] >> bit) & 1u);
            const uint16_t reg = (uint16_t)(((state << 1) | input) & 0x1FFu);
            emit_symbol(parity((uint16_t)(reg & 0x1AFu)));
            emit_symbol(parity((uint16_t)(reg & 0x11Du)));
            state = (uint16_t)(((state << 1) | input) & 0xFFu);
        }
    }
}

int main(void) {
    uint8_t sequence = 0;
    for (;;) {
        send_frame(YURLAPS_ID, sequence++);
        /* TODO: wait 750–2250 us using a hardware timer and jitter source. */
    }
}
