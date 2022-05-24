/*
    Command line controlled over the USB serial interface
*/

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

static const char *CLI_TAG = "CLI";
#define PROMPT_STR CONFIG_IDF_TARGET

static void (*senderCallback)();

void register_sender_callback(void (*cb)())
{
    senderCallback = cb;
}

int sendLTC()
{
    if (senderCallback)
    {
        ESP_LOGI(CLI_TAG, "Calling the cb");
        (*senderCallback)();
        return 0;
    }
    return 1;
}

/** Arguments used by 'show_clock' function */
static struct
{
    struct arg_dbl *time; /* time duration printing the current time (in milliseconds) */
    struct arg_dbl *step; /* step between 2 prints (in milliseconds)  */
    struct arg_end *end;
} show_clock_args;

/* Printing the esp clock timestamp on the monitor */
static int show_clock(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&show_clock_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, show_clock_args.end, argv[0]);
        return 1;
    }

    /* default values */
    double duration = 1000.0;
    double step = 1000.0;

    if (show_clock_args.time->count)
    {
        duration = show_clock_args.time->dval[0];
        ESP_LOGI(CLI_TAG, "duration: %f", show_clock_args.time->dval[0]);
    }
    if (show_clock_args.step->count)
    {
        step = show_clock_args.step->dval[0];
        ESP_LOGI(CLI_TAG, "step: %f", step);
    }

    double iterations = duration / step;
    ESP_LOGI(CLI_TAG, "Start printing timestamp...");
    ESP_LOGI(CLI_TAG, "iterations %d", (int)iterations);

    for (int i = 0; i < (int)iterations; i++)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        printf("Clock : secs %ld, usecs: %ld\n", tv.tv_sec, tv.tv_usec);
        vTaskDelay(step / portTICK_RATE_MS);
    }

    return 0;
}

void register_cmd(void)
{
    show_clock_args.time = arg_dbl0(NULL, "time", "<t>", "Printing time duration, ms");
    show_clock_args.step = arg_dbl0(NULL, "step", "<t>", "Step between 2 clock printings, ms");
    show_clock_args.end = arg_end(2);

    const esp_console_cmd_t show_clock_cmd = {
        .command = "show_clock",
        .help = "Print timestamps",
        .hint = NULL,
        .func = &show_clock,
        .argtable = &show_clock_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&show_clock_cmd));

    const esp_console_cmd_t sendLTC_cmd = {
        .command = "sendLTC",
        .help = "Generate and send LTC timestamp",
        .hint = NULL,
        .func = &sendLTC,
        .argtable = NULL};

    ESP_ERROR_CHECK(esp_console_cmd_register(&sendLTC_cmd));
}

/* console initialization and commands registering */
void start_cli(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    /* Prompt to be printed before each line. */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 1024;

    /* Register commands */
    esp_console_register_help_command();
    register_cmd();

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
