; YurLaps PIC16F18313 fixed-timing transmitter prototype
;
; This file emits the YurLaps test frame for ID 2504260 using the
; PIC16F18313 DSM/CWG path. The 104 transmitted bits are differential phase
; bits. The byte stream was generated from the YurLaps ID, sequence zero,
; CRC-8, K=9 rate-1/2 payload, and a four-bit tail.
;
; Build with MPASM for PIC16F18313. This file is independent firmware and is
; not a copy of an OpenStint or RC Hourglass message table.

#include "p16f18313.inc"

__CONFIG _CONFIG1, _FEXTOSC_HS & _RSTOSC_EXT4X & _CLKOUTEN_OFF & _CSWEN_OFF & _FCMEN_OFF
__CONFIG _CONFIG2, _MCLRE_OFF & _PWRTE_OFF & _WDTE_OFF & _LPBOREN_OFF & _BOREN_ON & _BORV_LOW & _STVREN_ON & _DEBUG_OFF
__CONFIG _CONFIG3, _WRT_OFF & _LVP_ON
__CONFIG _CONFIG4, _CP_OFF & _CPD_OFF

SHARED_DATA UDATA_SHR
message_ram RES 13

BANKED_DATA UDATA
delay_low  RES 1
delay_high RES 1
led_count  RES 1

SEND_BIT MACRO source
    ; One symbol is one fixed-time macro expansion. A one toggles DSM phase.
    NOP
    LSLF source, 1
    BTFSC STATUS, C
    XORWF MDCON, 1
    ENDM

SEND_BYTE MACRO source
    SEND_BIT source
    SEND_BIT source
    SEND_BIT source
    SEND_BIT source
    SEND_BIT source
    SEND_BIT source
    SEND_BIT source
    SEND_BIT source
    ENDM

RES_VECTOR CODE 0x0000
    GOTO setup

ISR_VECTOR CODE 0x0004
    RETFIE

MAIN CODE

setup
    BANKSEL PORTA
    CLRF PORTA
    BANKSEL LATA
    CLRF LATA
    BANKSEL ANSELA
    CLRF ANSELA
    BANKSEL ODCONA
    CLRF ODCONA
    BANKSEL TRISA
    MOVLW B'00001000'       ; RA3 input, RA0/RA1 RF, RA2 LED
    MOVWF TRISA
    BANKSEL SLRCONA
    MOVLW B'00000100'
    MOVWF SLRCONA

    BANKSEL CLKRCON
    MOVLW B'10010010'       ; 5 MHz reference from external 4x clock
    MOVWF CLKRCON

    BANKSEL MDCON
    CLRF MDCON
    CLRF MDSRC
    MOVLW B'00100011'
    MOVWF MDCARH
    MOVLW B'01100011'
    MOVWF MDCARL

    BANKSEL CLC1CON
    MOVLW B'00000100'
    MOVWF CLC1CON
    MOVLW B'00001010'
    MOVWF CLC1SEL0
    MOVLW B'00011111'
    MOVWF CLC1SEL1
    MOVWF CLC1SEL2
    MOVWF CLC1SEL3
    CLRF CLC1POL
    MOVLW B'00001000'
    MOVWF CLC1GLS0
    MOVLW B'00000010'
    MOVWF CLC1GLS1
    CLRF CLC1GLS2
    CLRF CLC1GLS3

    BANKSEL CWG1CON0
    BCF CWG1CON0, 7
    MOVLW B'00000100'
    MOVWF CWG1CON0
    CLRF CWG1CON1
    CLRF CWG1CLKCON
    CLRF CWG1DBR
    CLRF CWG1DBF
    CLRF CWG1AS1
    CLRF CWG1STR
    MOVLW B'00001010'
    MOVWF CWG1DAT
    MOVLW B'10101000'
    MOVWF CWG1AS0

    ; PPS: CWG1A to RA0 and CWG1B to RA1
    BCF INTCON, GIE
    BANKSEL PPSLOCK
    MOVLW 0x55
    MOVWF PPSLOCK
    MOVLW 0xAA
    MOVWF PPSLOCK
    BCF PPSLOCK, PPSLOCKED
    BANKSEL RA0PPS
    MOVLW 0x08
    MOVWF RA0PPS
    BANKSEL RA1PPS
    MOVLW 0x09
    MOVWF RA1PPS
    BANKSEL PPSLOCK
    MOVLW 0x55
    MOVWF PPSLOCK
    MOVLW 0xAA
    MOVWF PPSLOCK
    BSF PPSLOCK, PPSLOCKED
    BSF INTCON, GIE

    BANKSEL CLC1CON
    BSF CLC1CON, 7
    BANKSEL CWG1CON0
    BSF CWG1CON0, 7
    BANKSEL CWG1AS0
    BCF CWG1AS0, SHUTDOWN
    BANKSEL MDCON
    BCF MDCON, MDBIT
    BSF MDCON, MDEN

    ; 0A E6 81 18 80 FE 2B 27 F5 00 C2 55 90
    ; 4 init + 0xAE68 marker + 80 tail-biting FEC/CRC bits + four tail bits
    BANKSEL message_ram
    MOVLW 0x0A
    MOVWF message_ram+0
    MOVLW 0xE6
    MOVWF message_ram+1
    MOVLW 0x81
    MOVWF message_ram+2
    MOVLW 0x18
    MOVWF message_ram+3
    MOVLW 0x80
    MOVWF message_ram+4
    MOVLW 0xFE
    MOVWF message_ram+5
    MOVLW 0x2B
    MOVWF message_ram+6
    MOVLW 0x27
    MOVWF message_ram+7
    MOVLW 0xF5
    MOVWF message_ram+8
    MOVLW 0x00
    MOVWF message_ram+9
    MOVLW 0xC2
    MOVWF message_ram+10
    MOVLW 0x55
    MOVWF message_ram+11
    MOVLW 0x90
    MOVWF message_ram+12

send_frame
    BANKSEL MDCON
    BCF MDCON, MDBIT
    SEND_BYTE (message_ram+0)
    SEND_BYTE (message_ram+1)
    SEND_BYTE (message_ram+2)
    SEND_BYTE (message_ram+3)
    SEND_BYTE (message_ram+4)
    SEND_BYTE (message_ram+5)
    SEND_BYTE (message_ram+6)
    SEND_BYTE (message_ram+7)
    SEND_BYTE (message_ram+8)
    SEND_BYTE (message_ram+9)
    SEND_BYTE (message_ram+10)
    SEND_BYTE (message_ram+11)
    SEND_BYTE (message_ram+12)
    NOP
    NOP

    BANKSEL LATA
    XORWF LATA, 1
    BANKSEL delay_low
    MOVLW 0x40
    MOVWF delay_low
    MOVLW 0x06
    MOVWF delay_high
delay_loop
    DECFSZ delay_low, 1
    GOTO delay_loop
    DECFSZ delay_high, 1
    GOTO delay_loop
    GOTO send_frame

    END
