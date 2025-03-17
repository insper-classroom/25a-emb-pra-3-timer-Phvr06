/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"

const int ECHO_PIN = 2;
const int TRIGGER_PIN = 3;
volatile int alarm_fired = 0;
volatile uint64_t start_time = 0;
volatile uint64_t end_time = 0;

void echo_callback(uint gpio, uint32_t events) {
    if (events == GPIO_IRQ_EDGE_RISE) {
        start_time = to_us_since_boot(get_absolute_time());
    } else if (events == GPIO_IRQ_EDGE_FALL) {
        end_time = to_us_since_boot(get_absolute_time());
    }
}

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    alarm_fired = 1;

    // Can return a value here in us to fire in the future
    return 0;
}

int main() {
    stdio_init_all();

    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    gpio_put(TRIGGER_PIN, 0);

    gpio_set_irq_enabled_with_callback(
        ECHO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &echo_callback);

    datetime_t t = {
        .year = 2025,
        .month = 03,
        .day = 15,
        .dotw = 6, // 0 is Sunday, so 3 is Wednesday
        .hour = 19,
        .min = 05,
        .sec = 00};
    rtc_init();
    rtc_set_datetime(&t);

    alarm_id_t alarm;
    int dt;
    float distance;
    int start = 0;
    char c;
    printf("Start? (Y)\n");

    while (1) {
        if (!start) {
            c = getchar();
            printf("%c\n", c);
            if (c == 'Y') {
                start = 1;
            }
        }

        if (start) {
            start_time = 0;
            end_time = 0;
            gpio_put(TRIGGER_PIN, 1);
            sleep_us(10);
            gpio_put(TRIGGER_PIN, 0);

            alarm = add_alarm_in_ms(1000, alarm_callback, NULL, false);

            if (!(alarm)) {
                printf("Failed to add timer\n");
            }

            while (end_time == 0) {
                if (alarm_fired) {
                    alarm_fired = 0;
                    cancel_alarm(alarm);
                    rtc_get_datetime(&t);
                    char datetime_buf[256];
                    char *datetime_str = &datetime_buf[0];
                    datetime_to_str(datetime_str, sizeof(datetime_buf), &t);
                    printf("%s - Falha\n", datetime_str);
                    break;
                } 
            }


            if (end_time != 0) {
                alarm_fired = 0;
                cancel_alarm(alarm);
                dt = end_time - start_time;
                distance = (dt * 0.0343) / 2;

                rtc_get_datetime(&t);
                char datetime_buf[256];
                char *datetime_str = &datetime_buf[0];
                datetime_to_str(datetime_str, sizeof(datetime_buf), &t);

                if (distance <= 300) {
                    printf("%s - %.2f\n", datetime_str, distance);
                } else {
                    printf("%s - Falha\n", datetime_str);
                }
            }

            c = getchar_timeout_us(100);
            if (c == 'N') {
                start = 0;
            }
            sleep_ms(1000);
        }
    }
}