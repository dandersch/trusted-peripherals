#include <zephyr/kernel.h>

/* partition includes */
#include <tfm_ns_interface.h>
#include "trusted_peripheral_ns.h"

/* sensor includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#define I2C_SLAVE_ADDR 0x28


/* TODO duplicated on secure side */
#define MAC_HASH_SIZE 256
typedef struct {
    uint8_t buf[MAC_HASH_SIZE];  /* lets assume our data packet fits into this buffer */
} tp_mac_t;

int main(void)
{
    tp_mac_t mac = {0};

    /* sensor code */
    for (;;)
    {
        uint32_t tick_begin = HAL_GetTick(); /* TODO by default the tick increments for every ms.
                                              * is that accurate enough to measure performance ? */

        float sensor_temperature = 0.0f;
        float sensor_humidity    = 0.0f;

        /* i2c code in secure code */
        psa_status_t ret = tp_sensor_data_get(&sensor_temperature, &sensor_humidity, &mac, sizeof(tp_mac_t));
        if (ret != PSA_SUCCESS) {
            printk("Getting sensor data failed with status: %i\n", ret);
        }

        uint32_t tick_end = HAL_GetTick();
        printk("(%u ms) ", tick_end - tick_begin);

        printk("Temp: %f ", sensor_temperature);
        printk("Humidity: %f\n", sensor_humidity);

        /*
        printk("MAC: ");
        for (int i = 0; i < 32; i++)
        {
            printk("%02x", mac.buf[i]);
        }
        printk("\n");
        */
    }

    return 0;
}
