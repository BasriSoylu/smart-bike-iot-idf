#include <string.h>
#include "ota.h"
#include "sim800c.h"
#include "cJSON.h"
#include "config.h"
#include "versiyon.h"
#include "esp_log.h"
#include "esp_ota_ops.h"    
#include "esp_partition.h"  
#include "esp_system.h"     
#include "esp_err.h"        
#include "mbedtls/md5.h"    
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char *TAG = "OTA";

#define OTA_CHUNK_SIZE (1024)

ota_sonuc_t ota_kontrol(ota_firmware_bilgi_t *p_bilgi_st)
{
    char json_buf[512]     = {0} ;
    int  okunan        =  0  ;  

    if( NULL == p_bilgi_st )
    {
        ESP_LOGE(TAG, "Bilgi struct pointer'i NULL!");
        return OTA_HATA_NULL;
    }

    okunan = sim800c_http_get_json(OTA_VERSION_URL, json_buf, sizeof(json_buf));
    if( okunan <= 0 )
    {
        ESP_LOGE(TAG, "version.json alinamadi!");
        return OTA_HATA_HTTP;
    }

    cJSON *p_json = cJSON_Parse(json_buf);
    if( NULL == p_json )
    {
        ESP_LOGE(TAG, "JSON parse edilemedi!");
        return OTA_HATA_JSON;
    }

    cJSON *p_surum = cJSON_GetObjectItem(p_json, "version");
    cJSON *p_md5   = cJSON_GetObjectItem(p_json, "md5"    );
    cJSON *p_url   = cJSON_GetObjectItem(p_json, "url"    );
    cJSON *p_boyut = cJSON_GetObjectItem(p_json, "size"   );

    if( !cJSON_IsString(p_surum) || !cJSON_IsString(p_md5) || !cJSON_IsString(p_url) || !cJSON_IsNumber(p_boyut) )
    {
        ESP_LOGE(TAG, "JSON field hatasi!");
        cJSON_Delete(p_json);
        return OTA_HATA_JSON;
    }

    strncpy(p_bilgi_st->surum, p_surum->valuestring, sizeof(p_bilgi_st->surum) - 1);
    p_bilgi_st->surum[sizeof(p_bilgi_st->surum) - 1] = '\0';

    strncpy(p_bilgi_st->md5,   p_md5  ->valuestring, sizeof(p_bilgi_st->md5)   - 1);
    p_bilgi_st->md5[sizeof(p_bilgi_st->md5) - 1] = '\0';
    
    strncpy(p_bilgi_st->url,   p_url  ->valuestring, sizeof(p_bilgi_st->url)   - 1);
    p_bilgi_st->url[sizeof(p_bilgi_st->url) - 1] = '\0';

    p_bilgi_st->boyut = (uint32_t)p_boyut->valuedouble;

    cJSON_Delete(p_json);

    if( 0 == strcmp(p_bilgi_st->surum, YAZILIM_VERSIYON) )
    {
        ESP_LOGI(TAG, "Mevcut surum guncel: %s", p_bilgi_st->surum);
        return OTA_GUNCEL;
    }

    ESP_LOGI(TAG, "Yeni Firmwire bulundu: %s -> %s", YAZILIM_VERSIYON, p_bilgi_st->surum);

    return OTA_OK;
}

ota_sonuc_t ota_guncelle(const ota_firmware_bilgi_t *p_bilgi_st)
{
    if( NULL == p_bilgi_st )
    {
        ESP_LOGE(TAG, "Bilgi struct pointer'i NULL!");
        return OTA_HATA_NULL;
    }

    const esp_partition_t *p_hedef_partition = esp_ota_get_next_update_partition(NULL);
    if( NULL == p_hedef_partition )
    {
        ESP_LOGE(TAG, "Hedef partition bulunamadi!");
        return OTA_HATA_FLASH;
    }

    ESP_LOGI(   TAG, "Hedef partition: %s (offset=0x%lx, size=%lu)",
                p_hedef_partition->label                           ,
                p_hedef_partition->address                         ,
                p_hedef_partition->size                             );

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(p_hedef_partition, p_bilgi_st->boyut, &ota_handle);
    if ( ESP_OK != err )
    {
        ESP_LOGE(TAG, "esp_ota_begin hatasi: %s", esp_err_to_name(err));
        return OTA_HATA_FLASH;
    }

    mbedtls_md5_context md5_ctx;
    mbedtls_md5_init(&md5_ctx);
    mbedtls_md5_starts(&md5_ctx);

    int toplam_boyut = 0;
    if( 0 != sim800c_http_open(OTA_FIRMWARE_URL, &toplam_boyut) )
    {
        ESP_LOGE(TAG, "HTTP open basarisiz!");
        esp_ota_abort(ota_handle);
        mbedtls_md5_free(&md5_ctx);
        return OTA_HATA_HTTP;
    }

    ESP_LOGI(TAG, "HTTP acildi, sunucu boyutu: %d byte (JSON: %lu)",
    toplam_boyut, p_bilgi_st->boyut);

    if ( (int)p_bilgi_st->boyut != toplam_boyut )
    {
        ESP_LOGE(TAG, "Boyut uyumsuz! JSON=%lu server=%d", p_bilgi_st->boyut, toplam_boyut);
        sim800c_http_close();
        esp_ota_abort(ota_handle);
        mbedtls_md5_free(&md5_ctx);
        return OTA_HATA_HTTP;
    }

    uint8_t chunk_buf[OTA_CHUNK_SIZE];
    int     offset      = 0;
    int     okunan_byte = 0;
    
    while( offset < toplam_boyut)
    {
        int kalan = toplam_boyut - offset;
        int istenen = (kalan > OTA_CHUNK_SIZE) ? OTA_CHUNK_SIZE : kalan;

        if( 0 != sim800c_http_read(offset, chunk_buf, istenen, &okunan_byte) )
        {
            ESP_LOGE(TAG, "HTTP read hatasi (offset=%d)!!", offset);
            sim800c_http_close();
            esp_ota_abort(ota_handle);
            mbedtls_md5_free(&md5_ctx);
        return OTA_HATA_HTTP;
        }

        if( 0 >= okunan_byte)
        {
            ESP_LOGE(TAG, "HTTP read 0 byte dondu (offset=%d)", offset);
            sim800c_http_close();
            esp_ota_abort(ota_handle);
            mbedtls_md5_free(&md5_ctx);
        return OTA_HATA_HTTP;
        }

        mbedtls_md5_update(&md5_ctx, chunk_buf, okunan_byte);

        esp_err_t yaz_err = esp_ota_write(ota_handle, chunk_buf, okunan_byte);
        if( ESP_OK != yaz_err)
        {
            ESP_LOGE(TAG, "esp_ota_write hatasi: %s", esp_err_to_name(yaz_err));
            sim800c_http_close();
            esp_ota_abort   (ota_handle);
            mbedtls_md5_free(&md5_ctx );
        return OTA_HATA_FLASH;
        }

        offset += okunan_byte; 

        if ( (offset / 10240) != ((offset - okunan_byte) / 10240) )
        {
            ESP_LOGI(TAG, "Indirildi: %d / %d byte (%%%d)",
                    offset, toplam_boyut, (offset * 100) / toplam_boyut);
        }
        
    }

    ESP_LOGI(TAG, "Indirme tamam: %d byte", offset);

    sim800c_http_close();

    uint8_t md5_hash[16];
    mbedtls_md5_finish(&md5_ctx, md5_hash);
    mbedtls_md5_free  (&md5_ctx);

    char md5_hex[33];
    for (int i = 0; i < 16; i++ )
    {
        snprintf(&md5_hex[i * 2], 3, "%02x", md5_hash[i]);
    } 
    md5_hex[32] = '\0';

    ESP_LOGI(TAG, "Hesaplanan MD5: %s", md5_hex        );
    ESP_LOGI(TAG, "Beklenen   MD5: %s", p_bilgi_st->md5);

    if ( 0 != strcasecmp(md5_hex, p_bilgi_st->md5) )
    {
        ESP_LOGE(TAG, "MD5 uyumsuz! Indirme bozuk.");
        esp_ota_abort(ota_handle);
        return OTA_HATA_MD5;
    }

    ESP_LOGI(TAG, "MD5 dogrulandi");

    esp_err_t end_err = esp_ota_end(ota_handle);
    if ( ESP_OK != end_err )
    {
        ESP_LOGE(TAG, "esp_ota_end hatasi: %s", esp_err_to_name(end_err));
        return OTA_HATA_FLASH;
    }

    esp_err_t boot_err = esp_ota_set_boot_partition(p_hedef_partition);
    if ( ESP_OK != boot_err )
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition hatasi: %s", esp_err_to_name(boot_err));
        return OTA_HATA_FLASH;
    }

    ESP_LOGI(TAG, "OTA basarili! 3 sn sonra restart...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();

return OTA_OK;
}