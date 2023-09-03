#include <tfm_ns_interface.h>

#include "trusted_peripheral_ns.h"

#if defined(CONFIG_TFM_IPC)
#include "psa/client.h"
#include "psa_manifest/sid.h"
#endif


psa_status_t tp_hal_init()
{
	psa_status_t status;
	psa_handle_t handle;

	psa_invec  in_vec[]  = { { .base = NULL, .len = 0 } }; // input parameter
	psa_outvec out_vec[] = { { .base = NULL, .len = 0 } }; // output parameter

#if defined(CONFIG_TFM_IPC)
	handle = psa_connect(TFM_TP_HAL_INIT_SID, TFM_TP_HAL_INIT_VERSION);
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

/* TODO move to secure side */
#include <stdio.h> // TODO does this make tf-m include libc?
static int32_t our_value = 1337;
void tp_check_memory() { printf("%i \n", our_value); }
tp_return_struct_t tp_our_function(uint32_t first, tp_fill_struct_t* out)
{
	char msg[5] = "hello";
	/* TODO how to print out of secure partition? */
    //printk("Received: %i\n", out->foo);

    printf("Received: %i\n", out->foo);
    printf("Received: %i\n", out->foo);

	if (out->foo != 42) { msg[0] = 'e'; msg[1] = 'r'; msg[2] = 'r'; msg[3] = 'o'; msg[4] = 'r'; }

    out->bar = 3.141f;
	out->ptr = (void*) &our_value;

	tp_return_struct_t ret = {"hello"};

	return ret;
}
