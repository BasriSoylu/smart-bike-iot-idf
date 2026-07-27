#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"
#include "uart.h"
#include "sim800c.h"
#include "ota.h"
#include "versiyon.h"
#include "mqtt.h"
#include "cJSON.h"
#include "driver/gpio.h"

static const char *TAG = "MAIN";
#define LOG_DUVAR()   ESP_LOGI(TAG, "══════════════════════════════════════════════════════════════════════════════════════════════")

/* ────────────────────── Cihaz Kimligi ────────────────────── */
#define CIHAZ_ID_U32   (1U)   /* backend DB ID - BOLD-2026-0001 sasi no'nun sonuyla eslesiyor (varsayim) */

/* ────────────────────── SIM800C UART fiziksel baglanti ────────────────────── */
#define SIM800C_UART_PORT                                        (UART_NUM_2 )
#define SIM800C_TX_PIN                                           (GPIO_NUM_17)
#define SIM800C_RX_PIN                                           (GPIO_NUM_16)
#define SIM800C_PWRKEY_PIN                                       (GPIO_NUM_18)
#define SIM800C_RX_BUF_SIZE                                      (   32768   )
#define SIM800C_TX_BUF_SIZE                                      (   1024    )

#define MQTT_HATA_CALLBACK_HARD_RESET_ESIGI                      (     2     )

static volatile uint8_t       yazilim_kontrol_flag_u8 = 0U;
static          uart_handle_t g_sim_uart                  ;

/* ─────────────── sim800c_io_t wrapper fonksiyon prototipleri ───────────────── */
static void sim_send      (const uint8_t *data, size_t len                     );
static int  sim_read      (      uint8_t *buf , size_t len, uint32_t timeout_ms);
static void sim_log       (const char    *msg                                  );
static void sim_set_baud  (      uint32_t baud                                 );
static void sim_flush     (void                                                );
static void sim_pwrkey_set(      uint8_t d_seviye_u8                           );

/* ────────────── mqtt_transport_t wrapper fonksiyon prototipleri ─────────────── */
static int  mqtt_tcp_open_wrapper (const char    *host, uint16_t port                       );
static int  mqtt_tcp_close_wrapper(void                                                     );
static int  mqtt_send_wrapper     (const uint8_t *data, size_t len                          );
static int  mqtt_recv_wrapper     (      uint8_t *buf , size_t max_len, uint32_t timeout_ms );
static void mqtt_log_wrapper      (const char    *msg                                       );
static void mqtt_hata_callback    (void                                                     );

/* ──────────────────── main fonksiyonlarinin prototipleri ────────────────── */
static void cevresel_ayarla         ();
static void cevresel_baslat         ();
static void yazilim_versiyon_kontrol(uint8_t d_komut_u8                                     );
static void yazilim_kontrol_handler (const uint8_t *p_payload_u8, uint16_t d_payload_len_u16);

void app_main(void)
{
    uint32_t d_sayac_u32      = 0U;
    char     json_payload[128]    ;
    int      json_len             ;

    cevresel_ayarla(); 
    cevresel_baslat();

    ESP_LOGI(TAG, "Firmware v%s basliyor...", YAZILIM_VERSIYON);

    while(true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        d_sayac_u32++;

        if ( 0U != yazilim_kontrol_flag_u8 )
        {
            uint8_t d_komut_u8      = yazilim_kontrol_flag_u8;
            yazilim_kontrol_flag_u8 = 0U;

            LOG_DUVAR();
            yazilim_versiyon_kontrol(d_komut_u8);
            LOG_DUVAR();
        }

        if ( 0U == (d_sayac_u32 % 30U) )
        {
            /* GPS: donanimsal GNSS entegrasyonu henuz yok, gecici sabit deger (dummy) */
            json_len = snprintf(json_payload, sizeof(json_payload),
                                "{\"Cihaz_ID\":%u,\"Yukseklik\":%.1f,\"Enlem\":%.4f,\"Boylam\":%.4f}",
                                CIHAZ_ID_U32, 42.0, 41.0082, 28.9784);
            LOG_DUVAR();
            ESP_LOGI(TAG, "JSON (%d byte): %s", json_len, json_payload);

            if ( MQTT_OK == mqtt_publish("BOLD-2026-0001/gps", (const uint8_t *)json_payload, (uint16_t)json_len, 1U) )
            {
                ESP_LOGI(TAG, ">>> PUBLISH Gonderildi <<<");
            }
            else
            {
                ESP_LOGE(TAG, "PUBLISH basarisiz!");
            }
            LOG_DUVAR();

            json_len = snprintf(json_payload, sizeof(json_payload),
                                "{\"Cihaz_ID\":%u,\"versiyon\":\"%s\"}",
                                CIHAZ_ID_U32, YAZILIM_VERSIYON);
            LOG_DUVAR();
            ESP_LOGI(TAG, "JSON (%d byte): %s", json_len, json_payload);

            if ( MQTT_OK == mqtt_publish("BOLD-2026-0001/durum", (const uint8_t *)json_payload, (uint16_t)json_len, 1U) )
            {
                ESP_LOGI(TAG, ">>> PUBLISH Gonderildi <<<");
            }
            else
            {
                ESP_LOGE(TAG, "PUBLISH basarisiz!");
            }
            LOG_DUVAR();
        }
    }
}


/* ───────────────────────── main fonksiyonlari  ───────────────────── */
static void cevresel_ayarla()
{
    gpio_config_t sim808_reset_pin_st = 
    {
        .pin_bit_mask = (1ULL << SIM800C_PWRKEY_PIN),
        .mode         = GPIO_MODE_OUTPUT            ,
        .intr_type    = GPIO_INTR_DISABLE           ,
        .pull_up_en   = GPIO_PULLUP_ENABLE          ,
    };
    ESP_ERROR_CHECK(gpio_config(&sim808_reset_pin_st));
    gpio_set_level(SIM800C_PWRKEY_PIN, 1);

    /* Ilk olarak UART yoksa soft reset yersin.*/
    uart_cfg_t uart_konfigurasyonu_st =
    {
        .port_num      = SIM800C_UART_PORT        ,
        .tx_pin        = SIM800C_TX_PIN           ,
        .rx_pin        = SIM800C_RX_PIN           ,
        .baud_rate     = SIM800C_HEDEF_BAUD_RATE  ,
        .rx_buf_size   = SIM800C_RX_BUF_SIZE      ,
        .tx_buf_size   = SIM800C_TX_BUF_SIZE      ,
        .veri_bitleri  = UART_DATA_8_BITS         ,
        .parity        = UART_PARITY_DISABLE      ,
        .stop_bitleri  = UART_STOP_BITS_1         ,
        .akis_kontrolu = UART_HW_FLOWCTRL_DISABLE ,
    };
    g_sim_uart = uart_baslat(&uart_konfigurasyonu_st);

    /* Simdi sim800c UART ayyarlandiktan sonra */
    static sim800c_io_t fp_sim800c_fonksiyonlar_st =
    {
        .send       = sim_send      ,
        .read       = sim_read      ,
        .log        = sim_log       ,
        .set_baud   = sim_set_baud  ,
        .flush      = sim_flush     ,
        .pwrkey_set = sim_pwrkey_set,
    };
    sim800c_init(&fp_sim800c_fonksiyonlar_st);
    
    /* En son mqtt sim800c den sonra baslatilmasi grektigi icin*/
    static const mqtt_transport_t fp_mqtt_transport_st =
    {
        .tcp_open      = mqtt_tcp_open_wrapper ,
        .tcp_close     = mqtt_tcp_close_wrapper,
        .send          = mqtt_send_wrapper     ,
        .receive       = mqtt_recv_wrapper     ,
        .log           = mqtt_log_wrapper      ,
        .hata_callback = mqtt_hata_callback    ,
    };
    mqtt_init(&fp_mqtt_transport_st);
}


static void cevresel_baslat()
{
    if ( 0 != sim800c_baslat() )
    {
        ESP_LOGE(TAG, "SIM800C modulu baslatilamadi!");
        return;
    }

    if ( 0 != sim800c_gprs_connect() )
    {
        ESP_LOGE(TAG, "GPRS baglantisi basarisiz!");
        return;
    }

    LOG_DUVAR();

    static const mqtt_config_t mqtt_konfig_st =
    {
        .p_host_ch            = "dualino.com"                 ,
        .d_port_u16           = 1883U                         ,
        .p_client_id_ch       = "BOLD-2026-0001"              ,
        .d_keep_alive_sec_u16 = 120U                          ,
        .p_will_topic_ch      = NULL                          ,   /* LWT ilk teste girmiyor */
        .p_username_ch        = "BOLD-2026-0001"              ,
        .p_password_ch        = "F5bPyQcHw2USgKmCI9s3b17j"    ,
    };
    if( MQTT_OK == mqtt_start(&mqtt_konfig_st) )       /* baglanti sureci baslatildi (henuz baglanmis olmayabilir) */
    {
        if( MQTT_OK != mqtt_subscribe("BOLD-2026-0001/komut", 1U, yazilim_kontrol_handler) )
        {
            ESP_LOGE(TAG, "MQTT subscribe basarisiz!");
        }
    }
    LOG_DUVAR();
}

/* ─────────────── OTA yazilim guncelleme fonksiyonlari ─────────────── */
static void yazilim_versiyon_kontrol(uint8_t d_komut_u8)
{
    ota_firmware_bilgi_t firmware_bilgi_st;
    ota_sonuc_t          ota_sonuc        ;

    switch ( d_komut_u8 )
    {
        case 1U:
        {
            ESP_LOGI(TAG, "Komut 1: Versiyon kontrolu istenmedi, devam.");
            break;
        }
        case 2U:
        {
            ESP_LOGI(TAG, "Komut 2: Versiyon kontrol ediliyor...");
            ota_sonuc = ota_kontrol(&firmware_bilgi_st);

            if ( OTA_OK == ota_sonuc )
            {
                ESP_LOGI(TAG, "Yeni firmware var, guncellenecek...");
                ota_sonuc = ota_guncelle(&firmware_bilgi_st);
                if ( OTA_OK != ota_sonuc )
                {
                    ESP_LOGE(TAG, "Guncelleme basarisiz, kod=%d", ota_sonuc);
                }
            }
            else if ( OTA_GUNCEL == ota_sonuc )
            {
                ESP_LOGI(TAG, "Firmware zaten guncel.");
            }
            else
            {
                ESP_LOGE(TAG, "OTA kontrol hatasi, kod=%d", ota_sonuc);
            }
            break;
        }
        case 3U:
        {
            ESP_LOGI(TAG, "Komut 3: Zorla guncelleme (versiyon kontrolu atlandi)...");
            ota_sonuc = ota_kontrol(&firmware_bilgi_st);
            
            if ( (OTA_OK == ota_sonuc) || (OTA_GUNCEL == ota_sonuc) )
            {
                ESP_LOGI(TAG, "Sunucudaki firmware yukleniyor...");
                ota_sonuc = ota_guncelle(&firmware_bilgi_st);
                if ( OTA_OK != ota_sonuc )
                {
                    ESP_LOGE(TAG, "Zorla guncelleme basarisiz, kod=%d", ota_sonuc);
                }
            }
            else
            {
                ESP_LOGE(TAG, "Sunucu bilgisi alinamadi, kod=%d", ota_sonuc);
            }
            break;
        }
        default:
        {
            ESP_LOGW(TAG, "Bilinmeyen komut: %u", (unsigned)d_komut_u8);
            break;
        }
    }
}
static void yazilim_kontrol_handler(const uint8_t *p_payload_u8, uint16_t d_payload_len_u16)
{
    cJSON *p_json_st    = NULL;
    cJSON *p_kontrol_st = NULL;

    LOG_DUVAR();
    ESP_LOGI(TAG, ">>> MQTT MESAJ GELDI <<<");
    ESP_LOGI(TAG, "Topic   : hbs_smart_bike_2026_xyz123/komut");
    ESP_LOGI(TAG, "Payload : %.*s", (int)d_payload_len_u16, (const char *)p_payload_u8);

    p_json_st = cJSON_ParseWithLength((const char *)p_payload_u8, d_payload_len_u16);

    if ( NULL != p_json_st )
    {
        p_kontrol_st = cJSON_GetObjectItem(p_json_st, "YAZILIM_KONTROL");

        if ( (NULL != p_kontrol_st) && (true == cJSON_IsNumber(p_kontrol_st)) )
        {
            uint8_t d_komut_u8 = (uint8_t)p_kontrol_st->valueint;

            if ( (1U <= d_komut_u8) && (3U >= d_komut_u8) )
            {
                yazilim_kontrol_flag_u8 = d_komut_u8;
                ESP_LOGI(TAG, "YAZILIM_KONTROL komutu alindi: %u", (unsigned)d_komut_u8);
            }
            else
            {
                ESP_LOGW(TAG, "Gecersiz YAZILIM_KONTROL degeri: %u (beklenen: 1-3)", (unsigned)d_komut_u8);
            }
        }
        else
        {
            ESP_LOGW(TAG, "YAZILIM_KONTROL alani yok veya sayi degil");
        }
    }
    else
    {
        ESP_LOGE(TAG, "JSON parse edilemedi!");
    }
    LOG_DUVAR();
    cJSON_Delete(p_json_st);
}

/* ─────────────── sim800c_io_t wrapper fonksiyonlari ─────────────── */
static void sim_send(const uint8_t *data, size_t len)
{
    uart_gonder(g_sim_uart, data, (int)len);
}
static int  sim_read(uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    return uart_oku(g_sim_uart, buf, (int)len, timeout_ms);
}
static void sim_log(const char *msg)
{
    ESP_LOGI(TAG, "%s", msg);
}
static void sim_set_baud(uint32_t baud)
{
    uart_baud_degistir(g_sim_uart, (int)baud);
}
static void sim_flush(void)
{
    uart_temizle(g_sim_uart);
}
static void sim_pwrkey_set(uint8_t d_seviye_u8)
{
    gpio_set_level(SIM800C_PWRKEY_PIN, d_seviye_u8);
}
/* ─────────── mqtt_transport_t wrapper fonksiyonlari ─────────── */
static int mqtt_tcp_open_wrapper(const char *host, uint16_t port)
{
    return sim800c_tcp_open(host, (int)port);
}
static int mqtt_tcp_close_wrapper(void)
{
    return sim800c_tcp_close();
}
static int mqtt_send_wrapper(const uint8_t *data, size_t len)
{
    return sim800c_tcp_send(data, (int)len);
}
static int mqtt_recv_wrapper(uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    return sim800c_tcp_recv(buf, (int)max_len, timeout_ms);
}
static void mqtt_log_wrapper(const char *msg)
{
    ESP_LOGI(TAG, "%s", msg);
}
static void mqtt_hata_callback(void)
{
    static uint8_t s_ardisik_gprs_hata_u8 = 0;

    ESP_LOGW(TAG, "MQTT ardisik baglanma hatasi — GPRS kontrol ediliyor...");

    if ( 0 == sim800c_gprs_connect() )
    {
        s_ardisik_gprs_hata_u8 = 0U;
    }
    else
    {
        s_ardisik_gprs_hata_u8++;

        if ( s_ardisik_gprs_hata_u8 >= MQTT_HATA_CALLBACK_HARD_RESET_ESIGI )
        {
            ESP_LOGE(TAG, "GPRS onarimi ardisik basarisiz — PWRKEY ile donanimsal reset atiliyor...");
            sim800c_hard_reset();
            s_ardisik_gprs_hata_u8 = 0U;

            if ( 0 != sim800c_baslat() )
            {
                ESP_LOGE(TAG, "Donanimsal reset sonrasi SIM800C baslatilamadi!");
                return;
            }

            sim800c_gprs_connect();
        }
    }
}


















