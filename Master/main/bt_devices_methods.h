#ifndef _BT_DEVICES_METHODS
#define _BT_DEVICES_METHODS

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

char *bda2str(esp_bd_addr_t bda, char *str, size_t size);

char *uuid2str(esp_bt_uuid_t *uuid, char *str, size_t size);

bool get_name_from_eir(uint8_t *eir, char *bdname, uint8_t *bdname_len);


#endif