#include <time.h>
#include <string.h>
#include <ltc.h>
#include <sys/time.h>
#include <pthread.h>

#include "timeCodeDec.h"
#include "esp_log.h"

#define DEC_TAG "DEC"

void * decoder(void* p_data)
{
    struct data_received_t *data = p_data;

    while(1)
    {
        pthread_mutex_lock(&thread_decoder.mutex);
        ESP_LOGI(DEC_TAG,"PAAAAS BONNNN lock\n");
        ESP_LOGI(DEC_TAG,"wait\n");
        pthread_cond_wait(&thread_decoder.cond, &thread_decoder.mutex);
        ESP_LOGI(DEC_TAG,"length data received = %d\n", ((data_received_t *) p_data)->length);
        pthread_mutex_unlock(&thread_decoder.mutex);
        ESP_LOGI(DEC_TAG,"release lock\n");
    }
}

int init_decoder(data_received_t * data_received, int * len_data_received)
{
    int ret;
    ESP_LOGI(DEC_TAG,"data length received = %d\n", data_received->length);
   
    thread_decoder.mutex = PTHREAD_MUTEX_INITIALIZER,
    thread_decoder.cond = PTHREAD_COND_INITIALIZER,

    pthread_mutex_init(&thread_decoder.mutex, NULL);
    pthread_cond_init(&thread_decoder.cond, NULL);

    ret = pthread_create(&thread_decoder.thread, NULL, &decoder, data_received);

    return ret;
}