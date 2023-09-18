#include <stdint.h>

#include <psa/crypto.h>
#include "../keys.h"

#ifdef UNTRUSTED
    #include "trusted_peripheral.h"

    /* STM32 HAL */
    #include <soc.h>
    #if !defined(EMULATED)
    #include <stm32_ll_i2c.h>
    #endif
#else
    #include "../src/trusted_peripheral.h"
    #include "psa/service.h"
    #include "psa_manifest/tfm_trusted_peripheral.h"
    #include "tfm_api.h"

    /* NOTE ours */
    #include <stdio.h> // TODO does this make tf-m include libc?
    #if !defined(EMULATED)
    #include "stm32hal.h"
    #include "stm32l5xx_hal_i2c.h" /* NOTE we pasted the files for I2C module ourselves
                                    * into the tf-m folders and added them to the
                                    * CMakeLists.txt.  TF-M comes with the stm32 hal
                                    * library, but not i2c or spi related stuff */
    #include "stm32l5xx_hal_spi.h" /* same as above */
    #endif
#endif


/* PINS in USE:
**
** PA9 - red led
** PB7 - blue led
**
** PF0 - I2C2_SDA
** PF1 - I2C2_SCL
**
** PG0 - OLED_RST
** PG1 - OLED_DC
** PA4 - SPI1_CS
** PB3 - SPI1_SCK
** PB5 - SPI1_MOSI
*/

static int TP_USE_DISPLAY = 0;

#if !defined(EMULATED)
/* I2C peripheral */
#define I2C_SLAVE_ADDR 0x28 /* TODO why does zephyr use 0x28, but the HAL version expects 0x50 */
static I2C_HandleTypeDef i2c2_h; // pinctrl-0 = < &i2c2_sda_pf0 &i2c2_scl_pf1 >;

/* SPI display */
SPI_HandleTypeDef hspi1;
uint8_t black_image[1024]; // for oled test
uint8_t our_image[1024];
#include "OLED_0in96.h"
#include "main.h" // NOTE if we change SPI pins we have to change this file
#include "test.h"
#endif

static psa_status_t crp_imp_key_secp256r1(psa_key_id_t key_id, psa_key_usage_t key_usage, uint8_t *key_data);

/* keys */
static psa_key_id_t key_rsa_sign    = 1;
//static psa_key_id_t key_rsa_verify  = 2; // stored on server
static psa_key_id_t key_rsa_crypt   = 3;
static psa_key_id_t key_rsa_decrypt = 4;   // stored on server
static psa_algorithm_t alg_sign    = PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256);
static psa_algorithm_t alg_encrypt = PSA_ALG_RSA_PKCS1V15_CRYPT;

#ifdef UNTRUSTED
psa_status_t tp_init()
#else
static psa_status_t tfm_tp_init()
#endif
{
    /* setup hal & peripherals */
    {
    #if !defined(EMULATED)
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

        /* spi init */
        hspi1.Instance               = SPI1;
        hspi1.Init.Mode              = SPI_MODE_MASTER;
        hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
        hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
        hspi1.Init.CLKPolarity       = SPI_POLARITY_HIGH;
        hspi1.Init.CLKPhase          = SPI_PHASE_2EDGE;
        hspi1.Init.NSS               = SPI_NSS_SOFT;
        hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
        hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
        hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
        hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
        hspi1.Init.CRCPolynomial     = 10;
        //hspi1.Init.CRCLength         = SPI_CRC_LENGTH_DATASIZE;
        //hspi1.Init.NSSPMode          = SPI_NSS_PULSE_ENABLE;
        if (HAL_SPI_Init(&hspi1) != HAL_OK)
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); // set red led
            printf("spi init failed\n");
            return -1;
        }
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();

        GPIO_InitTypeDef GPIO_SPI_InitStruct = {0};
        GPIO_SPI_InitStruct.Pin       = GPIO_PIN_3 | // PB3 -> SPI1_SCK
                                        GPIO_PIN_5;  // PB5 -> SPI1_MOSI
        GPIO_SPI_InitStruct.Mode      = GPIO_MODE_AF_PP; // alternate function push pull
        GPIO_SPI_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_SPI_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOB, &GPIO_SPI_InitStruct);

        HAL_GPIO_WritePin(GPIOG, OLED_DC_Pin|OLED_RST_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_RESET);

        /* set dc rst pin */
        GPIO_InitStruct.Pin   = OLED_DC_Pin|OLED_RST_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

        /* set spi_cs pin */
        GPIO_InitStruct.Pin       = OLED_CS_Pin;
        GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(OLED_CS_GPIO_Port, &GPIO_InitStruct);

        HAL_SPI_StateTypeDef spi_state = HAL_SPI_GetState(&hspi1);
        uint32_t spi_error             = HAL_SPI_GetError(&hspi1); // HAL_SPI_ERROR_NONE
        if (spi_state != HAL_SPI_STATE_READY || spi_error != HAL_SPI_ERROR_NONE)
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); // set red led
            printf("SPI NOT READY, STATUS: %i, ERROR: %i\n", spi_state, spi_error);
            return spi_error;
        }

        /* LED code */
        {
            //OLED_0in96_test(); // demo with a while(1) loop

            OLED_0in96_Init();
            Driver_Delay_ms(500);

            Paint_NewImage(our_image, OLED_0in96_WIDTH, OLED_0in96_HEIGHT, 90, BLACK);
            Paint_SelectImage(our_image);
            Driver_Delay_ms(500);
            Paint_Clear(BLACK);
        }

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
            return -132; // PSA_ERROR_GENERIC_ERROR;
        }

        HAL_StatusTypeDef i2c_fail   = HAL_I2C_IsDeviceReady(&i2c2_h, 0x50, 100, HAL_MAX_DELAY);
        if (i2c_fail)
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET); // set red led
            printf("I2C NOT READY, STATUS: %i\n", i2c_fail);
            return -132; // PSA_ERROR_GENERIC_ERROR;
        }
    #endif
    }

    /* setup keys here */
    {
        psa_status_t status;
        status = psa_crypto_init();
        if (status != PSA_SUCCESS)
        {
            printf("Failed to init crypto service\n");
            return status;
        }

        /* import signing key (rsa private key) */
        psa_key_attributes_t attributes    = PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_VERIFY_HASH);
        psa_set_key_algorithm(&attributes,   alg_sign);
        psa_set_key_type(&attributes,        PSA_KEY_TYPE_RSA_KEY_PAIR);
        psa_set_key_bits(&attributes,        1024);
        //psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_PERSISTENT);

        status = psa_import_key(&attributes, key_data_rsa, sizeof(key_data_rsa), &key_rsa_sign);
        if (status != PSA_SUCCESS) {
            printf("Failed to import signing key: %d\n", status);
            return status;
        }

        /* import encryption key (rsa private key) */
        //attributes                         = PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
        psa_set_key_algorithm(&attributes,   alg_encrypt);
        psa_set_key_type(&attributes,        PSA_KEY_TYPE_RSA_KEY_PAIR);
        psa_set_key_bits(&attributes,        1024);
        //psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_PERSISTENT);
        status = psa_import_key(&attributes, key_data_rsa_crypt, sizeof(key_data_rsa_crypt), &key_rsa_crypt);
        if (status != PSA_SUCCESS) {
            printf("Failed to import private key for decryption: %d\n", status);
            return status;
        }

        /* get rsa public key from private key above */
        size_t key_len;
        psa_export_public_key(key_rsa_crypt, rsa_pub_key_buf, sizeof(rsa_pub_key_buf), &key_len);
        if (status != PSA_SUCCESS) {
            printf("Failed to export public key for encryption: %d\n", status);
            return status;
        }

        /* import encryption key (rsa public key) */
        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
        psa_set_key_algorithm(&attributes,   alg_encrypt);
        psa_set_key_type(&attributes,        PSA_KEY_TYPE_RSA_PUBLIC_KEY);
        psa_set_key_bits(&attributes,        1024);
        //psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_PERSISTENT);
        status = psa_import_key(&attributes, rsa_pub_key_buf, key_len, &key_rsa_crypt);
        if (status != PSA_SUCCESS) {
            printf("Failed to import public key for encryption: %d\n", status);
            return status;
        }
    }

    return 0; // PSA_SUCCESS
}

#ifdef UNTRUSTED
psa_status_t tp_sensor_data_get(float* temp, float* humidity, tp_mac_t* mac_out)
#else
static psa_status_t tfm_tp_sensor_data_get(void* handle)
#endif
{
    float    buffer[2];
    tp_mac_t mac;
#ifdef TRUSTED
    float* temp     = &buffer[0];
    float* humidity = &buffer[1];
#endif

    #if !defined(EMULATED)
    /* measuring request (MR) command to sensor */
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
                *temp     = ((float)(raw_value_temp) * 165.0F / 16383.0F) - 40.0F; // 14 bits, -40°C - +125°C
                *humidity = (float)raw_value_humid * 100.0F / 16383.0F; // 14 bits, 0% - 100%

            } else {
                printf("Error converting raw data to normal values.\n");
            }
        }
    }

    /* update oled display */
    if (TP_USE_DISPLAY) {
        Paint_DrawString_EN(10, 0,  "SENSOR",   &Font16, WHITE, WHITE);
        Paint_DrawString_EN(10, 20,  "Temp.",   &Font12, WHITE, WHITE);
        Paint_DrawNum(60, 20, *temp, &Font12,  4, WHITE, WHITE);
        Paint_DrawString_EN(10, 40, "Humid.", &Font12,  WHITE, WHITE);
        Paint_DrawNum(60, 40, *humidity, &Font12, 5, WHITE, WHITE);

        /* needs to at the end */
        OLED_0in96_display(our_image);
        Driver_Delay_ms(2000);
        Paint_Clear(BLACK);
    }
    #else
    /* made up sensor data for emulated setup */
    {
        static float change = 0;
        *temp     = 20 + change;
        *humidity = 50 + change;
        change += 1;
    }
    #endif


    /* hash & sign data */
    {
        psa_status_t status;
        psa_key_handle_t key_handle;
        psa_key_id_t key_slot = 1;

#ifdef UNTRUSTED
        buffer[0] = *temp;
        buffer[1] = *humidity;
#endif

        /* COMPUTE HASH (SHA256) */
        {
            size_t p_hash_length;
            status = psa_hash_compute(PSA_ALG_SHA_256, (uint8_t*) buffer, sizeof(buffer),
                                      mac.hash, MAC_HASH_SIZE, &p_hash_length);
            if (status != PSA_SUCCESS) {
                printf("Failed to compute hash: %d\n", status);
                return status;
            }
        }

        /* SIGN HASH (RSA) */
        {
            size_t sig_len;
            status = psa_sign_hash(key_rsa_sign, alg_sign, mac.hash, sizeof(mac.hash),
                                   mac.sign, sizeof(mac.sign), &sig_len);
            if (status != PSA_SUCCESS) {
                printf("Failed to sign hash: %d\n", status);
                return status;
            }

            if (sig_len != sizeof(mac.sign))
            {
                /* we expect the signature to fit perfectly */
                printf("Signature length and size are different: %u %u\n", sig_len, sizeof(mac.sign));
                return -1;
            }
        }

        /* VERIFY SIGNATURE */
        {
            status = psa_verify_hash(key_rsa_sign, alg_sign, mac.hash, sizeof(mac.hash),
                                     mac.sign, sizeof(mac.sign));
            if (status != PSA_SUCCESS) {
                printf("Couldn't verify signature: %d\n", status);
                return status;
            }
        }

        /* TODO ENCRYPT DATA (RSA) */
        {
            psa_status_t psa_asymmetric_encrypt(psa_key_id_t key,
                                    psa_algorithm_t alg,
                                    const uint8_t * input,
                                    size_t input_length,
                                    const uint8_t * salt,
                                    size_t salt_length,
                                    uint8_t * output,
                                    size_t output_size,
                                    size_t * output_length);
        }

        /* WRITE TO OUTPUT PARAMS */
        {
        #ifdef TRUSTED
            psa_write((psa_handle_t)handle, 0, temp, sizeof(*temp));
            psa_write((psa_handle_t)handle, 1, humidity, sizeof(*humidity));
            psa_write((psa_handle_t)handle, 2, &mac, sizeof(mac));
        #else
            *mac_out = mac;
        #endif
        }

    }

    return 0; // PSA_SUCCESS
}

#ifdef TRUSTED
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

psa_status_t tfm_tp_sensor_data_get_sfn(const psa_msg_t *msg)
{
    switch (msg->type) {

    case TP_API_INIT:
        return tfm_tp_init();
    case TP_SENSOR_DATA_GET:
        return tfm_tp_sensor_data_get(msg->handle);
    default:
        return PSA_ERROR_PROGRAMMER_ERROR;
    }

    return PSA_ERROR_GENERIC_ERROR;
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

#ifdef CONFIG_TFM_IPC
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
#endif
#endif
