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

/* TODO */
// TODO return proper psa_status
#define I2C_SLAVE_ADDR 0x28
static psa_status_t tfm_tp_hal_init()
{
    HAL_Init();
    //uint32_t tick = HAL_GetTick();

    /* i2c init */
    I2C_HandleTypeDef  i2c2_h; // pinctrl-0 = < &i2c2_sda_pf0 &i2c2_scl_pf1 >;
    HAL_I2C_MspInit(&i2c2_h);
    /* (#)Initialize the I2C low level resources by implementing the HAL_I2C_MspInit() API:
        (##) Enable the I2Cx interface clock
        (##) I2C pins configuration
            (+++) Enable the clock for the I2C GPIOs
            (+++) Configure I2C pins as alternate function open-drain
    */

    /* (#) Configure the Communication Clock Timing, Own Address1, Master
     * Addressing mode, Dual Addressing mode, Own Address2, Own Address2 Mask,
     * General call and Nostretch mode in the hi2c Init structure. */

    i2c2_h.Instance              = I2C2; /* TODO check in stm32l552xx.h if I2C2 is defined as I2C2_S (secure) or I2C2_NS */
    //i2c2_h.Instance              = I2C2_BASE_NS;
    //i2c2_h.Instance              = I2C2_BASE_S;
    //i2c2_h.ClockSpeed            = 100000;
    i2c2_h.Init.Timing           = 100000; // TODO get right timing
    //i2c2_h.Init.DutyCycle        = I2C_DUTYCYCLE_2;
    i2c2_h.Init.OwnAddress1      = 0;
    i2c2_h.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    i2c2_h.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    i2c2_h.Init.OwnAddress2      = 0;
    i2c2_h.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    i2c2_h.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

    /* (#) Initialize the I2C registers by calling the HAL_I2C_Init(),
     * configures also the low level Hardware (GPIO, CLOCK, NVIC...etc) by calling
     * the customized HAL_I2C_MspInit(&hi2c) API. */
    if (HAL_I2C_Init(&i2c2_h) != HAL_OK) { printf("i2c init failed\n"); }

    /* (#) To check if target device is ready for communication, use the function HAL_I2C_IsDeviceReady() */
    HAL_StatusTypeDef i2c_ready = HAL_I2C_IsDeviceReady(&i2c2_h, 0x50, 5, 1000);
    printf("I2C STATUS: %i\n", i2c_ready);

    /* send measuring request */
    uint8_t msg[1] = {0};
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(&i2c2_h, 0x50, msg, 0, 1000); // Sending in Blocking mode

    //HAL_GPIO_TogglePin (GPIOB, GPIO_PIN_7); /* toggle blue led */
    HAL_Delay(100);

    return PSA_SUCCESS;
}

static psa_status_t tfm_tp_hal_init_ipc(psa_msg_t *msg)
{
#if 0
    /* CHECK INPUT */
    uint32_t first_argument;
    /* The size of the first argument is incorrect */
    if (msg->in_size[0] != sizeof(first_argument)) { return PSA_ERROR_PROGRAMMER_ERROR; }

    /* get & set value of first argument */
    size_t ret = 0;
    ret = psa_read(msg->handle, 0, &first_argument, msg->in_size[0]); /* should return number of bytes copied */
    if (ret != msg->in_size[0]) { return PSA_ERROR_PROGRAMMER_ERROR; }
#endif

    return tfm_tp_hal_init();
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

        if (signals & TFM_TP_HAL_INIT_SIGNAL) {
            tp_signal_handle(TFM_TP_HAL_INIT_SIGNAL, tfm_tp_hal_init_ipc);
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
