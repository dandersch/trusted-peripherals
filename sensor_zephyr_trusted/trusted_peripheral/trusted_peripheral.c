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
/* TODO duplicated on non-secure side */
#define MAC_HASH_SIZE 256 /* NOTE in bytes, TODO get correct one with PSA_HASH_LENGTH(alg) or use
                           * PSA_MAC_MAX_SIZE
                           * PSA_HASH_MAX_SIZE
                           */
typedef struct {
    uint8_t buf[MAC_HASH_SIZE];  /* lets assume our data packet fits into this buffer */
} tp_mac_t;
static psa_status_t crp_imp_key_secp256r1(psa_key_id_t key_id, psa_key_usage_t key_usage, uint8_t *key_data);
static psa_status_t tfm_tp_sensor_data_get(void* handle)
{
    float temp_and_humidity[2];
    //float temp;
    //float humidity;
    tp_mac_t mac;

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
                temp_and_humidity[0]     = ((float)(raw_value_temp) * 165.0F / 16383.0F) - 40.0F; // 14 bits, -40°C - +125°C
                temp_and_humidity[1] = (float)raw_value_humid * 100.0F / 16383.0F; // 14 bits, 0% - 100%

            } else {
                printf("Error converting raw data to normal values.\n");
            }
        }
    }

    psa_write((psa_handle_t)handle, 0, &temp_and_humidity[0], sizeof(&temp_and_humidity[0]));
    psa_write((psa_handle_t)handle, 1, &temp_and_humidity[1], sizeof(&temp_and_humidity[1]));

    /* hash & sign data */
    if (0) {
        /* NOTE static private key for testing */
        static uint8_t priv_key_data[32] = {
            0x14, 0xbc, 0xb9, 0x53, 0xa4, 0xee, 0xed, 0x50,
            0x09, 0x36, 0x92, 0x07, 0x1d, 0xdb, 0x24, 0x2c,
            0xef, 0xf9, 0x57, 0x92, 0x40, 0x4f, 0x49, 0xaa,
            0xd0, 0x7c, 0x5b, 0x3f, 0x26, 0xa7, 0x80, 0x48
        };

        psa_status_t status;
        status = psa_crypto_init();
        if (status != PSA_SUCCESS) { /* TODO  */ }

        psa_key_id_t key_slot = 1;
        status = crp_imp_key_secp256r1(key_slot,
                                       PSA_KEY_USAGE_SIGN_HASH |
                                       PSA_KEY_USAGE_VERIFY_HASH,
                                       priv_key_data);
        psa_key_handle_t key_handle;

        size_t p_hash_length;
        status = psa_hash_compute(PSA_ALG_SHA_256,
                                  (uint8_t*) temp_and_humidity,
                                  sizeof(temp_and_humidity), // should work as long it's a real array
                                  mac.buf,
                                  MAC_HASH_SIZE, // TODO we also have this as an input parameter
                                  &p_hash_length
                                 );
        // assert(p_hash_length == PSA_HASH_LENGTH(PSA_ALG_SHA_256));

        /* test signing */
        psa_open_key(key_slot, &key_handle);
        if (status != PSA_SUCCESS) { // Failed to open persistent key
            return status;
        }

        uint8_t sig[PSA_VENDOR_ECDSA_SIGNATURE_MAX_SIZE] = { 0 };
        size_t sig_len;

        psa_sign_hash(key_handle,
                      PSA_ALG_ECDSA(PSA_ALG_SHA_256), // TODO check if correct
                      mac.buf,
                      MAC_HASH_SIZE,
                      sig,
                      sizeof(sig),
                      &sig_len),

        //psa_status_t psa_sign_message(psa_key_id_t key,
        //                              psa_algorithm_t alg,
        //                              const uint8_t * input,
        //                              size_t input_length,
        //                              uint8_t * signature,
        //                              size_t signature_size,
        //                              size_t * signature_length);

        /* TODO close key handle */

        psa_write((psa_handle_t)handle, 2, &mac, sizeof(mac));
    }


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

/**
 * @brief Stores a new persistent secp256r1 key (usage: ecdsa-with-SHA256)
 *        in ITS, associating it with the specified unique key identifier.
 *
 * This function will store a new persistent secp256r1 key in internal trusted
 * storage. Cryptographic operations can then be performed using the key
 * identifier (key_id) associated with this persistent key. Only the 32-byte
 * private key needs to be supplied, the public key can be derived using
 * the supplied private key value.
 *
 * @param key_id        The permament identifier for the generated key.
 * @param key_usage     The usage policy for the key.
 * @param key_data      Pointer to the 32-byte private key data.
 */
static psa_status_t crp_imp_key_secp256r1(psa_key_id_t key_id, psa_key_usage_t key_usage, uint8_t *key_data)
{
    psa_status_t status = PSA_SUCCESS;
    psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_type_t key_type = PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1);
    psa_algorithm_t alg = PSA_ALG_ECDSA(PSA_ALG_SHA_256);
    psa_key_handle_t key_handle;
    size_t key_len = 32;
    size_t data_len; (void) data_len;
    uint8_t data_out[65] = { 0 }; /* ECDSA public key = 65 bytes. */ (void) data_out;
    int comp_result; (void) comp_result;

    /* Setup the key's attributes before the creation request. */
    psa_set_key_id(&key_attributes, key_id);
    psa_set_key_usage_flags(&key_attributes, key_usage);
    psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_PERSISTENT);
    psa_set_key_algorithm(&key_attributes, alg);
    psa_set_key_type(&key_attributes, key_type);

    /* Import the private key, creating the persistent key on success */
    status = psa_import_key(&key_attributes, key_data, key_len, &key_handle);
    if (status != PSA_SUCCESS) { // Failed to import key
        return status;
    }

    /* Close the key to free up the volatile slot. */
    status = psa_close_key(key_handle);
    if (status != PSA_SUCCESS) { // Failed to close persistent key
        return status;
    }

#if 0
    /* Try to retrieve the public key. */
    status = crp_get_pub_key(key_id, data_out, sizeof(data_out), &data_len);

    /* Export the private key if usage includes PSA_KEY_USAGE_EXPORT. */
    if (key_usage & PSA_KEY_USAGE_EXPORT) {
        /* Re-open the persisted key based on the key ID. */
        status = psa_open_key(key_id, &key_handle);
        if (status != PSA_SUCCESS) { // Failed to open persistent key
            return status;
        }

        /* Read the original (private) key data back. */
        status = psa_export_key(key_handle, data_out, sizeof(data_out), &data_len);
        if (status != PSA_SUCCESS) { // Failed to export key
            return status;
        }

        /* Check key len. */
        if (data_len != key_len) { // Unexpected number of bytes in exported key
            return status;
        }

        /* Verify that the exported private key matches input data. */
        comp_result = memcmp(data_out, key_data, key_len);
        if (comp_result != 0) { // Imported/exported private key mismatch
            return status;
        }

        /* Display the private key. */
        //LOG_INF("Private key data:");
        //al_dump_log();
        //sf_hex_tabulate_16(&crp_fmt, data_out, data_len);

        /* Close the key to free up the volatile slot. */
        status = psa_close_key(key_handle);
        if (status != PSA_SUCCESS) { // Failed to close persistent key.
            return status;
        }
    }
#endif

    return status;

}
