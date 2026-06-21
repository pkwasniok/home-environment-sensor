#include <zephyr/kernel.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "reading.h"
#include "state.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

// App

enum app_state app_state = STATE_CONNECT;

// Devices

static const struct led_dt_spec led = LED_DT_SPEC_GET(DT_ALIAS(led));
static const struct device *bmp280 = DEVICE_DT_GET(DT_ALIAS(bmp280));
static const struct device *pms5003 = DEVICE_DT_GET(DT_ALIAS(pms5003));

// Queues

K_MSGQ_DEFINE(msgq_readings, sizeof(struct reading), 16, 1);

// BMP280 thread

void bmp280_handler(void *arg1, void *arg2, void *arg3);
K_THREAD_DEFINE(bmp280_tid, 1024, bmp280_handler, &msgq_readings, NULL, NULL, 5, 0, 5000);

// PMS5003 thread

void pms5003_handler(void *arg1, void *arg2, void *arg3);
K_THREAD_DEFINE(pms5003_tid, 1024, pms5003_handler, &msgq_readings, NULL, NULL, 5, 0, 5000);

// LED thread

// void led_handler(void *arg1, void *arg2, void *arg3);
// K_THREAD_DEFINE(led_tid, 512, led_handler, &app_state, NULL, NULL, 10, 0, 0);

// Main

int main(void) {
    // Initialize devices

    if (!led_is_ready_dt(&led))
        return -1;

    if (!device_is_ready(pms5003))
        return -1;

    if (!device_is_ready(bmp280))
        return -1;

    int ret;
    struct reading reading;

    while (1) {
        ret = k_msgq_get(&msgq_readings, &reading, K_FOREVER);

        if (ret)
            continue;

        switch (reading.channel) {
            case SENSOR_CHAN_AMBIENT_TEMP:
                printk("temp:%d.%d\n", reading.value.val1, reading.value.val2 / 10000);
                break;
            case SENSOR_CHAN_PRESS:
                printk("press:%d%d\n", reading.value.val1, reading.value.val2 / 100000);
                break;
            case SENSOR_CHAN_PM_1_0_CF:
                printk("pm1.0:%d\n", reading.value.val1);
                break;
            case SENSOR_CHAN_PM_2_5_CF:
                printk("pm2.5:%d\n", reading.value.val1);
                break;
            case SENSOR_CHAN_PM_10_CF:
                printk("pm10:%d\n", reading.value.val1);
                break;
            default:
                break;
        }
    }

    return 0;
}

