#include "tfm_api.h"

psa_status_t tp_hal_init();

/* TODO test code */
typedef enum
{
    TP_TYPE_NONE = 0,
    TP_TYPE_NORMAL,
    TP_TYPE_SPECIAL,
    TP_TYPE_COUNT,
} tp_discriminator_e;
typedef struct
{
    int32_t            foo;
    float              bar;
    tp_discriminator_e type;
    void*              ptr;
} tp_fill_struct_t;
typedef struct
{
    char name[5];
} tp_return_struct_t;
tp_return_struct_t tp_our_function(uint32_t first, tp_fill_struct_t* out);
void tp_check_memory();
