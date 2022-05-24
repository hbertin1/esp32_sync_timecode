#ifndef DECODE_H
#define DECODE_H


#include "sys/time.h"

/* data code to advert the beginning of LTC receiveing */
#define START_RECV_LTC   1
#define RECV_COMP        2
#define REQUEST_LTC_CODE 3

typedef struct
{
    uint8_t *buffer;
    int length;
} data_received_t;

typedef struct
{
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} thread_decoder_t;

typedef struct 
{
    struct timeval tv_startRCVTime;
    struct timeval tv_stopRCVTime;
} RCVTime;

static thread_decoder_t thread_decoder;

static RCVTime recv_time;

typedef struct 
{
    struct timeval time_send_request;
    struct timeval time_receiveLTC;
    uint16_t encoding_duration;
    uint16_t decoding_duration;
    int sync_number;
    struct timeval sum_time;
} latency_slave_t;

static latency_slave_t latency; 

#endif