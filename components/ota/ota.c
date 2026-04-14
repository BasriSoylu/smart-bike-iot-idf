#include "ota.h"
#include "gsm.h"
#include "config.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "OTA";

#define OTA_CHUNK_SIZE  16384

// JSON içinden "version" değerini çıkar: {"version":"1.0.1"} → "1.0.1"
static int parse_version(const char *json, char *out, int out_max)
{
    const char *key = "\"version\"";
    char *pos = strstr(json, key);
    if (pos == NULL) return -1;

    pos = strchr(pos, ':');
    if (pos == NULL) return -1;

    while (*pos == ':' || *pos == ' ' || *pos == '"') pos++;

    int i = 0;
    while (*pos != '"' && *pos != '\0' && i < out_max - 1) {
        out[i++] = *pos++;
    }
    out[i] = '\0';
    return i > 0 ? 0 : -1;
}

int ota_check_and_update(void)
{
    char json_buf[256];
    char server_version[32];

    // 1. Sunucudan version.json çek
    ESP_LOGI(TAG, "Versiyon kontrol ediliyor: %s", OTA_VERSION_URL);
    int ret = gsm_http_get_json(OTA_VERSION_URL, json_buf, sizeof(json_buf));
    if (ret < 0) {
        ESP_LOGE(TAG, "version.json alinamadi");
        return -1;
    }
    ESP_LOGI(TAG, "version.json: %s", json_buf);

    // 2. Versiyon parse et
    if (parse_version(json_buf, server_version, sizeof(server_version)) != 0) {
        ESP_LOGE(TAG, "Versiyon parse hatasi");
        return -1;
    }
    ESP_LOGI(TAG, "Sunucu: %s  Mevcut: %s", server_version, FIRMWARE_VERSION);

    // 3. Versiyon karşılaştır
    if (strcmp(server_version, FIRMWARE_VERSION) == 0) {
        ESP_LOGI(TAG, "Firmware guncel, guncelleme yok");
        return 0;
    }

    ESP_LOGI(TAG, "Yeni firmware bulundu! Indirme basliyor...");

    // 4. OTA partition hazırla (STM32'deki flash sector erase gibi)
    const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
    if (update_part == NULL) {
        ESP_LOGE(TAG, "OTA partition bulunamadi");
        return -1;
    }
    ESP_LOGI(TAG, "OTA partition: %s (0x%08lx)", update_part->label, update_part->address);

    esp_ota_handle_t ota_handle;
    if (esp_ota_begin(update_part, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin basarisiz");
        return -1;
    }

    // 5. Firmware'i chunk chunk indir ve yaz
    static uint8_t chunk_buf[OTA_CHUNK_SIZE];
    int offset = 0;
    int total_len = 0;
    int chunk_len = 0;
    int chunk_num = 0;

    while (1) {
        ret = gsm_http_get_binary(  OTA_FIRMWARE_URL, offset, chunk_buf,
                                    OTA_CHUNK_SIZE, &chunk_len, &total_len);
        if (ret != 0) {
            ESP_LOGE(TAG, "Chunk indirme hatasi (offset=%d)", offset);
            esp_ota_abort(ota_handle);
            char resp[64];
            gsm_send_command("AT+HTTPTERM", resp, 2000);
            return -1;
        }

        // Flash'a yaz (STM32'deki HAL_FLASH_Program gibi)
        if (esp_ota_write(ota_handle, chunk_buf, chunk_len) != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write hatasi (offset=%d)", offset);
            esp_ota_abort(ota_handle);
            char resp[64];
            gsm_send_command("AT+HTTPTERM", resp, 2000);
            return -1;
        }

        offset += chunk_len;
        chunk_num++;
        ESP_LOGI(TAG, "Chunk %d: %d/%d byte yazildi", chunk_num, offset, total_len);

        if (offset >= total_len) break;
    }

    // 6. HTTP kapat
    char response[128];
    gsm_send_command("AT+HTTPTERM", response, 2000);

    // 7. OTA tamamla ve boot partition'ı güncelle
    if (esp_ota_end(ota_handle) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end basarisiz (firmware bozuk olabilir)");
        return -1;
    }

    if (esp_ota_set_boot_partition(update_part) != ESP_OK) {
        ESP_LOGE(TAG, "Boot partition ayarlanamadi");
        return -1;
    }

    ESP_LOGI(TAG, "OTA tamamlandi! Yeniden baslatiliyor...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return 1; // buraya gelinmez
}
