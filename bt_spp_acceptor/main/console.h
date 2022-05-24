#ifndef _CONSOLE_H
#define _CONSOLE_H

void start_cli(void);

// void setParam(esp_spp_cb_param_t *param, pthread_cond_t *cond_main);

void register_sender_callback(void (* cb) ());

#endif