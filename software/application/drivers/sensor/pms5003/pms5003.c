#define DT_DRV_COMPAT plantower_pms5003

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(pms5003, CONFIG_LOG_DEFAULT_LEVEL);

#define PMS5003_MAGIC_1 0x42
#define PMS5003_MAGIC_2 0x4D

#define PMS5003_CMD_READ  0xE2
#define PMS5003_CMD_MODE  0xE1
#define PMS5003_CMD_POWER 0xE4

#define PMS5003_MODE_PASSIVE 0x00
#define PMS5003_MODE_ACTIVE  0x01

#define PMS5003_POWER_SLEEP 0x00
#define PMS5003_POWER_AWAKE 0x01

#define PMS5003_UART_TIMEOUT K_MSEC(5000)

struct pms5003_data {
    uint16_t pm_1_0_cf;
    uint16_t pm_2_5_cf;
    uint16_t pm_10_cf;
};

struct pms5003_config {
    const struct device *uart;
};

static const struct uart_config pms5003_uart_config = {
    .baudrate = 9600U,
    .data_bits = UART_CFG_DATA_BITS_8,
    .stop_bits = UART_CFG_STOP_BITS_1,
    .parity = UART_CFG_PARITY_NONE,
    .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
};

static void pms5003_write(const struct device *dev, uint8_t *buffer, size_t size) {
    const struct pms5003_config *config = dev->config;

    for (int i = 0; i < size; i++)
        uart_poll_out(config->uart, buffer[i]);
}

static uint16_t pms5003_checksum(uint8_t *buffer, size_t size) {
    uint16_t checksum = 0;

    for (int i = 0; i < size; i++)
        checksum += buffer[i];

    return checksum;
}

static void pms5003_write_command(const struct device *dev, uint8_t cmd, uint8_t data) {
    uint8_t buffer[7] = { PMS5003_MAGIC_1, PMS5003_MAGIC_2, cmd, 0x00, data, 0x00, 0x00 };

    uint16_t checksum = pms5003_checksum(buffer, 5);
    buffer[5] = (checksum >> 8);
    buffer[6] = checksum;

    pms5003_write(dev, buffer, 7);
}

static void pms5003_flush(const struct device *dev) {
    const struct pms5003_config *config = dev->config;

    uint8_t data;
    while (uart_poll_in(config->uart, &data) == 0);
}

static int pms5003_read_frame(const struct device *dev, uint8_t *buffer, size_t size) {
    int ret;
    const struct pms5003_config *config = dev->config;

    if (size < 32)
        return -1;

    k_timeout_t timeout = PMS5003_UART_TIMEOUT;
    int64_t time = k_uptime_ticks();

    uint8_t state = 0;
    uint8_t counter = 0;
    uint8_t data;
    while (counter < 30) {
        ret = uart_poll_in(config->uart, &data);

        // Timeout
        if ((k_uptime_ticks() - time) >= timeout.ticks) {
            return -ETIME;
        }

        // No data
        if (ret == -1) {
            continue;
        }

        // Error
        if (ret != 0) {
            return ret;
        }

        switch (state) {
            case 0:
                if (data == PMS5003_MAGIC_1)
                    state = 1;
                break;
            case 1:
                if (data == PMS5003_MAGIC_2)
                    state = 2;
                else
                    state = 0;
                break;
            case 2:
                if (counter < 30)
                    buffer[2 + (counter++)] = data;
                break;
        }
    }

    buffer[0] = PMS5003_MAGIC_1;
    buffer[1] = PMS5003_MAGIC_2;

    return 0;
}

static int pms5003_sample_fetch(const struct device *dev, enum sensor_channel chan) {
    int ret;
    uint8_t buffer[32];
    struct pms5003_data *data = dev->data;

    pms5003_flush(dev);

    ret = pms5003_read_frame(dev, buffer, 32);

    if (ret == -ETIME) {
        LOG_ERR("Timeout error");
        return -1;
    }

    if (ret) {
        LOG_ERR("Read error");
        return -1;
    }

    uint16_t frame_checksum = ((uint16_t) buffer[30] << 8) + buffer[31];
    uint16_t calc_checksum = pms5003_checksum(buffer, 30);

    if (frame_checksum != calc_checksum) {
        LOG_ERR("Checksum error");
        return -1; 
    }

    data->pm_1_0_cf = (buffer[4] << 8) | buffer[5];
    data->pm_2_5_cf = (buffer[6] << 8) | buffer[7];
    data->pm_10_cf = (buffer[8] << 8) | buffer[9];

    return 0;
}

static int pms5003_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val) {
    struct pms5003_data *data = dev->data;

    switch (chan) {
        case SENSOR_CHAN_PM_1_0_CF:
            val->val1 = data->pm_1_0_cf;
            break;
        case SENSOR_CHAN_PM_2_5_CF:
            val->val1 = data->pm_2_5_cf;
            break;
        case SENSOR_CHAN_PM_10_CF:
            val->val1 = data->pm_10_cf;
            break;
        default:
            return -1;
            break;
    }

    return 0;
}

static DEVICE_API(sensor, pms5003_api) = {
    .sample_fetch = pms5003_sample_fetch,
    .channel_get = pms5003_channel_get,
};

static void pms5003_pm_action_resume(const struct device *dev) {
    LOG_INF("Power mode set to awake");
    pms5003_write_command(dev, PMS5003_CMD_POWER, PMS5003_POWER_AWAKE);
    pms5003_write_command(dev, PMS5003_CMD_MODE, PMS5003_MODE_ACTIVE);
}

static void pms5003_pm_action_suspend(const struct device *dev) {
    LOG_INF("Power mode set to sleep");
    pms5003_write_command(dev, PMS5003_CMD_POWER, PMS5003_POWER_SLEEP);
}

static int pms5003_pm_action(const struct device *dev, enum pm_device_action action) {
    switch (action) {
        case PM_DEVICE_ACTION_TURN_OFF:
        case PM_DEVICE_ACTION_SUSPEND:
            pms5003_pm_action_suspend(dev);
            break;
        case PM_DEVICE_ACTION_TURN_ON:
        case PM_DEVICE_ACTION_RESUME:
            pms5003_pm_action_resume(dev);
            break;
    }

    return 0;
}

static int pms5003_init(const struct device *dev) {
    int ret;
    const struct pms5003_config *config = dev->config;

    if (!device_is_ready(config->uart))
        return -ENODEV;

    ret = uart_configure(config->uart, &pms5003_uart_config);
    if (ret)
        return -ENODEV;

    pms5003_write_command(dev, PMS5003_CMD_POWER, PMS5003_POWER_AWAKE);
    pms5003_write_command(dev, PMS5003_CMD_MODE, PMS5003_MODE_ACTIVE);

    return pm_device_driver_init(dev, pms5003_pm_action);
}

#define PMS5003_DEFINE(i)                                                     \
    static struct pms5003_data pms5003_data_##i;                              \
                                                                              \
    static const struct pms5003_config pms5003_config_##i = {                 \
        .uart = DEVICE_DT_GET(DT_INST_BUS(i))                                 \
    };                                                                        \
                                                                              \
    PM_DEVICE_DT_INST_DEFINE(i, pms5003_pm_action);                           \
                                                                              \
    DEVICE_DT_INST_DEFINE(i, pms5003_init, PM_DEVICE_DT_INST_GET(i), &pms5003_data_##i, &pms5003_config_##i, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &pms5003_api);

DT_INST_FOREACH_STATUS_OKAY(PMS5003_DEFINE)

