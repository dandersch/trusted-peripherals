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

#include <zephyr/timing/timing.h> /* for profiling */

// TODO uart transmission

int main(void)
{
    timing_init();

    tp_mac_t mac = {0};
    uint8_t ciphertext[ENCRYPTED_SENSOR_DATA_SIZE] = {0};
    transform_t transforms[TP_MAX_TRANSFORMS] = {0};

    trusted_transform_t tt = {0};

    psa_status_t ret = tp_init();
    if (ret != 0) { printk("Initializing TP service failed with status: %i\n", ret); }

    for (;;)
    {
        /* TODO try timing_ */
        timing_start();
        uint32_t tick_begin = GET_TICK(); /* NOTE tick increments for every ms. */
        timing_t cycle_begin = timing_counter_get();

        /* use our trusted peripheral api */
        sensor_data_t sensor_data = {0};
        psa_status_t ret = tp_trusted_capture(&sensor_data, &mac);
        if (ret != 0) { printk("Trusted Capture failed with status: %i\n", ret); }

        ret = tp_trusted_delivery(ciphertext, &mac);
        if (ret != 0) { printk("Trusted Delivery failed with status: %i\n", ret); }

        transform_t transform = {0};
        ret = tp_trusted_transform(&tt, transform);
        if (ret != 0) { printk("Trusted Transform init failed with status: %i\n", ret); }

        /* perform example transformation */
        transform.type = TRANSFORM_ID_CONVERT_CELCIUS_TO_FAHRENHEIT;
        transform.convert_params[0] = 1.8f;
        transform.convert_params[1] = 32.f;
        tp_trusted_transform(&tt, transform);
        transform.type = TRANSFORM_ID_CONVERT_FAHRENHEIT_TO_CELCIUS;
        tp_trusted_transform(&tt, transform);

        uint32_t tick_end  = GET_TICK();
        // TODO there is also timing_cycles_to_ns_avg()
        timing_t cycle_end = timing_counter_get();
        uint64_t cycles    = timing_cycles_get(&cycle_begin, &cycle_end);
        uint64_t nanosecs  = timing_cycles_to_ns(cycles);
        timing_stop();

        printk("(%" PRIu64 " cycles) ", cycles);
        printk("(%" PRIu64 " ns) ", nanosecs);
        printk("(%u ms) ", tick_end - tick_begin);
        printk("Temp: %f ",  tt.data.temp);
        printk("Humidity: %f\n", tt.data.humidity);


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
        //k_sleep(K_MSEC(10000000)); // emulated mps2_an521 is too fast
        #endif

    }

    return 0;
}
