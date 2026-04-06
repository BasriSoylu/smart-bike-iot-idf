#pragma once

#include <stdint.h>
#include "driver/uart.h"

// UART yapılandırma sabitleri
#define GSM_UART_PORT       (UART_NUM_2)
#define GSM_TX_PIN          (    17    )
#define GSM_RX_PIN          (    16    )
#define GSM_DEFAULT_BAUD    (   9600   )
#define GSM_TARGET_BAUD     (  115200  )

// Fonksiyon prototipleri
void gsm_init(void);
int gsm_send_command(const char *cmd, char *response, uint32_t timeout_ms);
int gsm_wait_response(const char *expected, char *response, uint32_t timeout_ms);
int gsm_gprs_connect(void);
int gsm_gprs_disconnect(void);
void gsm_send_raw(const uint8_t *data, int len);

// HTTP fonksiyonları
int gsm_http_get_json(const char *url, char *out_buf, int out_max);
int gsm_http_get_binary(const char *url, int offset, uint8_t *out_buf, int chunk_size, int *out_len, int *total_len);