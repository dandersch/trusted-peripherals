#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include <psa/crypto.h>

#ifdef UNTRUSTED
#else
  #include <tfm_ns_interface.h>
#endif

#include "trusted_peripheral.h"

#ifdef EMULATED
    //#define GET_TICK() sys_clock_elapsed()
    #define GET_TICK() k_uptime_ticks()
#else
    #define GET_TICK() HAL_GetTick()
#endif

#include <zephyr/timing/timing.h> /* for profiling */

// TODO uart transmission
#include <zephyr/drivers/uart.h>  /* for transmission */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

#define TEST_PERFORMANCE_TRUSTED_CAPTURE    0
#define TEST_PERFORMANCE_TRUSTED_DELIVERY   0
#define TEST_PERFORMANCE_TRUSTED_TRANSFORM  1
#define TEST_PERFORMANCE_CONTEXT_SWITCH     0
#define TEST_TRANSMISSION_TRUSTED_CAPTURE   0

#if TEST_TRANSMISSION_TRUSTED_CAPTURE
#define SENSOR_READINGS 100
struct packet_t {
    tp_mac_t       mac [SENSOR_READINGS];
    sensor_data_t  data[SENSOR_READINGS];
} packet;
#endif

int main(void)
{
    psa_status_t ret = tp_init();
    if (ret != 0) { printk("Initializing TP service failed with status: %i\n", ret); }

    /* blink red led to signal start */
    #if !defined(EMULATED)
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_9);
    HAL_Delay(100);
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_9);
    #endif

#if TEST_PERFORMANCE_TRUSTED_CAPTURE
    tp_mac_t      mac  = {0};
    sensor_data_t data = {0};

    printk("PROFILING TC START\n");

    timing_start();
    for (int i = 0; i < 1000; i++)
    {
        uint64_t tick_begin = GET_TICK(); /* NOTE tick increments for every ms. */
        timing_t cycle_begin = timing_counter_get();

        /* use our trusted peripheral api */
        psa_status_t ret = tp_trusted_capture(&data, &mac);
        if (ret != 0) { printk("Trusted Capture failed with status: %i\n", ret); }

        uint64_t tick_end  = GET_TICK();
        timing_t cycle_end = timing_counter_get();
        uint64_t cycles    = timing_cycles_get(&cycle_begin, &cycle_end);
        uint64_t nanosecs  = timing_cycles_to_ns(cycles);

        //printk("(%" PRIu64 "", cycles);
        printk("%" PRIu64 "\n", nanosecs);
        //printk("%" PRIu64 " ms\n", tick_end - tick_begin);
        //printk("%u ms\n", tick_end - tick_begin);

        //printk("Temp: %f ",  data.temp);
        //printk("Humidity: %f\n", data.humidity);
    }
    timing_stop();

    /* toggle red led on to signal we are finished */
    #if !defined(EMULATED)
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_9);
    #endif

    return 0;
#endif

#if TEST_PERFORMANCE_TRUSTED_DELIVERY
    uint8_t   ciphertext[ENCRYPTED_SENSOR_DATA_SIZE] = {0};
    tp_mac_t  mac                                    = {0};

    printk("PROFILING TD START\n");

    timing_start();
    for (int i = 0; i < 1000; i++)
    {
        timing_t cycle_begin = timing_counter_get();

        psa_status_t ret = tp_trusted_delivery(ciphertext, &mac);
        if (ret != 0) { printk("Trusted Capture failed with status: %i\n", ret); }

        timing_t cycle_end = timing_counter_get();
        uint64_t cycles    = timing_cycles_get(&cycle_begin, &cycle_end);
        uint64_t nanosecs  = timing_cycles_to_ns(cycles);

        printk("%" PRIu64 "\n", nanosecs);
    }
    timing_stop();

    #if !defined(EMULATED)
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_9);
    #endif

    return 0;
#endif

#if TEST_PERFORMANCE_TRUSTED_TRANSFORM

    trusted_transform_t tt = {0};

    printk("PROFILING TT START\n");

    timing_start();
    for (int i = 0; i < 100; i++)
    {
        timing_t cycle_begin = timing_counter_get();

        transform_t transform = {0};
        psa_status_t ret = tp_trusted_transform(&tt, transform);
        if (ret != 0) { printk("Trusted Transform init failed with status: %i\n", ret); }

        /* perform example transformation */
        transform.type = TRANSFORM_ID_CONVERT_CELCIUS_TO_FAHRENHEIT;
        transform.convert_params[0] = 1.8f;
        transform.convert_params[1] = 32.f;
        tp_trusted_transform(&tt, transform);

        transform.type = TRANSFORM_ID_CONVERT_FAHRENHEIT_TO_CELCIUS;
        tp_trusted_transform(&tt, transform);

        transform.type = TRANSFORM_ID_CONVERT_CELCIUS_TO_FAHRENHEIT;
        tp_trusted_transform(&tt, transform);

        transform.type = TRANSFORM_ID_CONVERT_FAHRENHEIT_TO_CELCIUS;
        tp_trusted_transform(&tt, transform);

        transform.type = TRANSFORM_ID_CONVERT_CELCIUS_TO_FAHRENHEIT;
        tp_trusted_transform(&tt, transform);

        timing_t cycle_end = timing_counter_get();
        uint64_t cycles    = timing_cycles_get(&cycle_begin, &cycle_end);
        uint64_t nanosecs  = timing_cycles_to_ns(cycles);

        printk("%" PRIu64 "\n", nanosecs);
    }
    timing_stop();

    #if !defined(EMULATED)
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_9);
    #endif

    return 0;

#endif

#if TEST_PERFORMANCE_CONTEXT_SWITCH
    /* calulcate fibonacci numbers from 0 to 40 a hundred times */
    for (int i = 0; i < 100; i++)
    {
        timing_t cycle_begin = timing_counter_get();

        uint64_t result = 0;
        measure_context_switch(&result, i);

        timing_t cycle_end = timing_counter_get();
        uint64_t cycles    = timing_cycles_get(&cycle_begin, &cycle_end);
        uint64_t nanosecs  = timing_cycles_to_ns(cycles);

        //printk("result: %" PRIu64 "\n", result);
        printk("%" PRIu64 "\n",      nanosecs);
    }

#endif


#if TEST_TRANSMISSION_TRUSTED_CAPTURE

    printk("MEASURING TRANSMISSION %u TC START\n", SENSOR_READINGS);
    uint32_t tick_begin = GET_TICK();

    timing_start();
    for (int i = 0; i < SENSOR_READINGS; i++)
    {
        /* use our trusted peripheral api */
        psa_status_t ret = tp_trusted_capture(&packet.data[i], &packet.mac[i]);
        if (ret != 0) { printk("Trusted Capture failed with status: %i\n", ret); }
    }

    /* transmit */
    for (int i = 0; i < sizeof(packet); i++) {
        uart_poll_out(uart_dev, ((uint8_t*) &packet)[i]);
    }

    uint32_t tick_end  = GET_TICK();
    printk("\n");
    printk("MEASURING END WITH: %u ms\n", tick_end - tick_begin);

    return 0;
#endif

#if 0
    if (!device_is_ready(uart_dev)) {
        printk("UART device not found!");
        return 0;
    }

    uint8_t rx_buf[10] = {0};
    uart_rx_enable(uart_dev, rx_buf, 10, 50 * USEC_PER_MSEC);

    char in;
    int wait = -1;
    printf("Waiting for python script to send data.\n");
    while (wait == -1)
    {
        wait = uart_poll_in(uart_dev, &in);
    }

    uint8_t buf[4] = {'a', 'b', 'c', '\n'};
    for (int i = 0; i < 4; i++) {
        uart_poll_out(uart_dev, buf[i]);
    }

    return 0;

    timing_init();

    tp_mac_t mac = {0};
    uint8_t ciphertext[ENCRYPTED_SENSOR_DATA_SIZE] = {0};
    transform_t transforms[TP_MAX_TRANSFORMS] = {0};

    trusted_transform_t tt = {0};

    tt_handle_cipher_t hc = {0};

    psa_status_t ret = tp_init();
    if (ret != 0) { printk("Initializing TP service failed with status: %i\n", ret); }

    for (;;)
    {
        /* TODO try timing_ */
        timing_start();
        uint32_t tick_begin = GET_TICK(); /* NOTE tick increments for every ms. */
        timing_t cycle_begin = timing_counter_get();

        /* use our trusted peripheral api */
        sensor_data_t sensor_data = {0};
        psa_status_t ret = tp_trusted_capture(&sensor_data, &mac);
        if (ret != 0) { printk("Trusted Capture failed with status: %i\n", ret); }

        ret = tp_trusted_delivery(ciphertext, &mac);
        if (ret != 0) { printk("Trusted Delivery failed with status: %i\n", ret); }

        transform_t transform = {0};
        ret = tp_trusted_transform(&tt, transform);
        if (ret != 0) { printk("Trusted Transform init failed with status: %i\n", ret); }

        /* perform example transformation */
        transform.type = TRANSFORM_ID_CONVERT_CELCIUS_TO_FAHRENHEIT;
        transform.convert_params[0] = 1.8f;
        transform.convert_params[1] = 32.f;
        tp_trusted_transform(&tt, transform);
        transform.type = TRANSFORM_ID_CONVERT_FAHRENHEIT_TO_CELCIUS;
        tp_trusted_transform(&tt, transform);

        transform_t transform_2 = {0};
        ret = tp_trusted_handle(&hc, transform_2);
        if (ret != 0) { printk("Trusted Handle init failed with status: %i\n", ret); }
        transform_2.type = TRANSFORM_RESOLVE_HANDLE_AND_ENCRYPT;
        ret = tp_trusted_handle(&hc, transform_2);
        if (ret != 0) { printk("Trusted Handle encryption failed with status: %i\n", ret); }

        // printk("CIPHER: ");
        // for (int i = 0; i < ENCRYPTED_SENSOR_DATA_SIZE; i++)
        // {
        //     printk("%02x", hc.ciphertext[i]);
        // }
        // printk("\n");


        uint32_t tick_end  = GET_TICK();
        // TODO there is also timing_cycles_to_ns_avg()
        timing_t cycle_end = timing_counter_get();
        uint64_t cycles    = timing_cycles_get(&cycle_begin, &cycle_end);
        uint64_t nanosecs  = timing_cycles_to_ns(cycles);
        timing_stop();

        printk("(%" PRIu64 " cycles) ", cycles);
        printk("(%" PRIu64 " ns) ", nanosecs);
        printk("(%u ms) ", tick_end - tick_begin);
        printk("Temp: %f ",  tt.data.temp);
        printk("Humidity: %f\n", tt.data.humidity);


        printk("HASH: ");
        for (int i = 0; i < MAC_HASH_SIZE; i++)
        {
            printk("%02x", mac.hash[i]);
        }
        printk("\n");

        printk("SIGN: ");
        for (int i = 0; i < MAC_SIGN_SIZE; i++)
        {
            printk("%02x", mac.sign[i]);
        }
        printk("\n");

        #ifdef EMULATED
        //k_sleep(K_MSEC(10000000)); // emulated mps2_an521 is too fast
        #endif

    }
#endif

    return 0;
}
