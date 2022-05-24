#ifndef ENCODE_H
#define ENCODE_H

#include <ltc.h>
#include <sys/time.h>
#include <string.h>

#include "esp_spp_api.h"
#include "esp_log.h"

/* encoder parameters */
#define SAMPLE_RATE 48000
#define FPS 25

/* logs tags */
#define BT_ENC_TAG "ENC"
#define BT_DEC_TAG "DEC"

/* data code to advert the beginning of LTC receiving */
#define START_SEND_LTC 0x01
#define SEND_COMP 0x02
#define REQUEST_LTC_CODE 3

/* struct used to store timestamps in order to compute the latency */
typedef struct 
{
    struct timeval time_recvd_request;      /* see if it is needed */
    struct timeval time_gen_LTC;
    struct timeval time_send_LTC;
} latency_master_t;

static latency_master_t latency; 

/* timeCodeGen methods */
/* Generate LTC thanks to the internal clock, LTC are written at parameters' adresses  */
int genTimeCode(uint8_t *data, int *len_data);

#endif