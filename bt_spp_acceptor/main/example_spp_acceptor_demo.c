/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*----------------------------------------------------------------
Voir ce qu'on fait de ce coté l'autre attend des services
----------------------------------------------------------------*/

/****************************************************************************
 *
 * This file is for Classic Bluetooth device and service discovery Demo.
 *
 ****************************************************************************/

#include <stdint.h>
#include <string.h>
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

#include "timeCodeGen.h"
#include <ltc.h>

// #include "BluetoothSerial/src/BluetoothSerial.h"

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
static uint8_t buf_spp_data[3*1920];



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

static char *bda2str(esp_bd_addr_t bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18)
    {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

static char *uuid2str(esp_bt_uuid_t *uuid, char *str, size_t size)
{
    if (uuid == NULL || str == NULL)
    {
        return NULL;
    }

    if (uuid->len == 2 && size >= 5)
    {
        sprintf(str, "%04x", uuid->uuid.uuid16);
    }
    else if (uuid->len == 4 && size >= 9)
    {
        sprintf(str, "%08x", uuid->uuid.uuid32);
    }
    else if (uuid->len == 16 && size >= 37)
    {
        uint8_t *p = uuid->uuid.uuid128;
        sprintf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                p[15], p[14], p[13], p[12], p[11], p[10], p[9], p[8],
                p[7], p[6], p[5], p[4], p[3], p[2], p[1], p[0]);
    }
    else
    {
        return NULL;
    }

    return str;
}

static bool get_name_from_eir(uint8_t *eir, uint8_t *bdname, uint8_t *bdname_len)
{
    uint8_t *rmt_bdname = NULL;
    uint8_t rmt_bdname_len = 0;

    if (!eir)
    {
        return false;
    }

    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname)
    {
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }

    if (rmt_bdname)
    {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN)
        {
            rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
        }

        if (bdname)
        {
            memcpy(bdname, rmt_bdname, rmt_bdname_len);
            bdname[rmt_bdname_len] = '\0';
        }
        if (bdname_len)
        {
            *bdname_len = rmt_bdname_len;
        }
        return true;
    }

    return false;
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
        // TODO: think about the security setting mask (NONE for now)
        // Slave: passive device, wait for a connection request
        // TODO: think if we need to choose another channel (0 == any)
        esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, SPP_SERVER_NAME);
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
#if (SPP_SHOW_MODE == SPP_SHOW_DATA)
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_DATA_IND_EVT len=%d handle=%d\n",
                 param->data_ind.len, param->data_ind.handle);
        esp_log_buffer_hex("", param->data_ind.data, param->data_ind.len);
        // mettre le write ici pour envoyer les TimeCode
        // test
        // int len = 20;
        // c buf[len];
        // sprintf(buf, ")

        // esp_spp_write(param->srv_open.handle, len, buf);
        // esp_spp_write(param->srv_open.handle, SPP_DATA_LEN, spp_data);
        genTimeCode(param, buf_spp_data, &len_buf_spp);
        
#else
        gettimeofday(&time_new, NULL);
        data_num += param->data_ind.len;
        if (time_new.tv_sec - time_old.tv_sec >= 3)
        {
            print_speed();
        }     
#endif
        break;
    case ESP_SPP_CONG_EVT:
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_CONG_EVT\n");
        break;
    case ESP_SPP_WRITE_EVT:
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_WRITE_EVT\n");
        // TODO: verify if the data in the buffer is the data we want
        if(len_buf_spp)
        {
            // ESP_LOGI(BT_SPP_TAG, "writing data...\n");
            // int ret;
            // uint8_t buf[ESP_SPP_MAX_MTU];
            // int len;
            // if(len_buf_spp > ESP_SPP_MAX_MTU) len = ESP_SPP_MAX_MTU;
            // else len = len_buf_spp;
            // memcpy(buf, buf_spp_data, len);
            // // ret = esp_spp_write(param->srv_open.handle, len, buf_spp_data);
            // // see if we transmit again or we drop in case of error
            // // if(ret != ESP_OK)
            // // {
            // //     ESP_LOGE(BT_SPP_TAG, "%s spp write failed: %s\n", __func__, esp_err_to_name(ret));
            // // }
            // // need to free buf?
            // len_buf_spp -= len;
            // memmove(buf_spp_data + len, buf_spp_data, len_buf_spp);           
        }
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(BT_SPP_TAG, "ESP_SPP_SRV_OPEN_EVT\n");
        // don't know the utility
        // gettimeofday(&time_old, NULL);
        break;
    default:
        break;
    }
}

void bt_spp_init(void)
{
    esp_err_t ret;
    // Register spp callback
    if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK)
    {
        ESP_LOGE(BT_SPP_TAG, "spp register callback failed: %s\n", __func__);
        return;
    }

    // Maybe use Virtual File System Mode (VFS) to handle Linear Timecode R/W action 
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

    // bt_app_gap_start_up();
    bt_spp_init();

    // if un clinet connected
    // maybe change bda (change to remote bda)

    // u_int32_t t_poll = 10; // revoir ça, unite 0,625ms
    // if ((ret = esp_bt_gap_set_qos(m_dev_info.bda, t_poll)) != ESP_OK)
    // {
    //     ESP_LOGE(GAP_TAG, "%s set bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
    //     return;
    // }
}
