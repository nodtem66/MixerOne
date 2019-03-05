#include "main.h"
void gpio_init(void) {
    turn_off_motor();
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(REV_MOTOR_PIN, PWM);
    pinMode(ROT_MOTOR_PIN, PWM);
    Timer3.setPrescaleFactor(36);
    pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
    pinMode(PREV_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(NEXT_BUTTON_PIN, on_push_button, FALLING);
    attachInterrupt(PREV_BUTTON_PIN, on_push_button, FALLING);
    
    UART.begin(9600);
    
    lcd.init();
    lcd.backlight();

    ASSERT( EEPROM.init() == EEPROM_OK );
    load_EEPROM();
}

void status_led_task(void* args) {
    for (;;) {
        digitalWrite(LED_BUILTIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(500));
        digitalWrite(LED_BUILTIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    vTaskDelete( NULL );
}

void test_task(void* args) {
    for (;;) {
        turn_off_motor();
        vTaskDelay(configTICK_RATE_HZ);
        turn_on_motor(65535, 65535);
        vTaskDelay(configTICK_RATE_HZ);
    }
    vTaskDelete( NULL );
}

void serial_command_task(void* args) {
    static byte b;
    static uint8_t success = 0;
    
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
                UART.println(F("OK"));
                if (!session_running) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    test_pwm(&serial_rx);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    //xTaskNotify(displayNotify, BIT_STATE_TEST, eSetBits);
                    UART.println("!!");
                }
            } else if (serial_rx.length() > 0) {
                success = parse_command(&serial_rx, session_list, &session_length);
                if (success == 0)  {
                    UART.println(F("OK"));
                    if (!session_running) {
                        taskENTER_CRITICAL();
                        save_EEPROM();
                        taskEXIT_CRITICAL();
                        //xTaskNotify(displayNotify, BIT_STATE_VIEW, eSetBits);
                    }
                }
                else { UART.print(F("ERROR ")); UART.println(success); }
            }
            serial_rx = "";
            b = '\0';
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete( NULL );
}

static inline void display_home() {
    lcd_display_home();
    BaseType_t status = xTaskNotifyWait(0x00, UINT32_MAX, &notifiedValue, portMAX_DELAY);
    if (status == pdPASS) {
        if ( IS_BIT_SET(notifiedValue, BIT_NEXT_BUTTON) ) {
            taskENTER_CRITICAL();
            load_EEPROM();
            taskEXIT_CRITICAL();
            if (session_length > 0) {
                state = STATE_VIEW;
            } else {
                lcd.setCursor(0,2);
                lcd.print("     Not found");
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        else if ( IS_BIT_SET(notifiedValue, BIT_PREV_BUTTON) ) {
            taskENTER_CRITICAL();
            status = clear_EEPROM();
            taskEXIT_CRITICAL();
            if (status == EEPROM_OK) {
                lcd.setCursor(0,2);
                lcd.print("      Success");
                vTaskDelay(pdMS_TO_TICKS(1000));
            } else {
                lcd.setCursor(0,2);
                lcd.print("       Error");
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        if ( IS_BIT_SET(notifiedValue, BIT_STATE_TEST) ) {
            state = STATE_TEST;
        }
        else if ( IS_BIT_SET(notifiedValue, BIT_STATE_VIEW) ) {
            state = STATE_VIEW;
        }
    }
}

static inline void display_testpwm(void) {
    if (xTaskNotifyWait(0x00, UINT32_MAX, &notifiedValue, portMAX_DELAY) == pdPASS) {
        if ( IS_BIT_SET(notifiedValue, BIT_NEXT_BUTTON) || IS_BIT_SET(notifiedValue, BIT_PREV_BUTTON)) {
            state = STATE_HOME;
        } else if ( IS_BIT_SET(notifiedValue, BIT_STATE_VIEW) ) {
            state = STATE_VIEW;
        }
    }
}

static inline void display_session(void) {
    for(uint8_t i=0; i<session_length;) {
        lcd_display_session(i);
        if (xTaskNotifyWait(0x00, UINT32_MAX, &notifiedValue, portMAX_DELAY) == pdPASS) {
            if ( IS_BIT_SET(notifiedValue, BIT_STATE_VIEW) ) {
                state = STATE_VIEW;
                break;
            } else if ( IS_BIT_SET(notifiedValue, BIT_STATE_TEST) ) {
                state = STATE_TEST;
                break;
            }
            if ( IS_BIT_SET(notifiedValue, BIT_NEXT_BUTTON) ) {
                i++;
                if (i >= session_length) {
                    state = STATE_RUNNING;
                    break;
                }
            } else if ( IS_BIT_SET(notifiedValue, BIT_PREV_BUTTON) ) {
                if (i-1 >= 0) i--;
            }
        }
    }
}

static inline void display_running(void) {
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

                // Wait for PAUSE or RESUME
                if (xTaskNotifyWait(0x00, UINT32_MAX, &notifiedValue, pdMS_TO_TICKS(1000)) == pdPASS) {
                    // WHEN PAUSE
                    if ( IS_BIT_SET(notifiedValue, BIT_NEXT_BUTTON) ) {
                        if (session_running) {
                            session_running = false;
                            lcd.setCursor(13, 3);
                            lcd.print("  End >");
                            turn_off_motor();
                        } else {
                            state = STATE_VIEW;
                            goto finish;
                        }
                    }
                    // WHEN RESUME
                    else if ( IS_BIT_SET(notifiedValue, BIT_PREV_BUTTON) ) {
                        session_running = true;
                        lcd.setCursor(13, 3);
                        lcd.print("Pause >");
                        turn_on_motor(
                            PWM_PERCENT_TO_SPEED(session_list[session_page].motor1_pwm),
                            PWM_PERCENT_TO_SPEED(session_list[session_page].motor2_pwm)
                        );
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

void display_task(void* args) {
    state = STATE_HOME;
    for(;;) {
        switch (state) {
            case STATE_HOME:
                display_home();
                break;
            case STATE_TEST:
                display_testpwm();
                break;
            case STATE_VIEW:
                display_session();
                break;
            case STATE_RUNNING:
                display_running();
                break;
        }
    }
    vTaskDelete( NULL );
}

/*=== START Main CODE =================================*/
int main(void) {
    gpio_init();
    MY_ASSERT(xTaskCreate(status_led_task, "LED", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL) == pdPASS);
    MY_ASSERT(xTaskCreate(test_task, "LED2", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, NULL) == pdPASS);
    MY_ASSERT(xTaskCreate(serial_command_task, "serial", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+3, NULL) == pdPASS);
    MY_ASSERT(xTaskCreate(display_task, "display", configMINIMAL_STACK_SIZE*2, NULL, tskIDLE_PRIORITY+2, &displayNotify) == pdPASS);
    vTaskStartScheduler();
    return 0;
}
/*=== END Main CODE ===================================*/