#include <zephyr/kernel.h>
#include <zephyr/drivers/led.h>

#include "state.h"

static const struct led_dt_spec led = LED_DT_SPEC_GET(DT_ALIAS(led));

void led_handler(enum app_state *state, void *arg2, void *arg3) {
    while (1) {
        switch (*state) {
            case STATE_CONNECT:
                led_on_dt(&led);
                k_msleep(500);
                led_off_dt(&led);
                k_msleep(500);
                break;
            case STATE_RUN:
                led_on_dt(&led);
                k_msleep(1000);
                break;
        }
    }
}

