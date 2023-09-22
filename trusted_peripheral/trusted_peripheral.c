#include <stdint.h>

#include <psa/crypto.h>
#include "../keys.h"

#include <string.h> // for memcpy, memcmp

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

/* keys */
#define RSA_KEY_SIZE 1024
static psa_key_id_t key_rsa_sign    = 1;
static psa_key_id_t key_rsa_verify  = 2; // stored on server
static psa_key_id_t key_rsa_crypt   = 3;
static psa_key_id_t key_rsa_decrypt = 4;   // stored on server
static psa_algorithm_t alg_sign    = PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256);
static psa_algorithm_t alg_encrypt = PSA_ALG_RSA_PKCS1V15_CRYPT;

#ifdef UNTRUSTED
#define TP_FUNC(name, ...) name(__VA_ARGS__)
#define TP_INTERNAL
#else
#define TP_FUNC(name, ...) tfm_##name(void* handle)
#define TP_INTERNAL        static
#endif

/* TODO no system for more than one handle yet */
static trusted_transform_t tt_internal = {0}; // for handle version

/*
 * INTERNAL HELPER FUNCTIONS
*/
static psa_status_t internal_capture(sensor_data_t* sensor_data_out)
{
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
                sensor_data_out->temp     = ((float)(raw_value_temp) * 165.0F / 16383.0F) - 40.0F; // 14 bits, -40°C - +125°C
                sensor_data_out->humidity = (float)raw_value_humid * 100.0F / 16383.0F; // 14 bits, 0% - 100%

            } else {
                printf("Error converting raw data to normal values.\n");
            }
        }
    }

    #else
    /* made up sensor data for emulated setup */
    {
        static float change = 0;
        sensor_data_out->temp     = 20 + change;
        sensor_data_out->humidity = 50 + change;
        change += 1;
    }
    #endif

    return 0;
}

/* get nth fibonacci number to measure performance */
static int compute_something(int n)
{
    if (n <= 1) {
        return n;
    }

    int prev = 0;
    int current = 1;
    int next;

    for (int i = 2; i <= n; i++) {
        next = prev + current;
        prev = current;
        current = next;
    }

    return current;
}

static psa_status_t internal_compute_mac(tp_mac_t* mac_out, sensor_data_t* sensor_data)
{
    psa_status_t status;

    /* COMPUTE HASH (SHA256) */
    {
        size_t p_hash_length;
        status = psa_hash_compute(PSA_ALG_SHA_256, (uint8_t*) sensor_data, sizeof(*sensor_data),
                                  mac_out->hash, MAC_HASH_SIZE, &p_hash_length);
        if (p_hash_length != MAC_HASH_SIZE)
        {
            /* we expect the hash to fit perfectly */
            printf("Hash length and size are different: %u %u\n", p_hash_length, sizeof(p_hash_length));
            return -1;
        }
        if (status != PSA_SUCCESS)
        {
            printf("Failed to compute hash: %d\n", status);
            return status;
        }
    }

    /* SIGN HASH (RSA) */
    {
        size_t sig_len;
        status = psa_sign_hash(key_rsa_sign, alg_sign, mac_out->hash, sizeof(mac_out->hash),
                               mac_out->sign, sizeof(mac_out->sign), &sig_len);
        if (status != PSA_SUCCESS)
        {
            printf("Failed to sign hash: %d\n", status);
            return status;
        }

        if (sig_len != sizeof(mac_out->sign))
        {
            /* we expect the signature to fit perfectly */
            printf("Signature length and size are different: %u %u\n", sig_len, sizeof(mac_out->sign));
            return -1;
        }
    }
}

static psa_status_t internal_verify_mac(tp_mac_t* mac, sensor_data_t* sensor_data)
{
    psa_status_t status;

    /* VERIFY HASH */
    {
        size_t p_hash_length;
        uint8_t test_hash[MAC_HASH_SIZE];

        status = psa_hash_compute(PSA_ALG_SHA_256, (uint8_t*) sensor_data, sizeof(*sensor_data),
                                  test_hash, MAC_HASH_SIZE, &p_hash_length);
        if (p_hash_length != MAC_HASH_SIZE)
        {
            /* we expect the hash to fit perfectly */
            printf("Hash length and size are different: %u %u\n", p_hash_length, sizeof(p_hash_length));
            return -1;
        }
        if (status != PSA_SUCCESS)
        {
            printf("Failed to compute test hash: %d\n", status);
            return status;
        }

        if (memcmp(test_hash, mac->hash, MAC_HASH_SIZE))
        {
            printf("Verifying hash failed: %d\n", status);
            return status;
        }
    }

    /* VERIFY SIGNATURE */
    {
        status = psa_verify_hash(key_rsa_sign, alg_sign, mac->hash, sizeof(mac->hash),
                                 mac->sign, sizeof(mac->sign));
        if (status != PSA_SUCCESS) {
            printf("Couldn't verify signature: %d\n", status);
            return status;
        }
    }
}

static psa_status_t internal_compute_mac_tt(tp_mac_t* mac_out, trusted_transform_t* tt)
{
    psa_status_t status;

    /* COMPUTE HASH (SHA256) */
    {
        size_t p_hash_length;
        status = psa_hash_compute(PSA_ALG_SHA_256, (uint8_t*) tt,
                                  sizeof(sensor_data_t) + (sizeof(transform_t) * TP_MAX_TRANSFORMS),
                                  tt->mac.hash, MAC_HASH_SIZE, &p_hash_length);
        if (p_hash_length != MAC_HASH_SIZE)
        {
            /* we expect the hash to fit perfectly */
            printf("Hash length and size are different: %u %u\n", p_hash_length, sizeof(p_hash_length));
            return -1;
        }
        if (status != PSA_SUCCESS)
        {
            printf("Failed to compute hash: %d\n", status);
            return status;
        }
    }

    /* SIGN HASH (RSA) */
    {
        size_t sig_len;
        status = psa_sign_hash(key_rsa_sign, alg_sign, tt->mac.hash, sizeof(tt->mac.hash),
                               tt->mac.sign, sizeof(tt->mac.sign), &sig_len);
        if (status != PSA_SUCCESS)
        {
            printf("Failed to sign hash: %d\n", status);
            return status;
        }

        if (sig_len != sizeof(tt->mac.sign))
        {
            /* we expect the signature to fit perfectly */
            printf("Signature length and size are different: %u %u\n", sig_len, sizeof(tt->mac.sign));
            return -1;
        }
    }
}

static psa_status_t internal_verify_mac_tt(tp_mac_t* mac, trusted_transform_t* tt)
{
    psa_status_t status;

    /* VERIFY HASH */
    {
        size_t p_hash_length;
        uint8_t test_hash[MAC_HASH_SIZE];

        status = psa_hash_compute(PSA_ALG_SHA_256, (uint8_t*) tt,
                                  sizeof(sensor_data_t) + (sizeof(transform_t) * TP_MAX_TRANSFORMS),
                                  test_hash, MAC_HASH_SIZE, &p_hash_length);
        if (p_hash_length != MAC_HASH_SIZE)
        {
            /* we expect the hash to fit perfectly */
            printf("Hash length and size are different: %u %u\n", p_hash_length, sizeof(p_hash_length));
            return -1;
        }
        if (status != PSA_SUCCESS)
        {
            printf("Failed to compute test hash: %d\n", status);
            return status;
        }

        if (memcmp(test_hash, mac->hash, MAC_HASH_SIZE))
        {
            printf("Verifying hash failed: %d\n", status);
            return status;
        }
    }

    /* VERIFY SIGNATURE */
    {
        status = psa_verify_hash(key_rsa_sign, alg_sign, tt->mac.hash, sizeof(tt->mac.hash),
                                 tt->mac.sign, sizeof(tt->mac.sign));
        if (status != PSA_SUCCESS) {
            printf("Couldn't verify signature: %d\n", status);
            return status;
        }
    }
}

static psa_status_t internal_encrypt(sensor_data_t* sensor_data, uint8_t* ciphertext_out, size_t buffer_size)
{
    psa_status_t status;
    size_t ciphertext_length;

    /* 1024 bit RSA only allows for the plaintext to be up to this large */
    static const uint32_t max_plaintext_size = (RSA_KEY_SIZE/8) - 11; // 117 bytes
    if (sizeof(*sensor_data) > max_plaintext_size)
    {
        printf("Plaintext too large to encrypt: %u vs. %u\n", sizeof(*sensor_data), max_plaintext_size);
        return -1;
    }

    status = psa_asymmetric_encrypt(key_rsa_crypt, alg_encrypt,
                                    (uint8_t*) sensor_data, sizeof(*sensor_data),
                                    NULL, 0, /* salt */
                                    ciphertext_out, buffer_size,
                                    &ciphertext_length);

    /* we assume the ciphertext is always 128 bytes for now */
    if (ciphertext_length > ENCRYPTED_SENSOR_DATA_SIZE)
    {
        printf("Ciphertext longer than expected: %u vs. %u\n", ciphertext_length, ENCRYPTED_SENSOR_DATA_SIZE);
        return -1;
    }

    if (status != PSA_SUCCESS)
    {
        printf("Couldn't encrypt peripheral data: %d\n", status);
        return status;
    }
}

/* not in real deployment */
static psa_status_t internal_decrypt(uint8_t* ciphertext, size_t ciphertext_length, sensor_data_t* sensor_data_out)
{
    psa_status_t status;
    size_t output_length; // NOTE: for now we know that the buffer will be completely filled
    status = psa_asymmetric_decrypt(key_rsa_decrypt, alg_encrypt,
                                    ciphertext, ciphertext_length,
                                    NULL, 0, /* salt */
                                    (uint8_t*) &sensor_data_out, sizeof(*sensor_data_out), /* putting the output back into the data for now */
                                    &output_length);
    if (status != PSA_SUCCESS)
    {
        printf("Couldn't decrypt ciphertext: %d\n", status);
        return status;
    }
    if (output_length != sizeof(*sensor_data_out))
    {
        printf("Decryption resulted in differently sized plain text: %u vs. %u\n", output_length, sizeof(sensor_data_out));
        return -1;
    }
}

static psa_status_t internal_encrypt_tt(trusted_transform_t* tt, uint8_t* ciphertext_out, size_t buffer_size)
{
    psa_status_t status;
    size_t ciphertext_length;

    size_t encryption_size = sizeof(sensor_data_t) + (sizeof(transform_t) * TP_MAX_TRANSFORMS);

    /* 1024 bit RSA only allows for the plaintext to be up to this large */
    static const uint32_t max_plaintext_size = (RSA_KEY_SIZE/8) - 11; // 117 bytes
    if (encryption_size > max_plaintext_size)
    {
        printf("Plaintext too large to encrypt: %u vs. %u\n", encryption_size, max_plaintext_size);
        return -1;
    }

    status = psa_asymmetric_encrypt(key_rsa_crypt, alg_encrypt,
                                    (uint8_t*) tt, encryption_size,
                                    NULL, 0, /* salt */
                                    ciphertext_out, buffer_size,
                                    &ciphertext_length);

    /* we assume the ciphertext is always 128 bytes for now */
    if (ciphertext_length > ENCRYPTED_SENSOR_DATA_SIZE)
    {
        printf("Ciphertext longer than expected: %u vs. %u\n", ciphertext_length, ENCRYPTED_SENSOR_DATA_SIZE);
        return -1;
    }

    if (status != PSA_SUCCESS)
    {
        printf("Couldn't encrypt peripheral data: %d\n", status);
        return status;
    }
}

/*
** TP API
*/
TP_INTERNAL psa_status_t TP_FUNC(tp_init)
{
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
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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

    /* setup keys */
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
        psa_set_key_bits(&attributes,        RSA_KEY_SIZE);

        status = psa_import_key(&attributes, key_data_rsa, sizeof(key_data_rsa), &key_rsa_sign);
        if (status != PSA_SUCCESS) {
            printf("Failed to import signing key: %d\n", status);
            return status;
        }

        /* import encryption key (rsa private key) */
        //attributes                         = PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT); /* decryption usage for testing */
        psa_set_key_algorithm(&attributes,   alg_encrypt);
        psa_set_key_type(&attributes,        PSA_KEY_TYPE_RSA_KEY_PAIR);
        psa_set_key_bits(&attributes,        RSA_KEY_SIZE);
        status = psa_import_key(&attributes, key_data_rsa_crypt, sizeof(key_data_rsa_crypt), &key_rsa_decrypt);
        if (status != PSA_SUCCESS) {
            printf("Failed to import private key for decryption: %d\n", status);
            return status;
        }

        /* get rsa public key from private key above */
        size_t key_len;
        psa_export_public_key(key_rsa_decrypt, rsa_pub_key_buf, sizeof(rsa_pub_key_buf), &key_len);
        if (status != PSA_SUCCESS) {
            printf("Failed to export public key for encryption: %d\n", status);
            return status;
        }

        /* import encryption key (rsa public key) */
        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
        psa_set_key_algorithm(&attributes,   alg_encrypt);
        psa_set_key_type(&attributes,        PSA_KEY_TYPE_RSA_PUBLIC_KEY);
        psa_set_key_bits(&attributes,        RSA_KEY_SIZE);
        status = psa_import_key(&attributes, rsa_pub_key_buf, key_len, &key_rsa_crypt);
        if (status != PSA_SUCCESS) {
            printf("Failed to import public key for encryption: %d\n", status);
            return status;
        }
    }

    return 0; // PSA_SUCCESS
}

TP_INTERNAL psa_status_t TP_FUNC(tp_trusted_capture, sensor_data_t* sensor_data_out, tp_mac_t* mac_out)
{
    psa_status_t status;
    sensor_data_t sensor_data;
    tp_mac_t mac;

    /* get peripheral data */
    status = internal_capture(&sensor_data);
    if (status != PSA_SUCCESS)
    {
        printf("Couldn't capture sensor data\n");
        return -1;
    }

    /* update oled display */
    if (TP_USE_DISPLAY)
    {
        #if !defined(EMULATED)
        Paint_DrawString_EN(10, 0,  "SENSOR",   &Font16, WHITE, WHITE);
        Paint_DrawString_EN(10, 20,  "Temp.",   &Font12, WHITE, WHITE);
        Paint_DrawNum(60, 20, sensor_data.temp, &Font12,  4, WHITE, WHITE);
        Paint_DrawString_EN(10, 40, "Humid.", &Font12,  WHITE, WHITE);
        Paint_DrawNum(60, 40, sensor_data.humidity, &Font12, 5, WHITE, WHITE);

        /* needs to at the end */
        OLED_0in96_display(our_image);
        Driver_Delay_ms(2000);
        Paint_Clear(BLACK);
        #endif
    }

    status = internal_compute_mac(&mac, &sensor_data);
    if (status != PSA_SUCCESS)
    {
        printf("Couldn't compute MAC\n");
        return -1;
    }

//    /* TODO only here for testing reasons, remove */
//    status = internal_verify_mac(&mac, &sensor_data);
//    if (status != PSA_SUCCESS)
//    {
//        printf("Couldn't verify MAC\n");
//        return -1;
//    }

    /* WRITE TO OUTPUT PARAMS */
    {
    #ifdef TRUSTED
        psa_write((psa_handle_t)handle, 0, &sensor_data, sizeof(sensor_data));
        psa_write((psa_handle_t)handle, 1, &mac,         sizeof(mac));
    #else
        *sensor_data_out = sensor_data;
        *mac_out         = mac;
    #endif
    }


    return status;
}

TP_INTERNAL psa_status_t TP_FUNC(tp_trusted_delivery, void* data_out, tp_mac_t* mac_out)
{
    psa_status_t status;
    sensor_data_t sensor_data;
    tp_mac_t mac;

    /* get peripheral data */
    status = internal_capture(&sensor_data);
    if (status != PSA_SUCCESS)
    {
        printf("Couldn't capture sensor data\n");
        return -1;
    }

    status = internal_compute_mac(&mac, &sensor_data);
    if (status != PSA_SUCCESS)
    {
        printf("Couldn't compute MAC\n");
        return -1;
    }

    status = internal_verify_mac(&mac, &sensor_data);
    if (status != PSA_SUCCESS)
    {
        printf("Couldn't verify MAC\n");
        return -1;
    }

    uint8_t ciphertext[ENCRYPTED_SENSOR_DATA_SIZE];
    status = internal_encrypt(&sensor_data, ciphertext, sizeof(ciphertext));
    if (status != PSA_SUCCESS)
    {
        printf("Couldn't encrypt sensor data\n");
        return -1;
    }

    /* WRITE TO OUTPUT PARAMS */
    {
    #ifdef TRUSTED
        psa_write((psa_handle_t)handle, 0, ciphertext, ENCRYPTED_SENSOR_DATA_SIZE);
        psa_write((psa_handle_t)handle, 1, &mac,       sizeof(mac));
    #else
        memcpy(data_out, ciphertext, sizeof(ciphertext));
        //*data_out = sensor_data;
        *mac_out  = mac;
    #endif
    }

    return status;
}

TP_INTERNAL psa_status_t TP_FUNC(tp_trusted_transform, trusted_transform_t* tt_io, transform_t transform)
{
    psa_status_t status = 0;
    trusted_transform_t tt;

#ifdef TRUSTED
    transform_t transform;
    size_t ret = psa_read((psa_handle_t) handle, 1, &transform, sizeof(transform_t));
    if (ret != sizeof(transform_t)) { return PSA_ERROR_PROGRAMMER_ERROR; }
    ret = psa_read((psa_handle_t) handle, 2, &tt, sizeof(trusted_transform_t));
    if (ret != sizeof(trusted_transform_t)) { return PSA_ERROR_PROGRAMMER_ERROR; }
#endif

    uint32_t list_index = 0;
    if (transform.type != TRANSFORM_ID_INITIAL)
    {
        /* find last entry in list */
        for (uint32_t i = 0; i < TP_MAX_TRANSFORMS; i++)
        {
            if (tt.list[0].type == TRANSFORM_ID_INITIAL)
            {
                list_index = i;
                break;
            }
        }

        if (list_index == TP_MAX_TRANSFORMS)
        {
            printf("Cannot perform transformation, list is full\n");
            return -1;
        }

        status = internal_verify_mac_tt(&tt.mac, &tt);
        if (status != PSA_SUCCESS)
        {
            printf("Couldn't verify MAC before transformation\n");
            return -1;
        }
    }

    switch (transform.type)
    {
        case TRANSFORM_ID_INITIAL:
        {
            /* empty the list */
            memset(tt.list, 0, TP_MAX_TRANSFORMS);

            /* get peripheral data */
            status = internal_capture(&tt.data);
            if (status != PSA_SUCCESS)
            {
                printf("Couldn't capture sensor data for initial transform\n");
                return -1;
            }

            status = internal_compute_mac_tt(&tt.mac, &tt);
            if (status != PSA_SUCCESS)
            {
                printf("Couldn't compute MAC for initial transform\n");
                return -1;
            }
        } break;

        case TRANSFORM_ID_CONVERT_CELCIUS_TO_FAHRENHEIT:
        {
            /* perform example transformation */
            tt.data.temp =  (tt.data.temp * transform.convert_params[0]) + transform.convert_params[1];

            /* append to list */
            tt.list[list_index] = transform;

            /* recompute mac */
            status = internal_compute_mac_tt(&tt.mac, &tt);
            if (status != PSA_SUCCESS)
            {
                printf("Couldn't compute MAC after transform\n");
                return -1;
            }
        } break;

        case TRANSFORM_ID_CONVERT_FAHRENHEIT_TO_CELCIUS:
        {
            /* perform example transformation */
            tt.data.temp = (tt.data.temp - transform.convert_params[1]) / transform.convert_params[0];

            /* append to list */
            tt.list[list_index] = transform;

            /* recompute mac */
            status = internal_compute_mac_tt(&tt.mac, &tt);
            if (status != PSA_SUCCESS)
            {
                printf("Couldn't compute MAC after transform\n");
                return -1;
            }
        } break;

    }

    /* WRITE TO OUTPUT PARAMS */
    {
    #ifdef TRUSTED
        psa_write((psa_handle_t)handle, 0, &tt, sizeof(trusted_transform_t));
    #else
        *tt_io           = tt;
    #endif
    }

    return status;
}

TP_INTERNAL psa_status_t TP_FUNC(tp_trusted_handle, tt_handle_cipher_t* hc_io, transform_t transform)
{
    psa_status_t status = 0;
    tt_handle_cipher_t hc;

#ifdef TRUSTED
    transform_t transform;
    size_t ret = psa_read((psa_handle_t) handle, 1, &transform, sizeof(transform_t));
    if (ret != sizeof(transform_t)) { return PSA_ERROR_PROGRAMMER_ERROR; }
    ret = psa_read((psa_handle_t) handle, 2, &hc, sizeof(tt_handle_cipher_t));
    if (ret != sizeof(tt_handle_cipher_t)) { return PSA_ERROR_PROGRAMMER_ERROR; }
#endif

    /* TODO resolve handle / assign handle */
    hc.handle = (void*) &tt_internal;

    uint32_t list_index = 0;
    if (transform.type != TRANSFORM_ID_INITIAL)
    {
        /* find last entry in list */
        for (uint32_t i = 0; i < TP_MAX_TRANSFORMS; i++)
        {
            if (tt_internal.list[0].type == TRANSFORM_ID_INITIAL)
            {
                list_index = i;
                break;
            }
        }

        if (list_index == TP_MAX_TRANSFORMS && transform.type != TRANSFORM_RESOLVE_HANDLE_AND_ENCRYPT)
        {
            printf("Cannot perform transformation, list is full\n");
            return -1;
        }

        status = internal_verify_mac_tt(&tt_internal.mac, &tt_internal);
        if (status != PSA_SUCCESS)
        {
            printf("Couldn't verify MAC before transformation\n");
            return -1;
        }
    }

    switch (transform.type)
    {
        case TRANSFORM_ID_INITIAL:
        {
            /* empty the list */
            memset(tt_internal.list, 0, TP_MAX_TRANSFORMS);

            /* get peripheral data */
            status = internal_capture(&tt_internal.data);
            if (status != PSA_SUCCESS)
            {
                printf("Couldn't capture sensor data for initial transform\n");
                return -1;
            }

            status = internal_compute_mac_tt(&tt_internal.mac, &tt_internal);
            if (status != PSA_SUCCESS)
            {
                printf("Couldn't compute MAC for initial transform\n");
                return -1;
            }
        } break;

        case TRANSFORM_RESOLVE_HANDLE_AND_ENCRYPT:
        {
            status = internal_encrypt_tt(&tt_internal, hc.ciphertext, ENCRYPTED_SENSOR_DATA_SIZE);
            if (status != PSA_SUCCESS)
            {
                printf("Couldn't encrypt resolved transform handle\n");
                return -1;
            }

            hc.mac = tt_internal.mac;
        } break;

        case TRANSFORM_ID_CONVERT_CELCIUS_TO_FAHRENHEIT:
        {
            /* perform example transformation */
            tt_internal.data.temp =  (tt_internal.data.temp * transform.convert_params[0]) + transform.convert_params[1];

            /* append to list */
            tt_internal.list[list_index] = transform;

            /* recompute mac */
            status = internal_compute_mac_tt(&tt_internal.mac, &tt_internal);
            if (status != PSA_SUCCESS)
            {
                printf("Couldn't compute MAC after transform\n");
                return -1;
            }
        } break;

        case TRANSFORM_ID_CONVERT_FAHRENHEIT_TO_CELCIUS:
        {
            /* perform example transformation */
            tt_internal.data.temp = (tt_internal.data.temp - transform.convert_params[1]) / transform.convert_params[0];

            /* append to list */
            tt_internal.list[list_index] = transform;

            /* recompute mac */
            status = internal_compute_mac_tt(&tt_internal.mac, &tt_internal);
            if (status != PSA_SUCCESS)
            {
                printf("Couldn't compute MAC after transform\n");
                return -1;
            }
        } break;

    }

    /* WRITE TO OUTPUT PARAMS */
    {
    #ifdef TRUSTED
        psa_write((psa_handle_t)handle, 0, &hc, sizeof(tt_handle_cipher_t));
    #else
        *hc_io = hc;
    #endif
    }

    return 0;
}

/* to measure context switch */
TP_INTERNAL psa_status_t TP_FUNC(measure_context_switch, uint32_t* trusted_start, uint32_t* trusted_end)
{
    /* ... */
    uint32_t start = HAL_GetTick();

    printf("30th: %u\n", compute_something(30));

    uint32_t end = HAL_GetTick();

    #ifdef TRUSTED
        psa_write((psa_handle_t)handle, 0, &start, sizeof(uint32_t));
        psa_write((psa_handle_t)handle, 1, &end,   sizeof(uint32_t));
    #else
        *trusted_start = start;
        *trusted_end   = end;
    #endif

    return 0;
}

/*
** TP SERVICE SPECIFIC
*/
#ifdef TRUSTED
static psa_status_t tfm_trusted_peripheral_ipc(psa_msg_t *msg)
{
    /* CHECK INPUT */
    size_t ret = 0;
    uint32_t api_call = 0;
    ret = psa_read(msg->handle, 0, &api_call, msg->in_size[0]); /* should return number of bytes copied */
    if (ret != msg->in_size[0]) { return PSA_ERROR_PROGRAMMER_ERROR; }

    switch (api_call) {
    case TP_API_INIT:
        return tfm_tp_init(NULL);
    case TP_TRUSTED_CAPTURE:
        return tfm_tp_trusted_capture(msg->handle);
    case TP_TRUSTED_DELIVERY:
        return tfm_tp_trusted_delivery(msg->handle);
    case TP_TRUSTED_TRANSFORM:
    {
        /* The size of arguments is incorrect */
        if (msg->in_size[1] != sizeof(transform_t))         { return PSA_ERROR_PROGRAMMER_ERROR; }
        if (msg->in_size[2] != sizeof(trusted_transform_t)) { return PSA_ERROR_PROGRAMMER_ERROR; }
        return tfm_tp_trusted_transform(msg->handle);
    }
    case TP_TRUSTED_HANDLE:
    {
        /* The size of arguments is incorrect */
        if (msg->in_size[1] != sizeof(transform_t))        { return PSA_ERROR_PROGRAMMER_ERROR; }
        if (msg->in_size[2] != sizeof(tt_handle_cipher_t)) { return PSA_ERROR_PROGRAMMER_ERROR; }
        return tfm_tp_trusted_handle(msg->handle);
    }
    case MEASURE_PERFORMANCE: return tfm_measure_context_switch(msg->handle);
    default:
        return PSA_ERROR_PROGRAMMER_ERROR;
    }

    return PSA_ERROR_GENERIC_ERROR;
}

psa_status_t tfm_trusted_peripheral_sfn(const psa_msg_t *msg)
{
    switch (msg->type) {

    case TP_API_INIT:
        return tfm_tp_init(NULL);
    case TP_TRUSTED_CAPTURE:
        return tfm_tp_trusted_capture(msg->handle);
    case TP_TRUSTED_DELIVERY:
        return tfm_tp_trusted_delivery(msg->handle);
    case TP_TRUSTED_TRANSFORM:
    {
        /* The size of arguments is incorrect */
        if (msg->in_size[1] != sizeof(transform_t))         { return PSA_ERROR_PROGRAMMER_ERROR; }
        if (msg->in_size[2] != sizeof(trusted_transform_t)) { return PSA_ERROR_PROGRAMMER_ERROR; }
        return tfm_tp_trusted_transform(msg->handle);
    }
    case TP_TRUSTED_HANDLE:
    {
        /* The size of arguments is incorrect */
        if (msg->in_size[1] != sizeof(transform_t))        { return PSA_ERROR_PROGRAMMER_ERROR; }
        if (msg->in_size[2] != sizeof(tt_handle_cipher_t)) { return PSA_ERROR_PROGRAMMER_ERROR; }
        return tfm_tp_trusted_handle(msg->handle);
    }
    case MEASURE_PERFORMANCE: return tfm_measure_context_switch(msg->handle);
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

/* TODO this does not get set w/ IPC mode */
#ifdef CONFIG_TFM_IPC
psa_status_t tfm_tp_req_mngr_init(void)
{
    psa_signal_t signals = 0;

    while (1) {
        signals = psa_wait(PSA_WAIT_ANY, PSA_BLOCK);

        if (signals & TFM_TP_SENSOR_DATA_GET_SIGNAL) {
            tp_signal_handle(TFM_TRUSTED_PERIPHERAL_SIGNAL, tfm_trusted_peripheral_ipc);
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
