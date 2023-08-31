/* mbed Microcontroller Library
 * Copyright (c) 2023 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"

// i2c addresses according to application note
#define HYT_ADDR              0x50   //
#define HYT_SLAVE_ADDRESS     0x28   //
#define HYT_COMMAND_ADDRESS   0x00   // TODO

#define HYT_I2C_SDA           PF_0   // corresponds to I2C2_SDA on ZIO header
#define HYT_I2C_SCL           PF_1   // corresponds to I2C2_SCL on ZIO header

#define SLEEP_TIME            100ms  // time between polling

static float humidity;               // rel. humidity in percentage
static float temperature;            // in celcius

/* From mbed PinNames.h
**
**  PinName     peripheral function
**                                      FUNC_OD         PUPD         AFNUM
** {PF_0,       I2C_2,     STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF4_I2C2)},
** {PF_1,       I2C_2,     STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF4_I2C2)},
*/

int main()
{
    I2C        i2c(HYT_I2C_SDA, HYT_I2C_SCL);
    DigitalOut led1(LED1);
    DigitalOut led2(LED2);
    DigitalIn  button(BUTTON1); // PC_13

    while(1)
    {
        /* HYT sensor polling cycle */
        {
            /*        address   data length */
            i2c.write(HYT_ADDR, 0,   0);         // measuring request (MR) command to sensor
            ThisThread::sleep_for(SLEEP_TIME); // wait for 100ms

            /* sensor data fetch (DF) command */
            {
                char i2c_data[4];
                int  state_bit;
                int  humidity_raw;
                int  temperature_raw;
                int  ret = 0;  // ret is 0 if no errors, -1 if no new values received

                i2c.read(HYT_ADDR, i2c_data, 4);

                state_bit = (i2c_data[0] & 0x40) >> 6;

                if (state_bit == 0) 
                {
                    humidity_raw    = ((i2c_data[0] & 0x3F) << 8) | i2c_data[1];
                    temperature_raw = ((i2c_data[2] << 8) | i2c_data[3]) >> 2;

                    if (temperature_raw < 0x3FFF && humidity_raw < 0x3FFF) {
                        temperature = ((float)(temperature_raw) * 165.0f / 16383.0f) - 40.0f;
                        humidity    = (float) humidity_raw * 100.0f / 16383.0f;
                    } else {
                        ret = -1; // sensor returned wrong data (1111...11)
                    }
                } else { // no new value received from sensor
                    ret = 0; 
                }

                if (ret == 0) {
                    printf("Temp.: %5.3f°C, Humidity: %5.3f\%\n", temperature, humidity);
                } else {
                    printf("Temperature reading failed\n");
                }
            }
        }
    }

    return 0;
}
