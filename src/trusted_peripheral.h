#ifdef UNTRUSTED
  #include <stdint.h> // for int32_t
  #include <stddef.h> // for size_t
  typedef int32_t psa_status_t;
#else
  #include "tfm_api.h"
#endif

//#define MAC_HASH_SIZE 64  // == PSA_HASH_MAX_SIZE
#define MAC_HASH_SIZE 32 // TODO why does it need to be 32 for untrusted?
#define MAC_SIGN_SIZE 512 // == PSA_SIGNATURE_MAX_SIZE
typedef struct {
    uint8_t hash[MAC_HASH_SIZE];
    uint8_t sign[MAC_SIGN_SIZE];
} tp_mac_t;

#define TP_API_INIT          0
#define TP_SENSOR_DATA_GET   1

psa_status_t tp_init();

psa_status_t tp_sensor_data_get(float* temp, float* humidity, tp_mac_t* mac_out);
