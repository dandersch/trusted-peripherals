#include "trusted_peripheral.h"

/*
** Implements the tp_api to pass execution over to the secure side over an IPC or SFN mechanism
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

    /* NOTE we encode our api call into the in_vec to be able to dispatch in the IPC case */
    psa_invec  in_vec[]  = { { .base = &api_call, .len = sizeof(uint32_t) } };

    #if defined(CONFIG_TFM_IPC)
        printf("USING IPC: ");

        psa_handle_t handle;
        handle = psa_connect(TFM_TRUSTED_PERIPHERAL_SID, TFM_TRUSTED_PERIPHERAL_VERSION);
        if (!PSA_HANDLE_IS_VALID(handle)) { return PSA_ERROR_GENERIC_ERROR; }

        status = psa_call(handle, PSA_IPC_CALL, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));

        psa_close(handle);
    #else
        printf("USING SFN\n");

        status = psa_call(TFM_TRUSTED_PERIPHERAL_HANDLE, api_call, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));
    #endif

    return status;
}

psa_status_t tp_trusted_capture(sensor_data_t* data_out, tp_mac_t* mac_out)
{
    psa_status_t status;
    uint32_t api_call = TP_TRUSTED_CAPTURE;

    psa_outvec out_vec[] = {
        { .base = data_out,     .len = sizeof(sensor_data_t)     },
        { .base = mac_out,      .len = sizeof(tp_mac_t) },
    };

    /* NOTE we encode our api call into the in_vec to be able to dispatch in the IPC case */
    psa_invec  in_vec[]  = { { .base = &api_call, .len = sizeof(uint32_t) } };

#if defined(CONFIG_TFM_IPC)

    psa_handle_t handle;
    handle = psa_connect(TFM_TRUSTED_PERIPHERAL_SID, TFM_TRUSTED_PERIPHERAL_VERSION);
    if (!PSA_HANDLE_IS_VALID(handle)) { return PSA_ERROR_GENERIC_ERROR; }

    status = psa_call(handle, PSA_IPC_CALL, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));

    psa_close(handle);
#else
    status = psa_call(TFM_TRUSTED_PERIPHERAL_HANDLE, api_call, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));
#endif
}

psa_status_t tp_trusted_delivery(void* data_out, tp_mac_t* mac_out)
{
    psa_status_t status;
    uint32_t api_call = TP_TRUSTED_DELIVERY;

    psa_outvec out_vec[] = {
        { .base = data_out,     .len = ENCRYPTED_SENSOR_DATA_SIZE },
        { .base = mac_out,      .len = sizeof(tp_mac_t)           },
    };

    /* NOTE we encode our api call into the in_vec to be able to dispatch in the IPC case */
    psa_invec  in_vec[]  = { { .base = &api_call, .len = sizeof(uint32_t) } };

#if defined(CONFIG_TFM_IPC)

    psa_handle_t handle;
    handle = psa_connect(TFM_TRUSTED_PERIPHERAL_SID, TFM_TRUSTED_PERIPHERAL_VERSION);
    if (!PSA_HANDLE_IS_VALID(handle)) { return PSA_ERROR_GENERIC_ERROR; }

    status = psa_call(handle, PSA_IPC_CALL, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));

    psa_close(handle);
#else
    status = psa_call(TFM_TRUSTED_PERIPHERAL_HANDLE, api_call, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));
#endif
}

psa_status_t tp_trusted_transform(trusted_transform_t* data_io, transform_t transform)
{
    psa_status_t status;
    uint32_t api_call = TP_TRUSTED_TRANSFORM;

    psa_outvec out_vec[] = {
        { .base = data_io,     .len = sizeof(trusted_transform_t)                   },
    };

    /* NOTE we encode our api call into the in_vec to be able to dispatch in the IPC case */
    psa_invec  in_vec[]  = {
        { .base = &api_call,  .len = sizeof(uint32_t)                        },
        { .base = &transform, .len = sizeof(transform_t)                     },
        { .base = data_io,    .len = sizeof(trusted_transform_t)             },
    };

#if defined(CONFIG_TFM_IPC)
    psa_handle_t handle;
    handle = psa_connect(TFM_TRUSTED_PERIPHERAL_SID, TFM_TRUSTED_PERIPHERAL_VERSION);
    if (!PSA_HANDLE_IS_VALID(handle)) { return PSA_ERROR_GENERIC_ERROR; }

    status = psa_call(handle, PSA_IPC_CALL, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));

    psa_close(handle);
#else
    status = psa_call(TFM_TRUSTED_PERIPHERAL_HANDLE, api_call, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));
#endif
}
