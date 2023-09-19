#ifdef UNTRUSTED
  #include <stdint.h> // for int32_t
  #include <stddef.h> // for size_t
  typedef int32_t psa_status_t;
#else
  #include "tfm_api.h"
#endif

// PSA_HASH_MAX_SIZE      64
// PSA_SIGNATURE_MAX_SIZE 512
#define MAC_HASH_SIZE 32
#define MAC_SIGN_SIZE 128
typedef struct {
    uint8_t hash[MAC_HASH_SIZE];
    uint8_t sign[MAC_SIGN_SIZE];
} tp_mac_t;

#define TP_API_INIT          32
#define TP_SENSOR_DATA_GET   33

#define TP_TRUSTED_DELIVERY   34
#define TP_TRUSTED_CAPTURE    35
#define TP_TRUSTED_TRANSFORM  36
//#define TP_TRUSTED_TRANSFORM2 37

psa_status_t tp_init();

psa_status_t tp_sensor_data_get(float* temp, float* humidity, tp_mac_t* mac_out);


/*
** TRUSTED CAPTURE
*/

typedef struct
{
    float temp;
    float humidity;
} sensor_data_t;

psa_status_t tp_trusted_capture(sensor_data_t* data_out, tp_mac_t* mac_out);

/*
** TRUSTED DELIVERY
*/

/* normal application doesn't know about sensor_data_t in case of trusted delivery */
#define ENCRYPTED_SENSOR_DATA_SIZE 128

psa_status_t tp_trusted_delivery(void* data_out, tp_mac_t* mac_out);


/*
** TRUSTED TRANSFORM
*/
#define MAX_TRANSFORMS 4
typedef enum
{
    TRANSFORM_ID_INITIAL, /* signals initial data capture */
    // TRANSFORM_ID_CONVERT_CELCIUS_TO_FAHRENHEIT, // (celsius * 9/5) + 32;
    TRANSFORM_ID_CONVERT_CELCIUS_TO_FAHRENHEIT, // (celsius * 9/5) + 32;
    TRANSFORM_ID_CENSOR_FACES,
    TRANSFORM_ID_CROP_PHOTO,
    /* ... */
    TRANSFORM_ID_COUNT,
} transform_id;

typedef struct
{
    transform_id type;
    union
    {
        float parameter;
    };
} transform_t;

psa_status_t tp_trusted_transform(sensor_data_t* data_io, transform_t* list_io,
                                  tp_mac_t* mac_out, transform_t transform);

/* TODO change to use handles */
psa_status_t tp_encrypted_transform(void* data_io, tp_mac_t* mac_io, transform_t transform);
