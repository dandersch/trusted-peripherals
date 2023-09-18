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

#ifdef EMULATED
    #define GET_TICK() sys_clock_elapsed()
#else
    #define GET_TICK() HAL_GetTick()
#endif

int main(void)
{
    tp_mac_t mac = {0};

    psa_status_t ret = tp_init();
    if (ret != 0) { printk("Initializing TP service failed with status: %i\n", ret); }

    for (;;)
    {
        uint32_t tick_begin = GET_TICK(); /* TODO by default the tick increments for every ms.
                                           * is that accurate enough to measure performance ? */

        float sensor_temperature = 0.0f;
        float sensor_humidity    = 0.0f;

        /* use our trusted peripheral api */
        psa_status_t ret = tp_sensor_data_get(&sensor_temperature, &sensor_humidity, &mac);
        if (ret != 0) { printk("Getting sensor data failed with status: %i\n", ret); }

        uint32_t tick_end = GET_TICK();
        printk("(%u ms) ", tick_end - tick_begin);
        printk("Temp: %f ", sensor_temperature);
        printk("Humidity: %f\n", sensor_humidity);

        printk("HASH: ");
        for (int i = 0; i < MAC_HASH_SIZE; i++)
        {
            printk("%02x", mac.hash[i]);
        }
        printk("\n");

        printk("SIGN: ");
        for (int i = 0; i < MAC_SIGN_SIZE; i++)
        {
            printk("%02x", mac.sign[i]);
        }
        printk("\n");

        #ifdef EMULATED
        k_sleep(K_MSEC(10000000)); // emulated mps2_an521 is too fast
        #endif
    }

    return 0;
}
