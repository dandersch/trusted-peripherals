#include <zephyr/kernel.h>

/* partition includes */
#include <tfm_ns_interface.h>
#include "trusted_peripheral_ns.h"

/* sensor includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#define LED0_NODE DT_ALIAS(led0) /* The devicetree node identifier for the "led0" alias. */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define I2C_SLAVE_ADDR 0x28

int main(void)
{
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE); // enable blue led (?)

#if 0
    /* Testing our partition code */
    /* TODO this is not calling secure code right now */
    tp_check_memory();
    tp_fill_struct_t   test = { 42, 6.9f, TP_TYPE_SPECIAL, NULL };
    tp_return_struct_t ret  = tp_our_function(100, &test);
    printk("%c %c %c %c %c\n", ret.name[0], ret.name[1], ret.name[2], ret.name[3], ret.name[4]);
    printk("their value: %i\n", *((int*) test.ptr));
    int* their_value = (int*) test.ptr;
    *their_value = 4; // can we change their memory?
    tp_check_memory();
#endif

    /* sensor code */
    const struct device *i2c_dev_2 = DEVICE_DT_GET(DT_NODELABEL(i2c2));
    if (i2c_dev_2 == NULL || !device_is_ready(i2c_dev_2))
    {
        printk("Could not get I2C device\n");
        return -1;
    }

    for (int i = 0; i < 1; i++)
    {
        /* send the MR in secure code */
        psa_status_t init_status = tp_hal_init();
        if (init_status != PSA_SUCCESS) {
            printk("HAL_INIT PSA Status: %d\n", init_status);
        }

        /* measuring request (MR) command to sensor */
        //uint8_t msg[1] = {0};
        //int ret  = i2c_write(i2c_dev_2, msg,  0, I2C_SLAVE_ADDR);
        //if (ret) { printk("Sending MR command at slave address failed!\n"); }
        //k_sleep(K_MSEC(100)); // 100ms wait dictated by the sensor
        //gpio_pin_toggle_dt(&led); // toggle blue led for every cycle

        /* sensor data fetch (DF) command */
        if (1) {
            uint8_t data[4];
            int err = i2c_read(i2c_dev_2, &data[0], 4, I2C_SLAVE_ADDR);
            if (err) { printk("Reading from slave address failed!\n"); }

            // 2 most significant bits of the first byte contain status information, the
            // bit at 0x40 being the stalebit (if set, there is no new data yet)
            uint8_t stale_bit = (data[0] & 0x40) >> 6;
            if (stale_bit) {
                printk("Data is not ready yet\n");
            } else {
                // 14 bits raw temperature in byte 3 & 4, read from left to right
                uint32_t raw_value_temp = ((data[2] << 8) | data[3]) >> 2;

                // 14 bits raw humidity in byte 1 & 2, read right from left and delete status bits from first byte
                uint32_t raw_value_humid = ((data[0] & 0x3F) << 8) | data[1];

                if (raw_value_temp < 0x3FFF && raw_value_humid < 0x3FFF) {
                    float temperature = ((float)(raw_value_temp) * 165.0F / 16383.0F) - 40.0F; // 14 bits, -40°C - +125°C
                    float humidity = (float)raw_value_humid * 100.0F / 16383.0F; // 14 bits, 0% - 100%

                    printk("Temp: %f ", temperature);
                    printk("Humidity: %f\n", humidity);
                } else {
                    printk("Error converting raw data to normal values.\n");
                }
            }

        }

    }

    return 0;
}
