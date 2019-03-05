#include "main.h"

void gpio_init(void) {
    turn_off_motor();
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(REV_MOTOR_PIN, PWM);
    pinMode(ROT_MOTOR_PIN, PWM);
    pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
    pinMode(PREV_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(NEXT_BUTTON_PIN, on_push_button, FALLING);
    attachInterrupt(PREV_BUTTON_PIN, on_push_button, FALLING);
    
    Timer3.setPrescaleFactor(36); // PWM 30-40Hz
    
    Timer4.setOverflow(57600);
    Timer4.setPrescaleFactor(625); // LED status 2Hz
    Timer4.attachInterrupt(0, status_led_task); // attach to led task
    
    UART.begin(9600);
    
    lcd.init();
    lcd.backlight();

    ASSERT( EEPROM.init() == EEPROM_OK );
    load_EEPROM();
}

void serial_command_task(void) {
    byte b;
    uint8_t success = 0;
    
    while (UART.available() > 0) {
        b = UART.read();
        if (b == '\n') break;
        serial_rx += (char) b;
    }        
    if (b == '\n') {
        if (serial_rx.startsWith("S")) {
            show_session(session_list, session_length);
        } else if (serial_rx.startsWith("HI")) {
            UART.println("HI");
        } else if (serial_rx.startsWith("T")) {
            if (!session_running) {
                test_pwm(&serial_rx);
                state = STATE_TEST;
            }
        } else if (serial_rx.length() > 0) {
            success = parse_command(&serial_rx, session_list, &session_length);
            if (success == 0)  {
                UART.println("OK");
                if (!session_running) {
                    save_EEPROM();
                    state = STATE_VIEW;
                }
            }
            else { UART.print("ERROR "); UART.println(success); }
        }
        serial_rx = "";
        b = '\0';
    }
}

static inline void display_home(boolean is_render) {
    if (is_render)
        lcd_display_home();
    if ( IS_BIT_SET(bit_notify, BIT_NEXT_BUTTON) ) {
        BIT_CLEAR(bit_notify, BIT_NEXT_BUTTON);
        load_EEPROM();
        if (session_length > 0) {
            state = STATE_VIEW;
        } else {
            lcd.setCursor(0,2);
            lcd.print("     Not found");
            delay(1000);
            lcd_display_home();
        }
    }
    else if ( IS_BIT_SET(bit_notify, BIT_PREV_BUTTON) ) {
        BIT_CLEAR(bit_notify, BIT_NEXT_BUTTON);
        lcd.setCursor(0,2);
        lcd.print(clear_EEPROM() == EEPROM_OK ? "      Success" : "       Error");
        delay(1000);
        lcd_display_home();
    }
}

static inline void display_testpwm(boolean is_render) {
    if (is_render) {
        lcd_display_test(rev_pwm, rot_pwm);
        turn_on_motor(rev_pwm, rot_pwm);
    }
    if ( IS_BIT_SET(bit_notify, BIT_NEXT_BUTTON) || IS_BIT_SET(bit_notify, BIT_PREV_BUTTON)) {
        BIT_CLEAR(bit_notify, BIT_NEXT_BUTTON | BIT_PREV_BUTTON);
        state = STATE_HOME;
    }
}

static inline void display_session(boolean is_first) {
    static uint8_t i=0;
    boolean is_render = false;
    if (is_first) {
        i = 0;
        is_render = true;
    }
    if ( IS_BIT_SET(bit_notify, BIT_NEXT_BUTTON) ) {
        BIT_CLEAR(bit_notify, BIT_NEXT_BUTTON);
        i++;
        if (i >= session_length) {
            state = STATE_RUNNING;
            return;
        }
        is_render = true;
    } else if ( IS_BIT_SET(bit_notify, BIT_PREV_BUTTON) ) {
        BIT_CLEAR(bit_notify, BIT_PREV_BUTTON);
        if (i-1 >= 0) {
            i--;
            is_render = true;
        }
    }
    if (is_render) lcd_display_session(i);
}

static inline void display_running() {
    static uint16_t minute = 0;
    static int8_t second = 0;
    session_running = true;
    for (session_page = 0;session_page < session_length; session_page++) {
        lcd_display_running(session_page);
        turn_on_motor(
            PWM_PERCENT_TO_SPEED(session_list[session_page].motor1_pwm),
            PWM_PERCENT_TO_SPEED(session_list[session_page].motor2_pwm)
        );
        minute = session_list[session_page].duration_minute - 1;
        for (; minute >=0; minute--) {
            for (second = 60; second >= 0;) {
                if (session_running) show_timer(minute, second);                
                // WHEN PAUSE
                if ( IS_BIT_SET(bit_notify, BIT_NEXT_BUTTON) ) {
                    BIT_CLEAR(bit_notify, BIT_NEXT_BUTTON);
                    if (session_running) {
                        session_running = false;
                        lcd.setCursor(13, 3);
                        lcd.print("  End >");
                        turn_off_motor();
                    } else {
                        turn_off_motor();
                        state = STATE_VIEW;
                        goto finish;
                    }
                }
                // WHEN RESUME
                else if ( IS_BIT_SET(bit_notify, BIT_PREV_BUTTON) ) {
                    BIT_CLEAR(bit_notify, BIT_PREV_BUTTON);
                    if (!session_running) {
                        session_running = true;
                        lcd.setCursor(13, 3);
                        lcd.print("Pause >");
                        turn_on_motor(
                            PWM_PERCENT_TO_SPEED(session_list[session_page].motor1_pwm),
                            PWM_PERCENT_TO_SPEED(session_list[session_page].motor2_pwm)
                        );
                    } else {
                        session_running = false;
                        lcd.setCursor(13, 3);
                        lcd.print("  End >");
                        turn_off_motor();
                    }
                }
                if (session_running) second--;
            }
            if (minute == 0) break;
        }
    }
finish:
    session_running = false;
}

static inline void display_task(void) {
    static uint8_t local_state = STATE_NONE;
    boolean is_render = (local_state != state);
    switch (state) {
        case STATE_HOME:
            display_home(is_render);
            break;
        case STATE_TEST:
            display_testpwm(is_render);
            break;
        case STATE_VIEW:
            display_session(is_render);
            break;
        case STATE_RUNNING:
            display_running();
            break;
    }
}

/*=== START Main CODE =================================*/
int main(void) {
    gpio_init();
    for(;;) {
        serial_command_task();
        display_task();
    }
    return 0;
}
/*=== END Main CODE ===================================*/