#include "main.h"

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(REV_MOTOR_PIN, OUTPUT);
  pinMode(ROT_MOTOR_PIN, OUTPUT);
  pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PREV_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(NEXT_BUTTON_PIN, on_push_button, FALLING);
  attachInterrupt(PREV_BUTTON_PIN, on_push_button, FALLING);
  turn_off_motor();
  
  timer4.timer = TIM4;
  TimerHandleInit(&timer4, 57600, 1250); // LED status 1Hz
  attachIntHandle(&timer4, status_led_task);

  UART.begin(9600);
  EEPROM.begin();

  lcd.init();
  lcd.backlight();
}

void loop() {
  serial_command_task();
  display_task();
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
            UART.println("OK");
            if (!session_running) {
                test_pwm(&serial_rx);
                state = STATE_TEST;
                BIT_SET(bit_notify, BIT_NEW_TEST);
                turn_on_motor(rev_pwm, rot_pwm);
            }
        } else if (serial_rx.length() > 0) {
            success = parse_command(&serial_rx, session_list, &session_length);
            if (success == 0)  {
                UART.println("OK");
                if (!session_running) {
                    save_EEPROM();
                    state = STATE_VIEW;
                    BIT_SET(bit_notify, BIT_NEW_VIEW);
                }
            }
            else { UART.print("ERROR "); UART.println(success); }
        }
        serial_rx = "";
    }
}

static inline void display_home(bool is_render) {
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
        BIT_CLEAR(bit_notify, BIT_PREV_BUTTON);
        lcd.setCursor(0,2);
        lcd.print("     Clearing...");
        clear_EEPROM();
        lcd.setCursor(0,2);
        lcd.print("      Success   ");
        delay(1000);
        lcd_display_home();
    }
}

static inline void display_testpwm(bool is_render) {
    bool is_new_test = IS_BIT_SET(bit_notify, BIT_NEW_TEST);
    if (is_render || is_new_test) {
        lcd_display_test(rev_pwm, rot_pwm);
        turn_on_motor(rev_pwm, rot_pwm);
    }
    if ( IS_BIT_SET(bit_notify, BIT_NEXT_BUTTON) || IS_BIT_SET(bit_notify, BIT_PREV_BUTTON)) {
        BIT_CLEAR(bit_notify, BIT_NEXT_BUTTON | BIT_PREV_BUTTON);
        state = STATE_HOME;
    }
    if ( is_new_test ) {
        BIT_CLEAR(bit_notify, BIT_NEW_TEST);
    }
}

static inline void display_session(bool is_first) {
    static uint8_t i=0;
    bool is_render = false;
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
    static uint8_t second = 0;
    static uint32_t t = 0;
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
                for(t=millis(); millis()-t<1000;) {
                    // WHEN PAUSE
                    if ( IS_BIT_SET(bit_notify, BIT_NEXT_BUTTON) ) {
                        BIT_CLEAR(bit_notify, BIT_NEXT_BUTTON);
                        if (session_running) {
                            session_running = false;
                            lcd.setCursor(13, 3);
                            lcd.print("  End >");
                            turn_off_motor();
                        } else {
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
                }
                if (session_running) second--;
            }
            if (minute == 0) break;
        }
    }
finish:
    turn_off_motor();
    session_running = false;
    state = STATE_VIEW;
}

static inline void display_task(void) {
    static uint8_t local_state = STATE_NONE;
    bool is_render = (local_state != state);
    local_state = state;
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