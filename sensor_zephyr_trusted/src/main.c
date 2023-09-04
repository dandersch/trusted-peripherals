#include <zephyr/kernel.h>

/* partition includes */
#include <tfm_ns_interface.h>
#include "trusted_peripheral_ns.h"

/* sensor includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#define I2C_SLAVE_ADDR 0x28

int main(void)
{
    /* sensor code */
    for (;;)
    {
        uint32_t tick_begin = HAL_GetTick(); /* TODO by default the tick increments for every ms.
                                              * is that accurate enough to measure performance ? */

        float sensor_temperature = 0.0f;
        float sensor_humidity    = 0.0f;

        /* i2c code in secure code */
        psa_status_t ret = tp_sensor_data_get(&sensor_temperature, &sensor_humidity);
        if (ret != PSA_SUCCESS) {
            printk("Getting sensor data failed with status: %i\n", ret);
        }

        uint32_t tick_end = HAL_GetTick();
        printk("(%u ms) ", tick_end - tick_begin);

        printk("Temp: %f ", sensor_temperature);
        printk("Humidity: %f\n", sensor_humidity);
    }

    return 0;
}
