#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gsm.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"

static const char *TAG = "GSM";

void gsm_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate  = GSM_TARGET_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };

    char response[1024];

    uart_param_config(GSM_UART_PORT, &uart_cfg);
    uart_set_pin(GSM_UART_PORT, GSM_TX_PIN, GSM_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(GSM_UART_PORT, 32768, 1024, 0, NULL, 0);

    vTaskDelay(pdMS_TO_TICKS(1000));

    gsm_send_command("AT", response, 1000);
    if(strstr(response, "OK") != NULL)
    {
        ESP_LOGI(TAG, "GSM hazir - %d baud", GSM_TARGET_BAUD);
        return;
    }
    
    ESP_LOGI(TAG, "115200'de cevap yok, 9600 deneniyor...");                             
    uart_set_baudrate(GSM_UART_PORT, GSM_DEFAULT_BAUD);                                  
    vTaskDelay(pdMS_TO_TICKS(100));
    gsm_send_command("AT", response, 1000);
    if(strstr(response, "OK") == NULL)
    {
        ESP_LOGE(TAG, "GSM modulu bulunamadi!"); 
        return;
    }
    ESP_LOGI(TAG, "GSM Modülü 9600 baud ile bulundu, 115200'e gecis yapiliyor...");
    gsm_send_command("AT+IPR=115200", response, 1000);
    uart_set_baudrate(GSM_UART_PORT, GSM_TARGET_BAUD);
    vTaskDelay(pdMS_TO_TICKS(100));

    gsm_send_command("AT", response, 1000);
    if(strstr(response, "OK")!= NULL)   ESP_LOGI(TAG, "GSM Hazır - %d baud", GSM_TARGET_BAUD);
    else                                ESP_LOGE(TAG, "GSM 11520 Geçiş Başarısız!");

    ESP_LOGI(TAG, "GSM UART baslatildi - 115200 baud");
}

int gsm_send_command(const char *cmd, char *response, uint32_t timeout_ms)
{
    uart_flush_input(GSM_UART_PORT);
    uart_write_bytes(GSM_UART_PORT, cmd, strlen(cmd));
    uart_write_bytes(GSM_UART_PORT, "\r\n", 2);

    int len = uart_read_bytes(GSM_UART_PORT, (uint8_t*)response, 1023, pdMS_TO_TICKS(timeout_ms));
    response[len] = '\0';
    return len;
}

int gsm_wait_response(const char *expected, char *response, uint32_t timeout_ms)
{
    int total_len = 0;
    uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;

    while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start) < timeout_ms)
    {
        int len = uart_read_bytes(GSM_UART_PORT, (uint8_t*)(response + total_len),
                                1023 - total_len, pdMS_TO_TICKS(500));
        if (len > 0)
        {
            total_len += len;
            response[total_len] = '\0';
            if (strstr(response, expected) != NULL)
                return total_len;
        }
    }
    return total_len;
}

// HTTP GET ile küçük JSON/metin yanıtı al (version.json için)
int gsm_http_get_json(const char *url, char *out_buf, int out_max)
{
    char cmd[256];
    char response[1024];

    // HTTP stack başlat
    gsm_send_command("AT+HTTPTERM", response, 2000);
    vTaskDelay(pdMS_TO_TICKS(500));

    gsm_send_command("AT+HTTPINIT", response, 3000);
    if (strstr(response, "OK") == NULL) {
        ESP_LOGE("GSM", "HTTPINIT basarisiz");
        return -1;
    }

    gsm_send_command("AT+HTTPPARA=\"CID\",1", response, 3000);

    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    gsm_send_command(cmd, response, 3000);

    // GET isteği gönder — OK ve +HTTPACTION aynı pencerede gelebilir,
    // gsm_wait_response ile direkt +HTTPACTION bekle
    uart_flush_input(GSM_UART_PORT);
    uart_write_bytes(GSM_UART_PORT, "AT+HTTPACTION=0\r\n", 17);
    int ret = gsm_wait_response("+HTTPACTION", response, 15000);
    if (ret <= 0 || strstr(response, ",200,") == NULL) {
        ESP_LOGE("GSM", "HTTP GET basarisiz: %s", response);
        gsm_send_command("AT+HTTPTERM", response, 2000);
        return -1;
    }

    // Yanıt boyutunu parse et
    int body_len = 0;
    char *comma2 = strrchr(response, ',');
    if (comma2) sscanf(comma2 + 1, "%d", &body_len);

    if (body_len <= 0 || body_len >= out_max) {
        ESP_LOGE("GSM", "HTTP body boyutu hatali: %d", body_len);
        gsm_send_command("AT+HTTPTERM", response, 2000);
        return -1;
    }

    // Yanıtı oku
    snprintf(cmd, sizeof(cmd), "AT+HTTPREAD=0,%d", body_len);
    uart_flush_input(GSM_UART_PORT);
    uart_write_bytes(GSM_UART_PORT, cmd, strlen(cmd));
    uart_write_bytes(GSM_UART_PORT, "\r\n", 2);

    static uint8_t raw[4096];
    int raw_len = uart_read_bytes(GSM_UART_PORT, raw, sizeof(raw) - 1, pdMS_TO_TICKS(5000));
    if (raw_len <= 0) {
        gsm_send_command("AT+HTTPTERM", response, 2000);
        return -1;
    }
    raw[raw_len] = '\0';

    // +HTTPREAD:<len>\r\n<data> formatından veriyi çıkar
    char *start = strstr((char*)raw, "+HTTPREAD:");
    if (start == NULL) {
        gsm_send_command("AT+HTTPTERM", response, 2000);
        return -1;
    }
    start = strchr(start, '\n');
    if (start == NULL) {
        gsm_send_command("AT+HTTPTERM", response, 2000);
        return -1;
    }
    start++; // '\n' sonrası veri başlar

    int copy_len = body_len < (out_max - 1) ? body_len : (out_max - 1);
    memcpy(out_buf, start, copy_len);
    out_buf[copy_len] = '\0';

    gsm_send_command("AT+HTTPTERM", response, 2000);
    return copy_len;
}

// HTTP GET ile binary chunk oku (firmware.bin için)
// offset: kaçıncı byte'tan başla
// out_len: bu çağrıda kaç byte okundu
// total_len: dosyanın toplam boyutu (ilk çağrıda doldurulur)
int gsm_http_get_binary(const char *url, int offset, uint8_t *out_buf, int chunk_size, int *out_len, int *total_len)
{
    char cmd[256];
    char response[512];

    if (offset == 0) {
        // İlk chunk: HTTP bağlantısını kur
        gsm_send_command("AT+HTTPTERM", response, 2000);
        vTaskDelay(pdMS_TO_TICKS(2000));
        gsm_send_command("AT+HTTPINIT", response, 3000);
        gsm_send_command("AT+HTTPPARA=\"CID\",1", response, 3000);
        snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
        gsm_send_command(cmd, response, 3000);
        uart_flush_input(GSM_UART_PORT);
        uart_write_bytes(GSM_UART_PORT, "AT+HTTPACTION=0\r\n", 17);
        ESP_LOGI("GSM", "Firmware indiriliyor, bekleniyor (max 90s)...");
        int ret = gsm_wait_response("+HTTPACTION", response, 90000);
        if (ret <= 0 || strstr(response, ",200,") == NULL) {
            ESP_LOGE("GSM", "Firmware HTTP GET basarisiz: [%s]", response);
            gsm_send_command("AT+HTTPTERM", response, 2000);
            return -1;
        }

        char *comma2 = strrchr(response, ',');
        if (comma2) sscanf(comma2 + 1, "%d", total_len);
        ESP_LOGI("GSM", "Firmware boyutu: %d byte", *total_len);
    }

    // Chunk oku
    snprintf(cmd, sizeof(cmd), "AT+HTTPREAD=%d,%d", offset, chunk_size);
    uart_flush_input(GSM_UART_PORT);
    uart_write_bytes(GSM_UART_PORT, cmd, strlen(cmd));
    uart_write_bytes(GSM_UART_PORT, "\r\n", 2);

    // Binary veriyi raw oku
    static uint8_t raw[16384 + 128];
    int raw_len = uart_read_bytes(GSM_UART_PORT, raw, sizeof(raw) - 1, pdMS_TO_TICKS(3000));
    if (raw_len <= 0) {
        ESP_LOGE("GSM", "HTTPREAD cevap gelmedi");
        return -1;
    }

    // +HTTPREAD:<len>\r\n<binary data> bul
    for (int i = 0; i < raw_len - 12; i++) {
        if (memcmp(raw + i, "+HTTPREAD:", 10) == 0) {
            int actual = 0;
            sscanf((char*)(raw + i + 10), "%d", &actual);
            for (int j = i + 10; j < raw_len - 1; j++) {
                if (raw[j] == '\r' && raw[j+1] == '\n') {
                    int copy = actual < chunk_size ? actual : chunk_size;
                    memcpy(out_buf, raw + j + 2, copy);
                    *out_len = copy;
                    return 0;
                }
            }
        }
    }

    ESP_LOGE("GSM", "HTTPREAD parse hatasi");
    return -1;
}

int gsm_gprs_connect(void)
{
    char response[1024];
    gsm_send_command("AT+CGATT=1", response, 5000);
    gsm_send_command("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", response, 3000);
    gsm_send_command("AT+SAPBR=3,1,\"APN\",\"internet\"", response, 3000);
    gsm_send_command("AT+SAPBR=1,1", response, 15000);
    gsm_send_command("AT+SAPBR=2,1", response, 3000);
    if (strstr(response, "0.0.0.0") != NULL) return -1;
    ESP_LOGI("GSM", "GPRS baglandi: %s", response);
    return 0;
}

int gsm_gprs_disconnect(void)
{
    char response[1024];
    gsm_send_command("AT+SAPBR=0,1", response, 5000);
    return 0;
}

void gsm_send_raw(const uint8_t *data, int len)
{
    uart_write_bytes(GSM_UART_PORT, data, len);
}