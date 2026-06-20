#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/pm/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

// Devices

static const struct led_dt_spec led = LED_DT_SPEC_GET(DT_ALIAS(led));
static const struct device *pms5003 = DEVICE_DT_GET(DT_ALIAS(pms5003));
static const struct device *bmp280 = DEVICE_DT_GET(DT_ALIAS(bmp280));

// Queue

struct reading {
    struct sensor_value value;
    enum sensor_channel channel;
};

K_FIFO_DEFINE(reading_fifo);

// BMP280 thread

void bmp280_handler(void *arg1, void *arg2, void *arg3);
K_THREAD_DEFINE(bmp280_tid, 1024, bmp280_handler, NULL, NULL, NULL, 5, 0, 5000);

void bmp280_handler(void *arg1, void *arg2, void *arg3) {
    int ret;
    struct reading reading;

    while (1) {
        ret = sensor_sample_fetch(bmp280);

        reading.channel = SENSOR_CHAN_AMBIENT_TEMP;
        ret = sensor_channel_get(bmp280, reading.channel, &(reading.value));

        k_fifo_put(&reading_fifo, &reading);

        reading.channel = SENSOR_CHAN_PRESS;
        ret = sensor_channel_get(bmp280, reading.channel, &(reading.value));

        k_fifo_put(&reading_fifo, &reading);

        k_sleep(K_SECONDS(5));
    }
}

// PMS5003 thread

void pms5003_handler(void *arg1, void *arg2, void *arg3);
K_THREAD_DEFINE(pms5003_tid, 1024, pms5003_handler, NULL, NULL, NULL, 5, 0, 5000);

void pms5003_handler(void *arg1, void *arg2, void *arg3) {
    int ret;
    struct reading reading;

    while (1) {
        pm_device_action_run(pms5003, PM_DEVICE_ACTION_RESUME);

        k_sleep(K_SECONDS(30));

        ret = sensor_sample_fetch(pms5003);

        reading.channel = SENSOR_CHAN_PM_1_0_CF;
        ret = sensor_channel_get(pms5003, reading.channel, &(reading.value));

        k_fifo_put(&reading_fifo, &reading);

        reading.channel = SENSOR_CHAN_PM_2_5_CF;
        ret = sensor_channel_get(pms5003, reading.channel, &(reading.value));

        k_fifo_put(&reading_fifo, &reading);

        reading.channel = SENSOR_CHAN_PM_10_CF;
        ret = sensor_channel_get(pms5003, reading.channel, &(reading.value));

        k_fifo_put(&reading_fifo, &reading);

        pm_device_action_run(pms5003, PM_DEVICE_ACTION_SUSPEND);

        k_sleep(K_SECONDS(30));
    }
}

// Main

int main(void) {
    if (!led_is_ready_dt(&led))
        return -1;

    if (!device_is_ready(pms5003))
        return -1;

    if (!device_is_ready(bmp280))
        return -1;

    pm_device_action_run(pms5003, PM_DEVICE_ACTION_RESUME);

    while (1) {
        struct reading *reading;
        reading = k_fifo_get(&reading_fifo, K_FOREVER);

        switch (reading->channel) {
            case SENSOR_CHAN_AMBIENT_TEMP:
                LOG_INF("Temperature: %d %d", reading->value.val1, reading->value.val2);
                break;
            case SENSOR_CHAN_PRESS:
                LOG_INF("Pressure: %d %d", reading->value.val1, reading->value.val2);
                break;
            case SENSOR_CHAN_PM_1_0_CF:
                LOG_INF("PM1.0: %d %d", reading->value.val1, reading->value.val2);
                break;
            case SENSOR_CHAN_PM_2_5_CF:
                LOG_INF("PM2.5: %d %d", reading->value.val1, reading->value.val2);
                break;
            case SENSOR_CHAN_PM_10_CF:
                LOG_INF("PM10: %d %d", reading->value.val1, reading->value.val2);
                break;
            default:
                break;
        }
    }

    return 0;
}

