    LIST P=PIC16F1503
    INCLUDE "p16f1503.inc"
    __CONFIG _CONFIG1, _CLKOUTEN_OFF & _BOREN_ON & _CP_OFF & _MCLRE_ON & _PWRTE_ON & _WDTE_OFF & _FOSC_INTOSC
    __CONFIG _CONFIG2, _LVP_ON & _LPBOR_OFF & _BORV_LO & _STVREN_ON & _WRT_ALL

INPUT_WORK EQU 0x20
OUTPUT_WORK EQU 0x21

    ORG 0
    ; initialize ports
    ; disable analog mode
    MOVLW D'3'
    MOVWF BSR
    CLRF ANSELA
    CLRF ANSELC
    ; set RC0-5 and RA5 to output
    MOVLW D'1'
    MOVWF BSR
    BCF TRISA, TRISA5
    CLRF TRISC
    ; enable weak pull-up
    BCF OPTION_REG, NOT_WPUEN
    ; return to BANK 0
    CLRF BSR

MAIN_LOOP
    ; read input value
    MOVF PORTA, W
    ANDLW 0x17
    MOVWF INPUT_WORK
    BTFSC INPUT_WORK, 4
    BSF INPUT_WORK, 3
    ; convert to font
    MOVF INPUT_WORK, W
    CALL FONT_TABLE
    ; output font
    MOVWF OUTPUT_WORK
    MOVWF PORTC
    BTFSC OUTPUT_WORK, 6
    BSF PORTA, 5
    BTFSS OUTPUT_WORK, 6
    BCF PORTA, 5
    ; do this again
    GOTO MAIN_LOOP

FONT_TABLE
    ANDLW 0xF
    ADDWF PCL, F

    RETLW 0x3F
    RETLW 0x06
    RETLW 0x5B
    RETLW 0x4F

    RETLW 0x66
    RETLW 0x6D
    RETLW 0x7D
    RETLW 0x07

    RETLW 0x7F
    RETLW 0x6F
    RETLW 0x77
    RETLW 0x7C

    RETLW 0x39
    RETLW 0x5E
    RETLW 0x79
    RETLW 0x71

    END
