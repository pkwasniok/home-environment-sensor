#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/pm/device.h>

#include "reading.h"

#define PMS5003_DELAY K_SECONDS(60 * 4 + 30)

static const struct device *pms5003 = DEVICE_DT_GET(DT_ALIAS(pms5003));

void pms5003_handler(struct k_msgq *msgq, void *arg2, void *arg3) {
    int ret;
    struct reading reading;

    while (1) {
        pm_device_action_run(pms5003, PM_DEVICE_ACTION_RESUME);

        k_sleep(K_SECONDS(30));

        ret = sensor_sample_fetch(pms5003);

        if (ret) {
            pm_device_action_run(pms5003, PM_DEVICE_ACTION_SUSPEND);
            k_sleep(PMS5003_DELAY);
            continue;
        }

        pm_device_action_run(pms5003, PM_DEVICE_ACTION_SUSPEND);

        reading.channel = SENSOR_CHAN_PM_1_0_CF;
        sensor_channel_get(pms5003, SENSOR_CHAN_PM_1_0_CF, &(reading.value));
        k_msgq_put(msgq, &reading, K_FOREVER);

        reading.channel = SENSOR_CHAN_PM_2_5_CF;
        sensor_channel_get(pms5003, SENSOR_CHAN_PM_2_5_CF, &(reading.value));
        k_msgq_put(msgq, &reading, K_FOREVER);

        reading.channel = SENSOR_CHAN_PM_10_CF;
        sensor_channel_get(pms5003, SENSOR_CHAN_PM_10_CF, &(reading.value));
        k_msgq_put(msgq, &reading, K_FOREVER);

        k_sleep(PMS5003_DELAY);
    }
}

