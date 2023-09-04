#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

/* STM32 HAL */
#include <soc.h>
#include <stm32_ll_i2c.h>

static I2C_HandleTypeDef  i2c2_h; // pinctrl-0 = < &i2c2_sda_pf0 &i2c2_scl_pf1 >;
#define I2C_SLAVE_ADDR 0x28 /* TODO why does zephyr use 0x28, but the HAL version expects 0x50 */
static int hal_ready = 0;

int tp_sensor_data_get(float* temp, float* humidity)
{
    if (!hal_ready)
    {
        HAL_Init();

        /* gpio init */
        //gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin   = GPIO_PIN_7; // blue led
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_9; /* red led */
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* i2c init */
        HAL_I2C_MspInit(&i2c2_h);
        i2c2_h.Instance              = I2C2; /* TODO check in stm32l552xx.h if I2C2 is defined as I2C2_S (secure) or I2C2_NS */
        i2c2_h.Init.Timing           = 0x40505681;
        i2c2_h.Init.OwnAddress1      = 0;
        i2c2_h.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
        i2c2_h.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
        i2c2_h.Init.OwnAddress2      = 0;
        i2c2_h.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
        i2c2_h.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
        i2c2_h.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

        if (HAL_I2C_Init(&i2c2_h) != HAL_OK)
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);  // set red led
            printk("i2c init failed\n");
            return -1;
        }

        HAL_StatusTypeDef i2c_fail   = HAL_I2C_IsDeviceReady(&i2c2_h, 0x50, 100, HAL_MAX_DELAY);
        if (i2c_fail)
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); // set red led
            printk("I2C NOT READY, STATUS: %i\n", i2c_fail);
            return -1;
        }

        hal_ready = 1;
    }

    /* measuring request (MR) command to sensor */
    uint8_t msg[1] = {0};
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(&i2c2_h, 0x50, msg, 0, 1000); // Sending in Blocking mode
    if (ret) { printk("Sending MR command at slave address failed!\n"); }
    HAL_Delay(100);

    //gpio_pin_toggle_dt(&led);
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_7); // toggle blue led for every cycle

    /* sensor data fetch (DF) command */
    {
        uint8_t data[4];
        HAL_StatusTypeDef err = HAL_I2C_Master_Receive(&i2c2_h, 0x50, (uint8_t*) &data, 4, 1000);
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
                *temp     = ((float)(raw_value_temp) * 165.0F / 16383.0F) - 40.0F; // 14 bits, -40°C - +125°C
                *humidity = (float)raw_value_humid * 100.0F / 16383.0F; // 14 bits, 0% - 100%

            } else {
                printk("Error converting raw data to normal values.\n");
            }
        }

    }

    /* prepare for trusted delivery */
    {
        /* hash the data */

        /* sign the data */

        /* encrypt the data */
    }

    return 0;
}

int main(void)
{
    for(;;)
    {
        uint32_t tick_begin = HAL_GetTick(); /* TODO by default the tick increments for every ms.
                                              * is that accurate enough to measure performance ? */

        float sensor_temperature = 0.0f;
        float sensor_humidity    = 0.0f;
        int ret = tp_sensor_data_get(&sensor_temperature, &sensor_humidity);
        if (ret) { printk("Getting sensor data failed with status: %i\n", ret);}

        uint32_t tick_end = HAL_GetTick();
        printk("(%u ms) ", tick_end - tick_begin);

        printk("Temp: %f ", sensor_temperature);
        printk("Humidity: %f\n", sensor_humidity);
    }

    return 0;
}
