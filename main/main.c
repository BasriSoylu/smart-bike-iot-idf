#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "gsm.h"
#include "ota.h"
#include "config.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Firmware v%s basliyor...", FIRMWARE_VERSION);

    // 1. GSM başlat
    gsm_init();
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 2. GPRS bağlan
    if (gsm_gprs_connect() != 0) {
        ESP_LOGE(TAG, "GPRS baglantisi basarisiz!");
        return;
    }

    // 3. OTA kontrol et — yeni firmware varsa indir, yaz, yeniden başlat
    ota_check_and_update();

    // 4. Normal çalışma döngüsü
    ESP_LOGI(TAG, "Normal calisma basliyor...");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Calisiyorum... v%s", FIRMWARE_VERSION);
    }
}
