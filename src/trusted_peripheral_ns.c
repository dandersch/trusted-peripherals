#include "trusted_peripheral.h"

/*
** Implements the tp_api to pass execution over to the secure side over an IPC mechanism
*/

#include <tfm_ns_interface.h>

#if defined(CONFIG_TFM_IPC)
#include "psa/client.h"
#endif
#include "psa_manifest/sid.h"

psa_status_t tp_init()
{
    psa_status_t status;
    uint32_t api_call = TP_API_INIT;

    psa_outvec out_vec[] = { { .base = NULL, .len = 0 } };

    #if defined(CONFIG_TFM_IPC)
        printf("USING IPC: ");

        /* NOTE we encode our api call into the in_vec to be able to dispatch */
        psa_invec  in_vec[]  = { { .base = &api_call, .len = sizeof(uint32_t) } };

        psa_handle_t handle;
        handle = psa_connect(TFM_TP_SENSOR_DATA_GET_SID, TFM_TP_SENSOR_DATA_GET_VERSION);
        if (!PSA_HANDLE_IS_VALID(handle)) { return PSA_ERROR_GENERIC_ERROR; }

        status = psa_call(handle, PSA_IPC_CALL, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));

        psa_close(handle);
    #else
        printf("USING SFN\n");

        psa_invec  in_vec[]  = { { .base = NULL, .len = 0 } };
        status = psa_call(TFM_TP_SENSOR_DATA_GET_HANDLE, api_call, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));
    #endif

    return status;
}

psa_status_t tp_sensor_data_get(float* temp, float* humidity, tp_mac_t* mac_out)
{
    psa_status_t status;
    uint32_t api_call = TP_SENSOR_DATA_GET;

    psa_outvec out_vec[] = {
        { .base = temp,     .len = sizeof(temp)     },
        { .base = humidity, .len = sizeof(humidity) },
        { .base = mac_out,  .len = sizeof(tp_mac_t) }, // causes psa error
    }; // output parameter

#if defined(CONFIG_TFM_IPC)
    /* NOTE we encode our api call into the in_vec to be able to dispatch */
    psa_invec  in_vec[]  = { { .base = &api_call, .len = sizeof(uint32_t) } };

    psa_handle_t handle;
    handle = psa_connect(TFM_TP_SENSOR_DATA_GET_SID, TFM_TP_SENSOR_DATA_GET_VERSION);
    if (!PSA_HANDLE_IS_VALID(handle)) { return PSA_ERROR_GENERIC_ERROR; }

    status = psa_call(handle, PSA_IPC_CALL, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));

    psa_close(handle);
#else
    psa_invec  in_vec[]  = { { .base = NULL, .len = 0 } };

    status = psa_call(TFM_TP_SENSOR_DATA_GET_HANDLE, api_call, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));
#endif

    return status;
}
