/****************************************************************************
 *
 * This file is for the device acting as a slave
 * the bluetooth mode used SPP
 *
 ****************************************************************************/

#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"
#include "bt_devices_methods.h"

#include <time.h>
#include "timeCodeGen.h"
#include <ltc.h>
#include "console.h"

#define GAP_TAG "GAP"
#define BT_SPP_TAG "SPP"
#define BT_DEVICE_NAME "DEV_B"
#define SPP_SERVER_NAME "SERVER_SPP"

#if (SPP_SHOW_MODE == SPP_SHOW_DATA)
#define SPP_DATA_LEN 20
#else
#define SPP_DATA_LEN ESP_SPP_MAX_MTU
#endif
static uint8_t spp_data[SPP_DATA_LEN];

// #define SPP_DATA_LEN ESP_SPP_MAX_MTU
static int len_buf_spp = 0;
static uint8_t buf_spp_data[3 * 1920];

typedef enum
{
    APP_GAP_STATE_IDLE = 0,
    APP_GAP_STATE_DEVICE_DISCOVERING,
    APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE,
    APP_GAP_STATE_SERVICE_DISCOVERING,
    APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE,
} app_gap_state_t;

typedef struct
{
    bool dev_found;
    uint8_t bdname_len;
    uint8_t eir_len;
    uint8_t rssi;
    uint32_t cod;
    uint8_t eir[ESP_BT_GAP_EIR_DATA_LEN];
    uint8_t bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    esp_bd_addr_t bda;
    app_gap_state_t state;
} app_gap_cb_t;

typedef struct
{
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_cond_t cond2;
} thread_t;

static thread_t thread_encoder =
    {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER,
};

static thread_t thread_spp_writer =
    {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER,
        .cond2 = PTHREAD_COND_INITIALIZER,
};

void *encoder(void *p_data)
{
    while (1)
    {
        pthread_mutex_lock(&thread_encoder.mutex);
        ESP_LOGD(BT_SPP_TAG, "waiting for the signal");
        pthread_cond_wait(&thread_encoder.cond, &thread_encoder.mutex);
        ESP_LOGI(BT_SPP_TAG, "Starting encoder");
        genTimeCode(buf_spp_data, &len_buf_spp);
        /* cond signal to spp writer thread */
        pthread_cond_signal(&thread_spp_writer.cond);
        pthread_mutex_unlock(&thread_encoder.mutex);
    }
}

/* Initialize the LTC Encoder, which uses its own thread. */
esp_err_t init_encoder()
{
    esp_err_t ret;

    pthread_mutex_init(&thread_encoder.mutex, NULL);
    pthread_cond_init(&thread_encoder.cond, NULL);

    ret = pthread_create(&thread_encoder.thread, NULL, &encoder, NULL);

    return ret;
}

void *spp_writer(void *p_data)
{
    esp_spp_cb_param_t *param = p_data;
    esp_err_t ret;

    while (1)
    {
        pthread_mutex_lock(&thread_spp_writer.mutex);
        pthread_cond_wait(&thread_spp_writer.cond, &thread_spp_writer.mutex);
        gettimeofday(&latency.time_send_LTC, NULL);
        ESP_LOGI(BT_SPP_TAG, "writing data...\n");
        uint16_t diff_seconds = latency.time_send_LTC.tv_sec - latency.time_recvd_request.tv_sec;
        uint16_t diff_usec = latency.time_send_LTC.tv_usec - latency.time_recvd_request.tv_usec;
        uint16_t diff = diff_seconds*1000000 + diff_usec;
        while(len_buf_spp > 0)
        {
            

            uint8_t buf[300];
            int len;

            if (len_buf_spp > 300)
                len = 300;
            else
                len = len_buf_spp;
            memcpy(buf, buf_spp_data, len);
            // if (param->cong.cong == 0)
            {
                ESP_LOGI(BT_SPP_TAG, "senddebut");
                ret = esp_spp_write(129, len, buf_spp_data);
                ESP_LOGI(BT_SPP_TAG, "WAIT");
                struct timeval now;
                gettimeofday( &now, NULL );
                now.tv_usec += 500;
                pthread_cond_timedwait(&thread_spp_writer.cond2, &thread_spp_writer.mutex, &now);
                // pthread_cond_wait(&thread_spp_writer.cond2, &thread_spp_writer.mutex);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(BT_SPP_TAG, "%s spp write failed: %s\n", __func__, esp_err_to_name(ret));
                }
                else 
                {
                    len_buf_spp -= len;
                    memmove(buf_spp_data, &buf_spp_data[len], len_buf_spp);
                }
            }
        }

        uint8_t comp_code[3];
        comp_code[0] = SEND_COMP;
        comp_code[1] = diff;
        comp_code[2] = diff>>8;

        ESP_LOGI(BT_SPP_TAG, "diff = %d", diff);

        ret = esp_spp_write(129, 3, comp_code);
   
        if (ret != ESP_OK)
        {
            ESP_LOGE(BT_SPP_TAG, "%s spp write failed: %s\n", __func__, esp_err_to_name(ret));
        }
        
        pthread_mutex_unlock(&thread_spp_writer.mutex);
    }
}

/* Initialization of the thread used to write the LTC as SPP data, to send it to the slave later */
int init_spp_writer(esp_spp_cb_param_t *param)
{
    int ret;

    pthread_mutex_init(&thread_spp_writer.mutex, NULL);
    pthread_cond_init(&thread_spp_writer.cond, NULL);

    ret = pthread_create(&thread_spp_writer.thread, NULL, &spp_writer, param);

    return ret;
}

/* method to force LTC sending, might be useful to debug */
void start_sendLTC()
{
    /* inform the master device that we're about to start sending LTC */
    ESP_LOGD(BT_SPP_TAG, "start sendind LTC");
    uint8_t startTransmitLTC[1];
    startTransmitLTC[0] = START_SEND_LTC;
    esp_spp_write(129, 1, startTransmitLTC);
    /* Start the encoder's thread */
    pthread_cond_signal(&thread_encoder.cond);
}

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event)
    {
    case ESP_SPP_INIT_EVT:
        // Once the SPP callback has been registered, ESP_SPP_INIT_EVT is triggered. Here sets
        // the device name and classic bluetooth scan mode, then starts the advertising.
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_INIT_EVT\n");
        // Create SPP Server and start listening for an SPP connection request
        // upgrade: think about the security setting mask (NONE for now)
        // Slave: passive device, wait for a connection request
        // upgrade: do we need to choose another channel ? (0 == any)
        esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, SPP_SERVER_NAME);

        /* init a thread sending SPP data */
        init_spp_writer(param);
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT\n");
        break;
    // After the SPP connection, ESP_SPP_OPEN_EVT is triggered.
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_OPEN_EVT\n");
        break;
    // After the SPP disconnection, ESP_SPP_CLOSE_EVT is triggered.
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_CLOSE_EVT\n");
        break;
    case ESP_SPP_START_EVT:
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_START_EVT\n");
        esp_bt_dev_set_device_name(BT_DEVICE_NAME);
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        break;
    case ESP_SPP_CL_INIT_EVT:
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_CL_INIT_EVT\n");
        break;
    case ESP_SPP_DATA_IND_EVT:
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_DATA_IND_EVT len=%d handle=%d\n",
                 param->data_ind.len, param->data_ind.handle);
        esp_log_buffer_hex("", param->data_ind.data, param->data_ind.len);

        if(*(param->data_ind.data) == REQUEST_LTC_CODE)
        {
            gettimeofday(&latency.time_recvd_request, NULL);
            /* Start LTC generator */
            pthread_cond_signal(&thread_encoder.cond);
        }
        break;
    case ESP_SPP_CONG_EVT:
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_CONG_EVT\n");

        int ret;
        if (len_buf_spp > 300)
        {
            ret = esp_spp_write(param->srv_open.handle, 300, buf_spp_data);
        }
        else
        {
            ret = esp_spp_write(param->srv_open.handle, len_buf_spp, buf_spp_data);
            uint8_t stop_sendLTC[2];
            stop_sendLTC[0] = SEND_COMP;
            // sprintf(stop_sendLTC, SEND_COMP);
            esp_spp_write(param->srv_open.handle, 1, stop_sendLTC);
        }
        // pthread_cond_signal(&thread_spp_writer.cond2);

        break;
    case ESP_SPP_WRITE_EVT:
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_WRITE_EVT\n");
        // if (!len_buf_spp)
        // {
        //     struct timespec now;
        //     gettimeofday( &now, NULL );
        //     now.tv_sec += 5;
        //     pthread_mutex_lock(&thread_spp_writer.mutex);
        //     pthread_cond_timedwait(&thread_spp_writer.cond, &thread_spp_writer.mutex, &now);
        //     pthread_mutex_unlock(&thread_spp_writer.mutex);
        // }

        // ESP_LOGI(BT_SPP_TAG, "writing data...\n");
        // uint8_t buf[300];
        // int len;

        // if (len_buf_spp > 300)
        // {
        //     ESP_LOGI(BT_SPP_TAG, "A");
        //     len = 300;
        //     memcpy(buf, buf_spp_data, len);
        //     ret = esp_spp_write(param->srv_open.handle, len, buf_spp_data);
        // }
        // else if (len_buf_spp > 0)
        // {
        //     ESP_LOGI(BT_SPP_TAG, "B");
        //     len = len_buf_spp;
        //     memcpy(buf, buf_spp_data, len);
        //     ret = esp_spp_write(param->srv_open.handle, len, buf_spp_data);

        //     // sprintf(stop_sendLTC, SEND_COMP);
        // }
        // else
        // {
        //     ESP_LOGI(BT_SPP_TAG, "C");
        //     len = 0;
        //     uint8_t stop_sendLTC[2];
        //     stop_sendLTC[0] = SEND_COMP;
        //     ret = esp_spp_write(param->srv_open.handle, 1, stop_sendLTC);
        // }
        // ESP_LOGI(BT_SPP_TAG, "D");

        // if (!param->write.cong)
        // {
        //     // ret = esp_spp_write(param->srv_open.handle, len, buf_spp_data);
        //     // see if we transmit again or we drop in case of error
        //     if (ret != ESP_OK)
        //     {
        //         ESP_LOGE(BT_SPP_TAG, "%s spp write failed: %s\n", __func__, esp_err_to_name(ret));
        //     }
        //     len_buf_spp -= len;
        //     // possibly another access to the same data by the encoder thread concurently
        //     memmove(buf_spp_data, buf_spp_data + len, len_buf_spp);
        // }
        ESP_LOGI(BT_SPP_TAG, "signal");
        pthread_cond_signal(&thread_spp_writer.cond2);

        ESP_LOGI(BT_SPP_TAG, "len_buf_spp = %d", len_buf_spp);
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_SRV_OPEN_EVT\n");
        break;
    default:
        break;
    }
}

void bt_spp_init(void)
{
    esp_err_t ret;
    /* Register spp callback */
    if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK)
    {
        ESP_LOGE(BT_SPP_TAG, "spp register callback failed: %s\n", __func__);
        return;
    }

    /* initialize SPP module */
    if ((ret = esp_spp_init(ESP_SPP_MODE_CB)) != ESP_OK)
    {
        ESP_LOGE(BT_SPP_TAG, "spp init failed: %s\n", __func__);
        return;
    }
}

void app_main(void)
{
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    /* Initialize and Enable the controller */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK)
    {
        ESP_LOGE(GAP_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK)
    {
        ESP_LOGE(GAP_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    /* Initialize and Enable the host */
    if ((ret = esp_bluedroid_init()) != ESP_OK)
    {
        ESP_LOGE(GAP_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK)
    {
        ESP_LOGE(GAP_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    /* Initialize the LTC encoder */
    if ((ret = init_encoder()) != ESP_OK)
    {
        ESP_LOGE(GAP_TAG, "%s initialize LTC Encoder failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    /* Initialize the Serial Bluetooth Protocol */
    bt_spp_init();

    /* Initialize and enable the CLI */
    register_sender_callback(start_sendLTC);
    start_cli();
}
