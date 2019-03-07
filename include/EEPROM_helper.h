#ifndef _MY_EEPROM_HELPER_H_
#define _MY_EEPROM_HELPER_H_

#include <EEPROM.h>

#define DATA_READ_EXP(ADDR, VAR, EXP) tmp = EEPROM.read(ADDR); VAR = EXP;
#define DATA_READ(ADDR, VAR) DATA_READ_EXP(ADDR, VAR, tmp)

// EEPROM SECTION NOTE
// BASE ADDRESS 0x08001F00 FOR STM32F103C8 128K ROM
// END ADDRESS 0x08002000 PAGE SIZE 2048
// VARIABLE 1: session_list 
//       size: 2*3*10 = 60 byte
// virtual addr: 0x01 - 0x3c
// VARIABLE 2: session_length
//       size: 1 byte
// virtual addr: 0x3d
typedef struct {
    uint16_t motor1_pwm;
    uint16_t motor2_pwm;
    uint16_t duration_minute;
} session_t;
session_t session_list[10];
uint8_t session_length = 0;

#define SESSION_LIST_ADDR(x,y) (3*(x)+(y)+1) 
#define SESSION_LENGTH_ADDR 0x3d

void load_EEPROM(void) {
    // Read Session Length
    uint16_t tmp = 0;
    DATA_READ_EXP(SESSION_LENGTH_ADDR, session_length, tmp & 0xff);

    // Read Session List
    for (uint8_t i=0; i<session_length; i++) {
        DATA_READ(SESSION_LIST_ADDR(i,0), session_list[i].motor1_pwm);
        DATA_READ(SESSION_LIST_ADDR(i,1), session_list[i].motor2_pwm);
        DATA_READ(SESSION_LIST_ADDR(i,2), session_list[i].duration_minute);
    }
}

void save_EEPROM(void) {
    // Write Session Length
    EEPROM.write(SESSION_LENGTH_ADDR, session_length);
    // Write Session List
    for (uint8_t i=0; i<session_length; i++) {
        EEPROM.write(SESSION_LIST_ADDR(i, 0), session_list[i].motor1_pwm);
        EEPROM.write(SESSION_LIST_ADDR(i, 1), session_list[i].motor2_pwm);
        EEPROM.write(SESSION_LIST_ADDR(i, 2), session_list[i].duration_minute);
    }
}

void clear_EEPROM(void) {
    for (uint32_t i=0x01; i<=SESSION_LENGTH_ADDR; i++) 
        EEPROM.write(i, 0);
}

#endif