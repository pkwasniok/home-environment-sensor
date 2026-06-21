#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

#include "reading.h"

#define BMP280_DELAY K_SECONDS(60)

static const struct device *bmp280 = DEVICE_DT_GET(DT_ALIAS(bmp280));

void bmp280_handler(struct k_msgq *msgq, void *arg2, void *arg3) {
    int ret;
    struct reading reading;

    while (1) {
        ret = sensor_sample_fetch(bmp280);

        if (ret) {
            k_sleep(BMP280_DELAY);
            continue;
        }

        reading.channel = SENSOR_CHAN_AMBIENT_TEMP;
        sensor_channel_get(bmp280, SENSOR_CHAN_AMBIENT_TEMP, &(reading.value));
        k_msgq_put(msgq, &reading, K_FOREVER);

        reading.channel = SENSOR_CHAN_PRESS;
        sensor_channel_get(bmp280, SENSOR_CHAN_PRESS, &(reading.value));
        k_msgq_put(msgq, &reading, K_FOREVER);

        k_sleep(BMP280_DELAY);
    }
}

