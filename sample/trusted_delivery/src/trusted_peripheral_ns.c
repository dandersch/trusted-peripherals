#include "trusted_peripheral.h"

/*
** Implements the tp_api to pass execution over to the secure side over an IPC mechanism
*/

#include <stdio.h>

#include <tfm_ns_interface.h>

#if defined(CONFIG_TFM_IPC)
#include "psa/client.h"
#endif
#include "psa_manifest/sid.h"

psa_status_t tp_sensor_data_get(float* temp, float* humidity, void* mac, size_t mac_size)
{
    psa_status_t status;

    psa_invec  in_vec[]  = { { .base = NULL, .len = 0 } }; // input parameter
    psa_outvec out_vec[] = {
        { .base = temp,     .len = sizeof(temp)   },
        { .base = humidity, .len = sizeof(humidity)   },
        { .base = mac,      .len = mac_size   }, // causes psa error
    }; // output parameter

#if defined(CONFIG_TFM_IPC)
    printf("USING IPC: ");

    psa_handle_t handle;
    handle = psa_connect(TFM_TP_SENSOR_DATA_GET_SID, TFM_TP_SENSOR_DATA_GET_VERSION);

    if (!PSA_HANDLE_IS_VALID(handle)) { return PSA_ERROR_GENERIC_ERROR; }

    status = psa_call(handle, PSA_IPC_CALL, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));

    psa_close(handle);
#else
    printf("USING SFN: ");

    status = psa_call(TFM_TP_SENSOR_DATA_GET_HANDLE, 0, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));
#endif

    return status;
}
