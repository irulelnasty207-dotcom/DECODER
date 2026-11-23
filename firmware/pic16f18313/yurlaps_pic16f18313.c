/*
 * YurLaps YLP-1 transmitter for PIC16F18313.
 *
 * YLP-1 is a coherent-BPSK frame. It has four low init symbols, the
 * Barker-13 marker padded to 0xF9A8, a 24-bit ID plus CRC16 protected by a
 * K=9 rate-1/2 tail-biting code, and four low tail symbols. There is no
 * sequence field and no RC4 learning/status frame.
 *
 * This source uses the RC Hourglass PIC16F18313 RF path as a test platform.
 * It does not copy its message tables. The assembly source in this folder is
 * the fixed-timing build used for a production timing test.
 */

#include <stdint.h>
#include <xc.h>

#define _XTAL_FREQ 20000000UL
#define YURLAPS_PREAMBLE 0xF9A8u

#pragma config FEXTOSC = HS
#pragma config RSTOSC = EXT4X
#pragma config CLKOUTEN = OFF
#pragma config CSWEN = OFF
#pragma config FCMEN = OFF
#pragma config MCLRE = OFF
#pragma config PWRTE = OFF
#pragma config WDTE = OFF
#pragma config LPBOREN = OFF
#pragma config BOREN = ON
#pragma config BORV = LOW
#pragma config STVREN = ON
#pragma config DEBUG = OFF
#pragma config WRT = OFF
#pragma config LVP = ON
#pragma config CP = OFF
#pragma config CPD = OFF

#ifndef YURLAPS_ID
#define YURLAPS_ID 2504260UL
#endif

#if (YURLAPS_ID == 0UL) || (YURLAPS_ID > 0x00FFFFFFUL)
#error "YURLAPS_ID must fit in a non-zero 24-bit value"
#endif

static uint8_t convolution_state;
static uint8_t led_divider;

static uint8_t parity16(uint16_t value) {
    uint8_t result = 0u;
    while (value != 0u) {
        result ^= (uint8_t)(value & 1u);
        value >>= 1u;
    }
    return result;
}

static uint16_t crc16(const uint8_t *bytes) {
    uint16_t crc = 0xFFFFu;
    uint8_t index;
    uint8_t bit;
    uint8_t carry;
    for (index = 0u; index < 3u; ++index) {
        crc ^= (uint16_t)bytes[index] << 8u;
        for (bit = 0u; bit < 8u; ++bit) {
            carry = (uint8_t)(crc & 0x8000u);
            crc <<= 1u;
            if (carry != 0u) crc ^= 0x1021u;
        }
    }
    return crc;
}

static uint8_t payload_bit(const uint8_t *bytes, uint8_t index) {
    return (uint8_t)((bytes[index >> 3u] >> (7u - (index & 7u))) & 1u);
}

static uint8_t tail_biting_state(const uint8_t *bytes) {
    uint16_t candidate;
    uint8_t bit;
    uint8_t state;
    for (candidate = 0u; candidate < 256u; ++candidate) {
        state = (uint8_t)candidate;
        for (bit = 0u; bit < 40u; ++bit) {
            state = (uint8_t)(((uint16_t)state << 1u)
                              | payload_bit(bytes, bit));
        }
        if (state == (uint8_t)candidate) return state;
    }
    return 0u;
}

static void led_toggle(void) {
    LATA = (uint8_t)(LATA ^ 0x04u);
}

static void led_startup_test(void) {
    uint8_t index;
    for (index = 0u; index < 4u; ++index) {
        LATA |= 0x04u;
        __delay_ms(120);
        LATA &= (uint8_t)~0x04u;
        __delay_ms(120);
    }
}

static void symbol_delay(void) {
    /* The assembly build is preferred for exact 1.25 Msymbol/s timing. */
    __asm("nop");
    __asm("nop");
    __asm("nop");
    __asm("nop");
}

static void set_modulation_bit(uint8_t bit) {
    /* Coherent BPSK uses the absolute phase for every symbol. */
    MDCONbits.MDBIT = bit != 0u;
    symbol_delay();
}

static void send_marker(void) {
    int8_t bit;
    uint8_t index;
    for (index = 0u; index < 4u; ++index) set_modulation_bit(0u);
    for (bit = 15; bit >= 0; --bit) {
        set_modulation_bit((uint8_t)((YURLAPS_PREAMBLE >> bit) & 1u));
    }
}

static void send_protected_bit(uint8_t bit) {
    uint16_t register_value = (uint16_t)(((uint16_t)convolution_state << 1u)
                                         | (uint16_t)(bit & 1u));
    set_modulation_bit(parity16((uint16_t)(register_value & 0x01AFu)));
    set_modulation_bit(parity16((uint16_t)(register_value & 0x011Du)));
    convolution_state = (uint8_t)register_value;
}

static void send_frame(void) {
    uint8_t bytes[5];
    uint16_t checksum;
    uint8_t index;
    uint8_t bit;

    bytes[0] = (uint8_t)(YURLAPS_ID >> 16u);
    bytes[1] = (uint8_t)(YURLAPS_ID >> 8u);
    bytes[2] = (uint8_t)YURLAPS_ID;
    checksum = crc16(bytes);
    bytes[3] = (uint8_t)(checksum >> 8u);
    bytes[4] = (uint8_t)checksum;

    convolution_state = tail_biting_state(bytes);
    send_marker();
    for (index = 0u; index < 40u; ++index) {
        send_protected_bit(payload_bit(bytes, index));
    }
    for (bit = 0u; bit < 4u; ++bit) set_modulation_bit(0u);
}

static void setup_rf_path(void) {
    PORTA = 0u;
    LATA = 0u;
    ANSELA = 0u;
    ODCONA = 0u;
    TRISA = 0x08u;
    WPUA = 0x08u;
    SLRCONA = 0x04u;
    CLKRCON = 0x92u;

    T0CON1 = 0x40u;
    T0CON0 = 0x80u;

    MDCON = 0u;
    MDSRC = 0u;
    MDCARH = 0x23u;
    MDCARL = 0x63u;

    CLC1CON = 0x04u;
    CLC1SEL0 = 0x0Au;
    CLC1SEL1 = 0x1Fu;
    CLC1SEL2 = 0x1Fu;
    CLC1SEL3 = 0x1Fu;
    CLC1POL = 0u;
    CLC1GLS0 = 0x08u;
    CLC1GLS1 = 0x02u;
    CLC1GLS2 = 0u;
    CLC1GLS3 = 0u;

    CWG1CON0 = 0x04u;
    CWG1CON1 = 0u;
    CWG1CLKCON = 0u;
    CWG1DBR = 0u;
    CWG1DBF = 0u;
    CWG1AS1 = 0u;
    CWG1STR = 0u;
    CWG1DAT = 0x0Au;

    INTCONbits.GIE = 0u;
    PPSLOCK = 0x55u;
    PPSLOCK = 0xAAu;
    PPSLOCKbits.PPSLOCKED = 0u;
    RA0PPS = 0x08u;
    RA1PPS = 0x09u;
    PPSLOCK = 0x55u;
    PPSLOCK = 0xAAu;
    PPSLOCKbits.PPSLOCKED = 1u;
    INTCONbits.GIE = 1u;

    CLC1CON |= 0x80u;
    CWG1AS0 = 0xA8u;
    CWG1AS0bits.SHUTDOWN = 0u;
    CWG1CON0 |= 0x80u;
    MDCONbits.MDEN = 1u;
}

static uint16_t next_silence_us(void) {
    return (uint16_t)(750u + ((uint16_t)TMR0L * 1501u / 256u));
}

static void silence(void) {
    uint16_t duration = next_silence_us();
    uint16_t us;
    MDCONbits.MDEN = 0u;
    for (us = 0u; us < duration; ++us) __delay_us(1);
    MDCONbits.MDBIT = 0u;
    MDCONbits.MDEN = 1u;
}

void main(void) {
    setup_rf_path();
    led_startup_test();
    for (;;) {
        send_frame();
        ++led_divider;
        if (led_divider == 32u) {
            led_divider = 0u;
            led_toggle();
        }
        silence();
    }
}
