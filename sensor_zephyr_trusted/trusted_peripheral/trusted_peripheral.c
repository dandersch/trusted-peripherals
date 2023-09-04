#include <psa/crypto.h>
#include <stdint.h>
#include "tfm_api.h"

#include "psa/service.h"
#include "psa_manifest/tfm_trusted_peripheral.h"

/* NOTE ours */
#include <stdio.h> // TODO does this make tf-m include libc?
#define HAL_I2C_MODULE_ENABLED // TODO only worked after adding this at the start of  "stm32l5xx_hal_i2c.c"
#include "stm32hal.h"
#include "stm32l5xx_hal_i2c.h" /* NOTE we pasted the files for I2C module ourselves
                                * into the tf-m folders and added them to the CMakeLists.txt.
                                * TF-M has code that is supposed to include the
                                * i2c files if HAL_I2C_MODULE_ENABLED is defined,
                                * but the files aren't in the hal folder */

#define I2C_SLAVE_ADDR 0x28
static int hal_ready = 0;
static I2C_HandleTypeDef i2c2_h; // pinctrl-0 = < &i2c2_sda_pf0 &i2c2_scl_pf1 >;
static psa_status_t tfm_tp_sensor_data_get(void* handle)
{
    float temp;
    float humidity;

    if (!hal_ready)
    {
        HAL_Init();

        /* gpio init */
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin   = GPIO_PIN_7; /* blue led */
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_9; /* red led */
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* i2c init */
        HAL_I2C_MspInit(&i2c2_h);
        i2c2_h.Instance              = I2C2; /* TODO check in stm32l552xx.h if I2C2 is I2C2_S (secure) or I2C2_NS */
        i2c2_h.Init.Timing           = 0x40505681;
        i2c2_h.Init.OwnAddress1      = 0;
        i2c2_h.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
        i2c2_h.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
        i2c2_h.Init.OwnAddress2      = 0;
        i2c2_h.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
        i2c2_h.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

        if (HAL_I2C_Init(&i2c2_h) != HAL_OK)
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET); // set red led
            printf("i2c init failed\n"); /* NOTE no working printf in secure code, so we set the led too */
            return PSA_ERROR_GENERIC_ERROR;
        }

        HAL_StatusTypeDef i2c_fail   = HAL_I2C_IsDeviceReady(&i2c2_h, 0x50, 100, HAL_MAX_DELAY);
        if (i2c_fail)
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET); // set red led
            printf("I2C NOT READY, STATUS: %i\n", i2c_fail);
            return PSA_ERROR_GENERIC_ERROR;
        }

        hal_ready = 1;
    }

    /* send measuring request */
    uint8_t msg[1] = {0};
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(&i2c2_h, 0x50, msg, 0, 1000); // Sending in Blocking mode
    if (ret) { printf("Sending MR command at slave address failed!\n"); }

    HAL_Delay(100);

    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_7); // toggle blue led for every cycle

    /* sensor data fetch (DF) command */
    {
        uint8_t data[4];
        HAL_StatusTypeDef err = HAL_I2C_Master_Receive(&i2c2_h, 0x50, (uint8_t*) &data, 4, 1000);
        if (err) { printf("Reading from slave address failed!\n"); }

        // 2 most significant bits of the first byte contain status information, the
        // bit at 0x40 being the stalebit (if set, there is no new data yet)
        uint8_t stale_bit = (data[0] & 0x40) >> 6;
        if (stale_bit) {
            printf("Data is not ready yet\n");
        } else {
            // 14 bits raw temperature in byte 3 & 4, read from left to right
            uint32_t raw_value_temp = ((data[2] << 8) | data[3]) >> 2;

            // 14 bits raw humidity in byte 1 & 2, read right from left and delete status bits from first byte
            uint32_t raw_value_humid = ((data[0] & 0x3F) << 8) | data[1];

            if (raw_value_temp < 0x3FFF && raw_value_humid < 0x3FFF) {
                temp     = ((float)(raw_value_temp) * 165.0F / 16383.0F) - 40.0F; // 14 bits, -40°C - +125°C
                humidity = (float)raw_value_humid * 100.0F / 16383.0F; // 14 bits, 0% - 100%

            } else {
                printf("Error converting raw data to normal values.\n");
            }
        }
    }

    psa_write((psa_handle_t)handle, 0, &temp, sizeof(temp));
    psa_write((psa_handle_t)handle, 1, &humidity, sizeof(humidity));

    /* Test psa_crypto functions */
    // psa_sign_message(key, alg, input, ...)` is equivalent to
    // psa_verify_message()
    //tfm_crypto_hash_interface(psa_invec in_vec[], psa_outvec out_vec[]);
    // uint8_t* mac
    // status = crp_hash_payload(msg, strlen(msg), hash, sizeof(hash), &hash_len);
    //
    // /* Sign the hash using key #1. */
    // status = crp_sign_hash(1, hash, hash_len, sig, sizeof(sig), &sig_len);
    //
    // /* Verify the hash signature using the public key. */
    // status = crp_verify_sign(1, hash, hash_len, sig, sig_len);

    //psa_invec  hash_input  = {.base = NULL, .len = 0};
    //psa_outvec hash_output = {.base = NULL, .len = 0};
    //tfm_crypto_hash_interface(hash_input, hash_output);

    return PSA_SUCCESS;
}

static psa_status_t tfm_tp_sensor_data_get_ipc(psa_msg_t *msg)
{
    /* CHECK INPUT */
    // NOTE our function only has output parameters right now
#if 0
    float* first_argument;
    /* The size of the first argument is incorrect */
    if (msg->out_size[0] != sizeof(first_argument)) { return PSA_ERROR_PROGRAMMER_ERROR; }

    /* get & set value of first argument */
    size_t ret = 0;
    ret = psa_read(msg->handle, 0, &first_argument, msg->out_size[0]); /* should return number of bytes copied */
    if (ret != msg->in_size[0]) { return PSA_ERROR_PROGRAMMER_ERROR; }
#endif

    return tfm_tp_sensor_data_get((void*) msg->handle);
}

typedef psa_status_t (*tp_func_t)(psa_msg_t *);
static void tp_signal_handle(psa_signal_t signal, tp_func_t pfn)
{
    psa_status_t status;
    psa_msg_t msg;

    status = psa_get(signal, &msg);
    switch (msg.type) {
    case PSA_IPC_CONNECT:
        psa_reply(msg.handle, PSA_SUCCESS);
        break;
    case PSA_IPC_CALL:
        status = pfn(&msg);
        psa_reply(msg.handle, status);
        break;
    case PSA_IPC_DISCONNECT:
        psa_reply(msg.handle, PSA_SUCCESS);
        break;
    default:
        psa_panic();
    }
}

psa_status_t tfm_tp_req_mngr_init(void)
{
    psa_signal_t signals = 0;

    while (1) {
        signals = psa_wait(PSA_WAIT_ANY, PSA_BLOCK);

        if (signals & TFM_TP_SENSOR_DATA_GET_SIGNAL) {
            tp_signal_handle(TFM_TP_SENSOR_DATA_GET_SIGNAL, tfm_tp_sensor_data_get_ipc);
        }

        /*
        else if (signals & TFM_TP_SERVICE_TWO_SIGNAL) {
            tp_signal_handle(TFM_TP_SERVICE_TWO_SIGNAL, tfm_tp_service_two_ipc);
        }
        */

        else {
            psa_panic();
        }
    }

    return PSA_ERROR_SERVICE_FAILURE;
}
