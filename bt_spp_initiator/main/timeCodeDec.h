#ifndef DECODE_H
#define DECODE_H


#include "sys/time.h"

/* data code to advert the beginning of LTC receiveing */
#define START_RECV_LTC   1
#define RECV_COMP        2
#define REQUEST_LTC_CODE 3

/* struct to associate a buffer pointer to its length */
typedef struct
{
    uint8_t *buffer;
    int length;
} data_received_t;

/* thread to handle decoder actions */
typedef struct
{
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} thread_decoder_t;

/* time references to compute the latency between the timeCode 
Request and the clock synchronization */
static thread_decoder_t thread_decoder;

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