#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include <psa/crypto.h>

#ifdef UNTRUSTED
#else
  #include <tfm_ns_interface.h>
#endif

#include "trusted_peripheral.h"

int main(void)
{
    tp_mac_t mac = {0};

    psa_status_t ret = tp_init();
    if (ret != 0) { printk("Initializing TP service failed with status: %i\n", ret); }

    /* TODO setup three keys using psa crypto for testing
     * 1. rsa private key
     * 2. rsa public key
     * 3. aes key
     */

    for (;;)
    {
        uint32_t tick_begin = HAL_GetTick(); /* TODO by default the tick increments for every ms.
                                              * is that accurate enough to measure performance ? */

        float sensor_temperature = 0.0f;
        float sensor_humidity    = 0.0f;

        /* use our trusted peripheral api */
        psa_status_t ret = tp_sensor_data_get(&sensor_temperature, &sensor_humidity, &mac);
        if (ret != 0) { printk("Getting sensor data failed with status: %i\n", ret); }

        uint32_t tick_end = HAL_GetTick();
        printk("(%u ms) ", tick_end - tick_begin);
        printk("Temp: %f ", sensor_temperature);
        printk("Humidity: %f\n", sensor_humidity);

        printk("HASH: ");
        for (int i = 0; i < 32; i++)
        {
            printk("%02x", mac.hash[i]);
        }
        printk("\n");

        printk("SIGN: ");
        for (int i = 0; i < 256; i++)
        {
            printk("%02x", mac.sign[i]);
        }
        printk("\n");
    }

    return 0;
}
