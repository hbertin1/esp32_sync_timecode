/****************************************************************************
 *
 * This file is for Classic Bluetooth device and service discovery Demo.
 *
 ****************************************************************************/

#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
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

#include "ltc.h"
#include "timeCodeDec.h"
#include "bt_devices_methods.h"
#include "console.h"

#define GAP_TAG "GAP"
#define SPP_TAG "SPP"
#define DEC_TAG "DEC"

#define FPS 25

esp_bd_addr_t peer_bd_addr = {0};
static uint8_t peer_bdname_len;
static char peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
static const char remote_spp_name[] = "DEV_B";
static const esp_bt_inq_mode_t inq_mode = ESP_BT_INQ_MODE_GENERAL_INQUIRY;
static const uint8_t inq_len = 30;
static const uint8_t inq_num_rsps = 0;

#if (SPP_SHOW_MODE == SPP_SHOW_DATA)
#define SPP_DATA_LEN 20
#else
#define SPP_DATA_LEN ESP_SPP_MAX_MTU
#endif
static uint8_t spp_data[SPP_DATA_LEN];

static uint8_t buffer_data_received[3 * 1920];
int len_buffer_data_received = 0;

void set_time(SMPTETimecode stime) 
{   
    struct tm secs;

    secs.tm_year = 0;
    secs.tm_mon = 0;
    secs.tm_mday = 0;
    secs.tm_hour = 0;
    secs.tm_min = stime.mins;
    secs.tm_sec = stime.secs;

    struct timeval tv;
    tv.tv_sec = stime.mins*60 + stime.secs;
    
    double msec = stime.frame * 1000.0 / FPS;
    tv.tv_usec = msec * 1000;

    struct timeval now;
    gettimeofday(&now, NULL);
    latency.decoding_duration = now.tv_usec-latency.time_receiveLTC.tv_usec;

    latency.sync_number++;
    struct timeval v_m;
    v_m.tv_usec = (latency.sum_time.tv_usec + ((now.tv_usec - latency.time_send_request.tv_usec)/2))/latency.sync_number;
    v_m.tv_usec += latency.encoding_duration + latency.decoding_duration;

    struct timeval t_new;
    t_new.tv_sec = tv.tv_sec;
    t_new.tv_usec = tv.tv_usec + v_m.tv_usec;

    ESP_LOGI(SPP_TAG, "v_m.tv_usec = %ld", v_m.tv_usec);

    settimeofday(&t_new, DST_NONE);
}

void *decoder(void *p_data)
{
    int apv = 1920;
    ltcsnd_sample_t sound[1024];
    size_t n;
    long int total = 0;

    LTCDecoder *decoder;
    LTCFrameExt frame;

    while (1)
    {
        decoder = ltc_decoder_create(apv, 32);
        int index = 0;

        pthread_mutex_lock(&thread_decoder.mutex);
        ESP_LOGI(DEC_TAG, "aquire lock\n");
        pthread_cond_wait(&thread_decoder.cond, &thread_decoder.mutex);
        ESP_LOGI(DEC_TAG, "start thread\n");

        do
        {
            if (index + 1024 < len_buffer_data_received)
                n = 1024;
            else
                n = len_buffer_data_received - index;
            memcpy(sound, &(buffer_data_received[index]), n);
            index += n;
            ESP_LOGD(DEC_TAG, "n = %zu, buffer = %s", n, buffer_data_received);
            ltc_decoder_write(decoder, sound, n, total);
            while (ltc_decoder_read(decoder, &frame))
            {
                SMPTETimecode stime;
                ltc_frame_to_time(&stime, &frame.ltc, 1);

                // ESP_LOGI(DEC_TAG, "%04d-%02d-%02d %s ",
                //          ((stime.years < 67) ? 2000 + stime.years : 1900 + stime.years),
                //          stime.months,
                //          stime.days,
                //          stime.timezone);

                // ESP_LOGI(DEC_TAG, "%02d:%02d:%02d%c%02d | %8lld %8lld%s\n",
                //          stime.hours,
                //          stime.mins,
                //          stime.secs,
                //          (frame.ltc.dfbit) ? '.' : ':',
                //          stime.frame,
                //          frame.off_start,
                //          frame.off_end,
                //          frame.reverse ? "  R" : "");

                printf("mins: %02d, secs: %02d, frame: %02d\n", stime.mins, stime.secs, stime.frame);
                set_time(stime);
            }
            total += n;
        } while (n);
        ltc_decoder_free(decoder);
        len_buffer_data_received = 0;
        pthread_mutex_unlock(&thread_decoder.mutex);
        ESP_LOGI(DEC_TAG, "release lock\n");
    }
}

int init_decoder(uint8_t *buffer_data_received, int *len_buffer_data_received)
{
    int ret;

    thread_decoder.mutex = PTHREAD_MUTEX_INITIALIZER,
    thread_decoder.cond = PTHREAD_COND_INITIALIZER,

    pthread_mutex_init(&thread_decoder.mutex, NULL);
    pthread_cond_init(&thread_decoder.cond, NULL);

    ret = pthread_create(&thread_decoder.thread, NULL, &decoder, NULL);

    return ret;
}

static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event)
    {
    /* Once the broadcast information is found, ESP_BT_GAP_DISC_RES_EVT is triggered. Then
       the device is connected. */
    case ESP_BT_GAP_DISC_RES_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_DISC_RES_EVT");
        esp_log_buffer_hex(SPP_TAG, param->disc_res.bda, ESP_BD_ADDR_LEN);
        for (int i = 0; i < param->disc_res.num_prop; i++)
        {
            if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_EIR && get_name_from_eir(param->disc_res.prop[i].val, peer_bdname, &peer_bdname_len))
            {
                ESP_LOGI(SPP_TAG, "remote dev name %s", peer_bdname);
                if (strlen(remote_spp_name) == peer_bdname_len && strncmp(peer_bdname, remote_spp_name, peer_bdname_len) == 0)
                {
                    memcpy(peer_bd_addr, param->disc_res.bda, ESP_BD_ADDR_LEN);
                    esp_spp_start_discovery(peer_bd_addr);
                    esp_bt_gap_cancel_discovery();
                }
            }
        }
        break;
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_DISC_STATE_CHANGED_EVT");
        break;
    case ESP_BT_GAP_RMT_SRVCS_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_RMT_SRVCS_EVT");
        break;
    case ESP_BT_GAP_RMT_SRVC_REC_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_RMT_SRVC_REC_EVT");
        break;
    default:
        break;
    }
}

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    esp_err_t ret;

    switch (event)
    {
    case ESP_SPP_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        /* Inquiry duration: 1.28 seconds unit
            inq_num_rsps: Number of responses before the inquiry is halted (0 means unlimited) */
        esp_bt_gap_start_discovery(inq_mode, inq_len, inq_num_rsps);
        ESP_LOGI(SPP_TAG, "discovery started...");
        latency.sync_number = 0;
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT status=%d scn_num=%d", param->disc_comp.status, param->disc_comp.scn_num);
        if (param->disc_comp.status == ESP_SPP_SUCCESS)
        {
            /* security mask: Maybe change to add security later */
            ret = esp_spp_connect(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_MASTER, param->disc_comp.scn[0], peer_bd_addr);
            ESP_LOGI(SPP_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        }
        break;
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
        esp_spp_write(param->srv_open.handle, SPP_DATA_LEN, spp_data);
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT");
        break;
    case ESP_SPP_START_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT");
        break;
    case ESP_SPP_CL_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
        break;
    /* receive data event */
    case ESP_SPP_DATA_IND_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len=%d handle=%d\n",
                 param->data_ind.len, param->data_ind.handle);
        if (param->data_ind.len < 50)
        {
            ESP_LOGI(SPP_TAG, "Data : %d\n",*(param->data_ind.data));
            esp_log_buffer_char("", param->data_ind.data, param->data_ind.len);
            if(*(param->data_ind.data) == START_RECV_LTC)
            {
                /* store the first LTC receiving time */
                gettimeofday(&latency.time_receiveLTC, NULL);
            }
            else if(param->data_ind.data[0] == RECV_COMP) 
            {
                pthread_cond_signal(&thread_decoder.cond);

                /* get the encoding duration sent */
                uint16_t encoding_duration = 0;
                encoding_duration = param->data_ind.data[1];
                encoding_duration = encoding_duration + (param->data_ind.data[2]<<8);
                latency.encoding_duration = encoding_duration;
                ESP_LOGI(SPP_TAG, "encoding_duration = %u", latency.encoding_duration);
            }
        }
        else
        {
            /* add reiceived data to the buffer in order to rebuild an entire encoded frame */
            memcpy(&(buffer_data_received[len_buffer_data_received]), param->data_ind.data, param->data_ind.len);
            len_buffer_data_received += param->data_ind.len;
        }
        break;
    case ESP_SPP_CONG_EVT:
#if (SPP_SHOW_MODE == SPP_SHOW_DATA)
        ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT cong=%d", param->cong.cong);
#endif
        if (param->cong.cong == 0)
        {
            esp_spp_write(param->cong.handle, SPP_DATA_LEN, spp_data);
        }
        break;
    case ESP_SPP_WRITE_EVT:
#if (SPP_SHOW_MODE == SPP_SHOW_DATA)
        ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT len=%d cong=%d", param->write.len, param->write.cong);
        esp_log_buffer_hex("", spp_data, SPP_DATA_LEN);
#else
        gettimeofday(&time_new, NULL);
        data_num += param->write.len;
        if (time_new.tv_sec - time_old.tv_sec >= 3)
        {
            print_speed();
        }
#endif
        if (param->write.cong == 0)
        {
            // esp_spp_write(param->write.handle, SPP_DATA_LEN, spp_data);
        }
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT");
        break;
    default:
        break;
    }
}

void bt_spp_init(void)
{
    char *dev_name = "DEV_A";
    esp_bt_dev_set_device_name(dev_name);
    esp_err_t ret;
    if ((ret = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s gap register failed\n", __func__);
        return;
    }
    /* Once the SPP callback has been registered, ESP_SPP_INIT_EVT is triggered. */
    if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s spp register failed\n", __func__);
        return;
    }

    if ((ret = esp_spp_init(ESP_SPP_MODE_CB)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s spp register failed\n", __func__);
        return;
    }
}

void app_main(void)
{
    /* Initialize NVS ??? it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

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

    init_decoder(buffer_data_received, len_buffer_data_received);

    bt_spp_init();

    start_cli();
}
