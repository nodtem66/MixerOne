#ifndef __MAIN__H_
#define __MAIN__H_

#include "util.h"
#include <Arduino.h>
#include <WString.h>
#include <LiquidCrystal_I2C.h>
#include <cocoos.h>

#ifdef PRODUCTION
#define UART Serial1
#else
#define UART Serial
#endif


#define NOT_FOUND -1
#define PARSE_CMD_MOTOR1    0
#define PARSE_CMD_MOTOR2    1
#define PARSE_CMD_DURATION  2

#define NEXT_BUTTON_PIN PA1
#define PREV_BUTTON_PIN PA0
#define REV_MOTOR_PIN PB0
#define ROT_MOTOR_PIN PB1

#define BUTTON_PRESSED_EVENT_DELAY_MILLIS 300
#define PWM_PERCENT_TO_SPEED(x) ((x) * 65535 / 100.0)

#define IS_DIGIT(x) ((x) >= '0' && (x) <= '9')
#define IS_LAST_INDEX(x,y) ((x) == ((y) - 1))

#define STATE_HOME 0
#define STATE_VIEW 1
#define STATE_RUNNING 2
#define STATE_TEST 3
uint8_t state = STATE_HOME;

String serial_rx = "";
bool session_running = false;
uint8_t session_page = 0;
uint32_t pressed_event_millis = 0;
Evt_t test_pwm_event;
Evt_t next_button_pressed_event;
Evt_t prev_button_pressed_event;
Evt_t session_display_event;
Evt_t session_running_event;
Evt_t home_event;

LiquidCrystal_I2C lcd(0x27, 20, 4);

#include "EEPROM_helper.h"

void on_assert(const char* filename, uint16_t line, const char* expr) {

    while(1) {
        UART.print("Assert: ");
        UART.print(expr);
        UART.print(" ");
        UART.print(filename);
        UART.print(" Line ");
        UART.println(line);

        delay(1000);
    }

}

void on_push_button(void) {
    if (millis() - pressed_event_millis > BUTTON_PRESSED_EVENT_DELAY_MILLIS) {
        if (digitalRead(NEXT_BUTTON_PIN) == LOW) {
            event_ISR_signal(next_button_pressed_event);
        }
        if (digitalRead(PREV_BUTTON_PIN) == LOW) {
            event_ISR_signal(prev_button_pressed_event);
        }
    }
    pressed_event_millis = millis();
}

void turn_on_motor(uint16_t speed1, uint16_t speed2) {
    pwmWrite(REV_MOTOR_PIN, speed1);
    pwmWrite(ROT_MOTOR_PIN, speed2);
}

void turn_off_motor() {
    pwmWrite(REV_MOTOR_PIN, 0);
    pwmWrite(ROT_MOTOR_PIN, 0);
}

void show_session(session_t *list, int length) {
    UART.print("Total Session: "); UART.println(length);
    for (int i=0; i<length; i++) {
        UART.print("Session "); UART.print(i+1);
        UART.print("   Revolution Speed: "); UART.print(list[i].motor1_pwm);
        UART.print("%  Rotation Speed:   "); UART.print(list[i].motor2_pwm);
        UART.print("%  Duration (min):   "); UART.println(list[i].duration_minute);
    }
}

void show_display_menu(int page=0) {
    if (page > 0) {
        lcd.setCursor(0, 3);
        lcd.print("< Prev");
    }
    if (IS_LAST_INDEX(page, session_length)) {
        lcd.setCursor(15, 3);
        lcd.print("Run >");
    } else {
        lcd.setCursor(14, 3);
        lcd.print("Next >");
    }
}

void show_running_menu() {
    lcd.setCursor(0, 3);
    lcd.print("< Resume     Pause >");
}

void show_timer(uint16_t minute, uint8_t second) {
    lcd.setCursor(7, 2);
    lcd.print("             ");
    lcd.setCursor(7, 2);
    lcd.print(minute);
    lcd.print("m");
    lcd.print(second);
    lcd.print("s");
}

void lcd_display_home() {
    lcd.clear();
    lcd.setCursor(3,1);
    lcd.print("MixerOne v1.0");
    lcd.setCursor(0,3);
    lcd.print("< Clear       Load >");
}

void lcd_display_test(uint16_t a, uint16_t b) {
    lcd.clear();
    lcd.home();
    lcd.print("TEST PWM");
    lcd.setCursor(0, 1);
    lcd.print("Revolution: "); lcd.print(a);
    lcd.setCursor(0, 2);
    lcd.print("Rotation:   "); lcd.print(b);
}

void lcd_display_session(int page=0) {
    if (page < session_length) {
        lcd.clear();
        lcd.home();
        lcd.print("Session "); lcd.print(page+1); lcd.print(" "); lcd.print(session_list[page].duration_minute); lcd.print("min");
        lcd.setCursor(0, 1);
        lcd.print("Revolution: "); lcd.print(session_list[page].motor1_pwm); lcd.print("%");
        lcd.setCursor(0, 2);
        lcd.print("Rotation:   "); lcd.print(session_list[page].motor2_pwm); lcd.print("%");
        show_display_menu(page);
        session_page = page;
    }
}

void lcd_display_running(int page=0) {
    if (page < session_length) {
        lcd.clear();
        lcd.home();
        lcd.print("Session "); lcd.print(page+1); lcd.print(" "); lcd.print(session_list[page].duration_minute); lcd.print("min");
        lcd.setCursor(0, 1);
        lcd.print("Rev: "); lcd.print(session_list[page].motor1_pwm); lcd.print("%");
        lcd.setCursor(10, 1);
        lcd.print("Rot:   "); lcd.print(session_list[page].motor2_pwm); lcd.print("%");
        lcd.setCursor(0, 2);
        lcd.print("Left: "); 
        show_timer(session_list[page].duration_minute, 0);
        show_running_menu();
    }
}

uint8_t parse_command(String *cmd, session_t *list, uint8_t *length) {
    
    unsigned int l = cmd->length(), i=1, j=0;
    if (l < 2) return 1;
    if (cmd->charAt(0) != 'N') return 2;
    if (cmd->charAt(l-1) == '\r') l--;

    *length = 0;
    // Parse N__:_,_,_.
    // Step 1: Parse N__:
    for (;i<l;i++) {
        char b = cmd->charAt(i);
        if (b == ':') {i++; break;}
        if (IS_DIGIT(b)) *length = 10*(*length) + b - '0';
        else return 4;
    }
    if (*length <= 0) return 5;

    // Step 2: Parse _,_,_._,_,_.
    char state = PARSE_CMD_MOTOR1;
    list[0] = {0,0,0};
    for (;i<l;i++) {
        char b = cmd->charAt(i);
        if (IS_DIGIT(b)) {
            if (state == PARSE_CMD_MOTOR1) 
                list[j].motor1_pwm = 10*list[j].motor1_pwm + b - '0';    
            else if (state == PARSE_CMD_MOTOR2)
                list[j].motor2_pwm = 10*list[j].motor2_pwm + b - '0';
            else if (state == PARSE_CMD_DURATION)
                list[j].duration_minute = 10*list[j].duration_minute + b - '0';   
        } else if (b == ',') {
            if (state == PARSE_CMD_MOTOR1)
                state = PARSE_CMD_MOTOR2;
            else if (state == PARSE_CMD_MOTOR2)
                state = PARSE_CMD_DURATION;
            else {
                *length = 0;
                return 6;
            }
        } else if (b == '.' && state == PARSE_CMD_DURATION) {
            j++;
            if (j > *length && i < l) {
                *length = 0;
                return 7;
            }
            list[j] = {0,0,0};
            state = PARSE_CMD_MOTOR1;
        } else {
            *length = 0;
            return 8;
        }
    }
    return 0;
}

void test_pwm(String *cmd) {
    byte b;
    unsigned int l = cmd->length();
    if (l >= 2) {
        unsigned int i=1;
        uint16_t cycle1 = 0, cycle2 = 0;
        for (; i<l; i++) {
            b = cmd->charAt(i);
            if (!IS_DIGIT(b)) break;
            cycle1 = cycle1*10 + (b - '0');
        }
        for (i++; i<l; i++) {
            b = cmd->charAt(i);
            if (!IS_DIGIT(b)) break;
            cycle2 = cycle2*10 + (b - '0');
        }
        turn_on_motor(cycle1, cycle2);

        lcd_display_test(cycle1, cycle2);
    }
}

/*=== Main CODE ===================================*/
__attribute__(( constructor )) void premain() {
    init();
}

#endif