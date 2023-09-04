#include <tfm_ns_interface.h>

#include "trusted_peripheral_ns.h"

#if defined(CONFIG_TFM_IPC)
#include "psa/client.h"
#include "psa_manifest/sid.h"
#endif

psa_status_t tp_sensor_data_get(float* temp, float* humidity)
{
    psa_status_t status;
    psa_handle_t handle;

    psa_invec  in_vec[]  = { { .base = NULL, .len = 0 } }; // input parameter
    psa_outvec out_vec[] = {
        { .base = temp,     .len = sizeof(float*) },
        { .base = humidity, .len = sizeof(float*) },
    }; // output parameter

#if defined(CONFIG_TFM_IPC)
    handle = psa_connect(TFM_TP_SENSOR_DATA_GET_SID, TFM_TP_SENSOR_DATA_GET_VERSION);
    if (!PSA_HANDLE_IS_VALID(handle)) { return PSA_ERROR_GENERIC_ERROR; }
    status = psa_call(handle, PSA_IPC_CALL, in_vec, IOVEC_LEN(in_vec), out_vec, IOVEC_LEN(out_vec));
    psa_close(handle);
#else
    /* NOTE SFN version would look like this */
    //status = tfm_ns_interface_dispatch((veneer_fn)..._veneer,
    //                                   (uint32_t)in_vec,  IOVEC_LEN(in_vec),
    //                                   (uint32_t)out_vec, IOVEC_LEN(out_vec));
#endif

    return status;
}
