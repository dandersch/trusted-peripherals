#ifdef UNTRUSTED
  #include <stdint.h> // for int32_t
  #include <stddef.h> // for size_t
  typedef int32_t psa_status_t;
#else
  #include "tfm_api.h"
#endif

/* TODO duplicated on secure side */
#define MAC_HASH_SIZE 256
typedef struct {
    uint8_t buf[MAC_HASH_SIZE];  /* lets assume our data packet fits into this buffer */
} tp_mac_t;

/*
 *  NOTE hash size in bytes
 *  TODO get correct one with PSA_HASH_LENGTH(alg) or use PSA_MAC_MAX_SIZE, PSA_HASH_MAX_SIZE
 */

psa_status_t tp_sensor_data_get(float* temp, float* humidity, void* mac, size_t mac_size);
