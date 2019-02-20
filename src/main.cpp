#include "main.h"

void gpio_init(void) {
    turn_off_motor();
    
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(REV_MOTOR_PIN, PWM);
    pinMode(ROT_MOTOR_PIN, PWM);
    Timer3.setPrescaleFactor(22);
    pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
    pinMode(PREV_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(NEXT_BUTTON_PIN, on_push_button, FALLING);
    attachInterrupt(PREV_BUTTON_PIN, on_push_button, FALLING);
    
    UART.begin(9600);
    
    //lcd.init();
    //lcd.backlight();

    ASSERT( EEPROM.init() == EEPROM_OK );
    load_EEPROM();
}

static void status_led_task(void* args) {
    for (;;) {
        digitalWrite(LED_BUILTIN, LOW);
        vTaskDelay(configTICK_RATE_HZ);
        digitalWrite(LED_BUILTIN, HIGH);
        vTaskDelay(configTICK_RATE_HZ);
    }
}

void display_home_task(void) {
    
    for (;;) {
        state = STATE_HOME;
        lcd_display_home();
        /*
        do {
            //event_wait_multiple(0, home_event, next_button_pressed_event, prev_button_pressed_event);
            //evt = event_last_signaled_get();
        } while (evt != home_event && state != STATE_HOME);
        
        if (evt == next_button_pressed_event) {
            load_EEPROM();
            if (session_length > 0) {
                event_signal(session_display_event);
                event_wait(home_event);
            } else {
                lcd.setCursor(0,2);
                lcd.print("     Not found");
                task_wait(3000);
            }
        } else if (evt == prev_button_pressed_event) {
            if (clear_EEPROM() == EEPROM_OK) {
                lcd.setCursor(0,2);
                lcd.print("      Success");
            } else {
                lcd.setCursor(0,2);
                lcd.print("       Error");
                
            }
            task_wait(3000);
        }*/
    }
}

/*
void serial_command_task(void) {
    static byte b;
    static uint8_t success = 0;
    task_open();
    for (;;) {
        while (UART.available() > 0) {
            b = UART.read();
            if (b == '\n') break;
            serial_rx += (char) b;
        }        
        if (b == '\n') {
            if (serial_rx.startsWith(F("S"))) {
                show_session(session_list, session_length);
            } else if (serial_rx.startsWith(F("HI"))) {
                UART.println(F("HI"));
            } else if (serial_rx.startsWith(F("T"))) {
                if (!session_running) {
                    test_pwm(&serial_rx);
                    event_signal(test_pwm_event);
                }
            } else {
                success = parse_command(&serial_rx, session_list, &session_length);
                if (success == 0)  {
                    UART.println(F("OK"));
                    if (!session_running) {
                        event_signal(session_display_event);
                        save_EEPROM();
                    }
                }
                else { UART.print(F("ERROR ")); UART.println(success); }
            }
            serial_rx = "";
            b = '\0';
        }
        task_wait(100);
    }
    task_close();
}

void test_pwm_task(void) {
    static Evt_t evt;
    task_open();
    event_wait(test_pwm_event);
    for (;;) {
        state = STATE_TEST;
        do {
            event_wait_multiple(0, test_pwm_event, next_button_pressed_event, prev_button_pressed_event);
            evt = event_last_signaled_get();
        } while (evt != test_pwm_event && state != STATE_TEST);

        if ( evt != test_pwm_event ) {
            turn_off_motor();
            event_signal(home_event);
            event_wait(test_pwm_event);
        }
    }
    task_close();
}

void session_display_task(void) {
    static uint8_t i = 0;
    static Evt_t evt = 0;
    task_open();
    event_wait(session_display_event);
    for(;;) {
        state = STATE_VIEW;
        for(i=0; i<session_length;) {
            lcd_display_session(i);
            do {
                event_wait_multiple(0, session_display_event, next_button_pressed_event, prev_button_pressed_event);
                evt = event_last_signaled_get();
            } while (evt != session_display_event && state != STATE_VIEW);

            if (evt == session_display_event) break;
            else if (evt == next_button_pressed_event) {
                i++;
                if (i >= session_length) {
                    event_signal(session_running_event);
                    event_wait(session_display_event);
                    break;
                }
            } else if (evt == prev_button_pressed_event) {
                if (i-1 >= 0) i--;
            }
        }
    }
    task_close();
}

void session_running_task(void) {
    static uint16_t minute = 0;
    static int8_t second = 0;
    for (;;) {
        session_running = true;
        state = STATE_RUNNING;
        for (session_page = 0;session_page < session_length; session_page++) {
            lcd_display_running(session_page);
            turn_on_motor(
                PWM_PERCENT_TO_SPEED(session_list[session_page].motor1_pwm),
                PWM_PERCENT_TO_SPEED(session_list[session_page].motor2_pwm)
            );
            minute = session_list[session_page].duration_minute - 1;
            for (; minute >=0; minute--) {
                for (second = 60; second >= 0;) {
                    show_timer(minute, second);
                    // WHEN PAUSE
                    if (event_last_signaled_get() == next_button_pressed_event) {
                        session_running = false;
                        lcd.setCursor(13, 3);
                        lcd.print("  End >");
                        turn_off_motor();
                    }
                    if (session_running) second--;
                    else {
                        event_wait_multiple(0, prev_button_pressed_event, next_button_pressed_event);
                        if (event_last_signaled_get() == next_button_pressed_event) {
                            goto finish;
                        } else {
                            // WHEN RESUME
                            session_running = true;
                            lcd.setCursor(13, 3);
                            lcd.print("Pause >");
                            turn_on_motor(
                                PWM_PERCENT_TO_SPEED(session_list[session_page].motor1_pwm),
                                PWM_PERCENT_TO_SPEED(session_list[session_page].motor2_pwm)
                            );
                        }
                    }
                }
                if (minute == 0) break;
            }
        }
finish:
        session_running = false;
    }
}*/

/*=== START Main CODE =================================*/
int main(void) {
    gpio_init();
    portBASE_TYPE s1;
    s1 = xTaskCreate(status_led_task, "LED", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    MY_ASSERT(s1 == pdPASS);
    vTaskStartScheduler();
    return 0;
}
/*=== END Main CODE ===================================*/