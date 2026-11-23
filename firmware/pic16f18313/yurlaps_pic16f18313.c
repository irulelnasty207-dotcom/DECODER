/*
 * YurLaps PIC16F18313 transmitter prototype.
 *
 * This source emits the YurLaps test protocol. It does not contain the
 * RC Hourglass message tables or its decoder protocol. The RF output stage
 * still has to match the user's PIC16F18313 transponder PCB.
 *
 * Build with MPLAB XC8 for PIC16F18313. Set YURLAPS_ID for each unit.
 */

#include <stdint.h>
#include <xc.h>

#define _XTAL_FREQ 20000000UL

/* External oscillator and 4x PLL are required by the 5 MHz DSM reference. */
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

static uint8_t sequence;
static uint8_t convolution_state;
static uint8_t led_divider;
static uint8_t phase_state;

static void led_toggle(void) {
    LATA ^= 0x04u;
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

static uint8_t parity16(uint16_t value) {
    uint8_t result = 0;
    while (value != 0u) {
        result ^= (uint8_t)(value & 1u);
        value >>= 1u;
    }
    return result;
}

static uint8_t crc8(uint32_t id, uint8_t seq) {
    uint8_t bytes[4];
    uint8_t crc = 0;
    uint8_t index;
    uint8_t bit;
    uint8_t carry;
    bytes[0] = (uint8_t)(id >> 16u);
    bytes[1] = (uint8_t)(id >> 8u);
    bytes[2] = (uint8_t)id;
    bytes[3] = seq;
    for (index = 0; index < 4u; ++index) {
        crc ^= bytes[index];
        for (bit = 0; bit < 8u; ++bit) {
            carry = (uint8_t)(crc & 0x80u);
            crc <<= 1u;
            if (carry != 0u) crc ^= 0x07u;
        }
    }
    return crc;
}

static void symbol_delay(void) {
    /* Four instruction cycles at 20 MHz = 0.8 us, or 1.25 MS/s. */
    __asm("nop");
    __asm("nop");
    __asm("nop");
    __asm("nop");
}

static void set_modulation_bit(uint8_t bit) {
    /* RC Hourglass uses differential BPSK. A one toggles the carrier phase,
       while a zero leaves the previous phase unchanged. */
    if (bit != 0u) phase_state ^= 1u;
    MDCONbits.MDBIT = phase_state != 0u;
    symbol_delay();
}

static void send_marker(void) {
    uint8_t index;
    int8_t bit;
    /* Four low init symbols followed by the YurLaps 0xA65C marker. */
    for (index = 0; index < 4u; ++index) set_modulation_bit(0u);
    for (bit = 15; bit >= 0; --bit)
        set_modulation_bit((uint8_t)((0xA65Cu >> bit) & 1u));
}

static void send_protected_bit(uint8_t bit) {
    uint16_t register_value;
    register_value = (uint16_t)(((uint16_t)convolution_state << 1u)
                                | (uint16_t)(bit & 1u));
    set_modulation_bit(parity16((uint16_t)(register_value & 0x01AFu)));
    set_modulation_bit(parity16((uint16_t)(register_value & 0x011Du)));
    convolution_state = (uint8_t)(register_value & 0x00FFu);
}

static void send_payload_byte(uint8_t value) {
    int8_t bit;
    for (bit = 7; bit >= 0; --bit)
        send_protected_bit((uint8_t)((value >> bit) & 1u));
}

static void send_frame(void) {
    uint8_t id_high;
    uint8_t id_middle;
    uint8_t id_low;
    uint8_t checksum;

    id_high = (uint8_t)(YURLAPS_ID >> 16u);
    id_middle = (uint8_t)(YURLAPS_ID >> 8u);
    id_low = (uint8_t)YURLAPS_ID;
    checksum = crc8(YURLAPS_ID, sequence);

    convolution_state = 0u;
    phase_state = 0u;
    send_marker();
    send_payload_byte(id_high);
    send_payload_byte(id_middle);
    send_payload_byte(id_low);
    send_payload_byte(sequence);
    send_payload_byte(checksum);
}

static void setup_rf_path(void) {
    /* These values select the 5 MHz DSM reference used by the PCB. */
    /* RA0/RA1 are the two RF outputs. RA2 is the status LED. RA3 stays input. */
    PORTA = 0u;
    LATA = 0u;
    ANSELA = 0u;
    ODCONA = 0u;
    TRISA = 0x08u;
    WPUA = 0x08u;
    SLRCONA = 0x04u;
    CLKRCON = 0x92u;

    /* Free-running Timer0 supplies repeat jitter. */
    T0CON1 = 0x40u;
    T0CON0 = 0x80u;

    MDCON = 0u;
    MDSRC = 0u;
    MDCARH = 0x23u;
    MDCARL = 0x63u;

    /* CLC1 routes DSM output to the board's RF path. */
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
    /* Route CWG1A/CWG1B to RA0/RA1 through PPS. */
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
    uint16_t random_part;
    /* Timer0 is free-running. Every value maps to 750..2250 us. */
    random_part = (uint16_t)TMR0L * 1501u / 256u;
    return (uint16_t)(750u + random_part);
}

static void silence(void) {
    uint16_t duration;
    uint16_t us;
    duration = next_silence_us();
    MDCONbits.MDEN = 0u;
    for (us = 0; us < duration; ++us) __delay_us(1);
    MDCONbits.MDBIT = 0u;
    MDCONbits.MDEN = 1u;
}

void main(void) {
    setup_rf_path();
    LATA &= (uint8_t)~0x04u;
    led_startup_test();
    for (;;) {
        send_frame();
        ++sequence;
        ++led_divider;
        if (led_divider >= 32u) {
            led_divider = 0u;
            led_toggle();
        }
        silence();
    }
}
