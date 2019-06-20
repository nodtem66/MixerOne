#ifndef __MAIN__H_
#define __MAIN__H_

#define PWM_FREQUENCY 10
#define PWM_RESOLUTION 8
#define PWM_MAX_DUTY_CYCLE 255

#ifdef PRODUCTION
#define UART Serial1
#else
#define UART Serial
#endif

#include <Arduino.h>
#include <WString.h>

#define NEXT_BUTTON_PIN PA1
#define PREV_BUTTON_PIN PA0

// Usage 2: based on 
// Article: http://www.hessmer.org/blog/2013/12/28/ibt-2-h-bridge-with-arduino/
#define REV_MOTOR_PIN PB0
#define REV_MOTOR_R_IS PA5
#define REV_MOTOR_L_IS PA4
#define REV_MOTOR_R_PWM PB11
#define REV_MOTOR_L_PWM PB10
#define REV_MOTOR_EN REV_MOTOR_R_PWM

#define ROT_MOTOR_PIN PB1
#define ROT_MOTOR_R_IS PA3
#define ROT_MOTOR_L_IS PA2
#define ROT_MOTOR_R_PWM PA7
#define ROT_MOTOR_L_PWM PA6
#define ROT_MOTOR_EN ROT_MOTOR_R_PWM

#define FAILURE_VOLTAGE 500 // Target 1.6V (Calulated from 1024 * 1.6 / 3.3)

#define BUTTON_PRESSED_EVENT_DELAY_MILLIS 300
#define PWM_PERCENT_TO_SPEED(x) ((x) * PWM_MAX_DUTY_CYCLE / 100.0)

#define NOT_FOUND -1
#define PARSE_CMD_MOTOR1    0
#define PARSE_CMD_MOTOR2    1
#define PARSE_CMD_DURATION  2
#define MY_ASSERT(x) configASSERT(x)

#define IS_DIGIT(x) ((x) >= '0' && (x) <= '9')
#define IS_LAST_INDEX(x,y) ((x) == ((y) - 1))

// State system
#define STATE_NONE      -1
#define STATE_HOME      0
#define STATE_VIEW      1
#define STATE_RUNNING   2
#define STATE_TEST      3
uint8_t state = STATE_HOME;

// Notify system 
#define BIT_NEXT_BUTTON     (1 << 0)
#define BIT_PREV_BUTTON     (1 << 1)
#define BIT_NEW_TEST        (1 << 2)
#define BIT_NEW_VIEW        (1 << 3)
#define BIT_REFRESH         (1 << 4)
#define IS_BIT_SET(x,b) ((x & (b)) != 0)
#define BIT_SET(x,b) (x |= (b))
#define BIT_CLEAR(x,b) (x &= ~(b))
uint8_t bit_notify = 0;


String serial_rx = "";
bool session_running = false;
bool is_fail = false;
uint8_t session_page = 0;
uint16_t rev_pwm = 0;
uint16_t rot_pwm = 0;
uint32_t pressed_event_millis = 0;
stimer_t timer4;


#include "EEPROM_helper.h"

void serial_command_task(void);
static inline void display_home(bool is_render);
static inline void display_testpwm(bool is_render);
static inline void display_session(bool is_first);
static inline void display_running(bool);
static inline void display_task(void);

void on_assert(const char* filename, uint16_t line, const char* expr) {
    //Timer4.setPrescaleFactor(312);
    while(1) {
        
        UART.print("Assert: ");
        UART.print(expr);
        UART.print(" ");
        UART.print(filename);
        UART.print(" Line ");
        UART.println(line);
        delay(2000);
    }
}

// Depreciated: Use UART COMMAND
void on_push_button(void) {
    if (millis() - pressed_event_millis > BUTTON_PRESSED_EVENT_DELAY_MILLIS) {
        if (digitalRead(NEXT_BUTTON_PIN) == LOW) {
            BIT_SET(bit_notify, BIT_NEXT_BUTTON);
        }
        if (digitalRead(PREV_BUTTON_PIN) == LOW) {
            BIT_SET(bit_notify, BIT_PREV_BUTTON);
        }
    }
    pressed_event_millis = millis();
}

void status_led_task() {
    digitalToggle(LED_BUILTIN);
}

void motor_init() {
    pinMode(REV_MOTOR_PIN, OUTPUT);
    pinMode(REV_MOTOR_L_PWM, OUTPUT);
    pinMode(REV_MOTOR_R_PWM, OUTPUT);
    pinMode(REV_MOTOR_L_IS, INPUT_ANALOG);
    pinMode(REV_MOTOR_R_IS, INPUT_ANALOG);

    pinMode(ROT_MOTOR_PIN, OUTPUT);
    pinMode(ROT_MOTOR_L_PWM, OUTPUT);
    pinMode(ROT_MOTOR_R_PWM, OUTPUT);
    pinMode(ROT_MOTOR_L_IS, INPUT_ANALOG);
    pinMode(ROT_MOTOR_R_IS, INPUT_ANALOG);

    // Change this if direction is not correct
    digitalWrite(REV_MOTOR_L_PWM, LOW);
    digitalWrite(REV_MOTOR_R_PWM, LOW);
    digitalWrite(ROT_MOTOR_L_PWM, LOW);
    digitalWrite(ROT_MOTOR_R_PWM, LOW);
}

void turn_on_motor(uint16_t speed1, uint16_t speed2) {
    digitalWrite(REV_MOTOR_EN, HIGH);
    digitalWrite(ROT_MOTOR_EN, HIGH);
    analogWrite(REV_MOTOR_PIN, speed1);
    analogWrite(ROT_MOTOR_PIN, speed2);
}

void turn_off_motor() {
    digitalWrite(REV_MOTOR_EN, LOW);
    digitalWrite(ROT_MOTOR_EN, LOW);
    analogWrite(REV_MOTOR_PIN, 0);
    analogWrite(ROT_MOTOR_PIN, 0);
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
        UART.print("(0) Prev");
    }
    if (IS_LAST_INDEX(page, session_length)) {
        UART.println("(1) Run");
    } else {
        UART.println("(1) Next");
    }
}

void show_running_menu() {
    if (session_running)
        UART.println("(1) Pause");
    else
        UART.println("(0) Resume (1) END");
}

void show_timer(uint16_t minute, uint8_t second) {
    UART.print(minute);
    UART.print("m");
    UART.print(second);
    UART.println("s");
}

void lcd_display_home() {
    UART.println("MixerOne v1.0");
    UART.println("(0) Clear (1) Load");
}

void lcd_display_test(uint16_t a, uint16_t b) {
    UART.println("TEST PWM");
    UART.print("Revolution: "); UART.println(a);
    UART.print("Rotation:   "); UART.println(b);
}

void lcd_display_session(int page=0) {
    if (page < session_length) {
        UART.print("Session "); UART.print(page+1); UART.print(" "); UART.print(session_list[page].duration_minute); UART.println("min");
        UART.print("Revolution: "); UART.print(session_list[page].motor1_pwm); UART.println('%');
        UART.print("Rotation:   "); UART.print(session_list[page].motor2_pwm); UART.println('%');
        show_display_menu(page);
        session_page = page;
    }
}

void lcd_display_running(int page=0) {
    if (page < session_length) {
        UART.print("Session "); UART.print(page+1); UART.print(" "); UART.print(session_list[page].duration_minute); UART.println("min");
        UART.print("Rev: "); UART.print(session_list[page].motor1_pwm); UART.println('%');
        UART.print("Rot:   "); UART.print(session_list[page].motor2_pwm); UART.println('%');
        UART.print("Left: "); 
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

static inline void test_pwm(String *cmd) {
    byte b;
    unsigned int l = cmd->length();
    if (l >= 2) {
        unsigned int i=1;
        rev_pwm = 0; rot_pwm = 0;
        for (; i<l; i++) {
            b = cmd->charAt(i);
            if (!IS_DIGIT(b)) break;
            rev_pwm = rev_pwm*10 + (b - '0');
        }
        for (i++; i<l; i++) {
            b = cmd->charAt(i);
            if (!IS_DIGIT(b)) break;
            rot_pwm = rot_pwm*10 + (b - '0');
        }
    }
}

void on_fail() {
    while (is_fail) {
        digitalWrite(REV_MOTOR_L_PWM, LOW);
        digitalWrite(REV_MOTOR_R_PWM, LOW);
        digitalWrite(ROT_MOTOR_L_PWM, LOW);
        digitalWrite(ROT_MOTOR_R_PWM, LOW);
        turn_off_motor();

        UART.println("MOTOR ERROR");
        delay(2000);
    }
}

void watchdog(stimer_t*) {
    
    digitalToggle(LED_BUILTIN);
    
    if (is_fail)
        return;
    
    if (analogRead(REV_MOTOR_L_IS) >= FAILURE_VOLTAGE ||
        analogRead(REV_MOTOR_R_IS) >= FAILURE_VOLTAGE ||
        analogRead(ROT_MOTOR_L_IS) >= FAILURE_VOLTAGE ||
        analogRead(ROT_MOTOR_R_IS) >= FAILURE_VOLTAGE) {
        is_fail = true;
        on_fail();
    }
}
#endif