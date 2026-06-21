#ifndef READING_H
#define READING_H

#include <zephyr/drivers/sensor.h>

struct reading {
    enum sensor_channel channel;
    struct sensor_value value;
};

#endif // READING_H
