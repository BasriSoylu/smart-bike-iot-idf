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

static const char *TAG = "MAIN";

/* ────────────────────── SIM800C UART fiziksel baglanti ────────────────────── */
#define SIM800C_UART_PORT     (UART_NUM_2)
#define SIM800C_TX_PIN        (    17    )
#define SIM800C_RX_PIN        (    16    )
#define SIM800C_RX_BUF_SIZE   (   32768  )
#define SIM800C_TX_BUF_SIZE   (   1024   )


static uart_handle_t g_sim_uart;

/* ─────────────── sim800c_io_t wrapper fonksiyon prototipleri ───────────────── */
static void sim_send    (const uint8_t *data, size_t len                     );
static int  sim_read    (      uint8_t *buf , size_t len, uint32_t timeout_ms);
static void sim_log     (const char    *msg                                  );
static void sim_set_baud(      uint32_t baud                                 );
static void sim_flush   (void                                                );

/* ────────────── mqtt_transport_t wrapper fonksiyon prototipleri ─────────────── */
static int  mqtt_tcp_open_wrapper (const char    *host, uint16_t port                       );
static int  mqtt_tcp_close_wrapper(void                                                     );
static int  mqtt_send_wrapper     (const uint8_t *data, size_t len                          );
static int  mqtt_recv_wrapper     (      uint8_t *buf , size_t max_len, uint32_t timeout_ms );
static void mqtt_log_wrapper      (const char    *msg                                       );

/* ──────────────────── main fonksiyonlarinin prototipleri ────────────────── */
static void cevresel_ayarla         ();
static void cevresel_baslat         ();
static void yazilim_versiyon_kontrol(uint8_t d_komut_u8);
static void mqtt_test               ();
static void mqtt_message_handler    (const char *p_topic_ch, const uint8_t *p_payload_u8, size_t d_payload_len);


/* ──────────────────── Main Struct Yapilari ────────────────── */
typedef struct
{
    uint32_t devices_id_u32 ;
    float    gps_altitute_f ;
    float    gps_latitute_f ;
    float    gps_longitute_f;
}gps_veri_paketi_t;
gps_veri_paketi_t gps_veri_paketi_st;

static volatile uint8_t yazilim_kontrol_flag_u8 = 0U; /* 0 = beklenmiyor, 1/2/3 = MQTT'den gelen komut */

void app_main(void)
{
    cevresel_ayarla();

    cevresel_baslat();

    //yazilim_versiyon_kontrol();

    mqtt_test();

    ESP_LOGI(TAG, "Firmware v%s basliyor...", YAZILIM_VERSIYON);

    while(true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Calisiyorum... v%s", YAZILIM_VERSIYON);
    }
}





/* ───────────────────────── main fonksiyonlari  ───────────────────── */
static void cevresel_ayarla()
{
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
        .send     = sim_send    ,
        .read     = sim_read    ,
        .log      = sim_log     ,
        .set_baud = sim_set_baud,
        .flush    = sim_flush   ,
    };
    sim800c_init(&fp_sim800c_fonksiyonlar_st);

    static const mqtt_transport_t fp_mqtt_transport_st =
    {
        .tcp_open  = mqtt_tcp_open_wrapper ,
        .tcp_close = mqtt_tcp_close_wrapper,
        .send      = mqtt_send_wrapper     ,
        .receive   = mqtt_recv_wrapper     ,
        .log       = mqtt_log_wrapper      ,
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

    /* 4) GPRS bearer'ini ac */
    if ( 0 != sim800c_gprs_connect() )
    {
        ESP_LOGE(TAG, "GPRS baglantisi basarisiz!");
        return;
    }
}

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
                mqtt_disconnect();                             
                ESP_LOGI(TAG, "MQTT kapatildi, OTA basliyor.");
    
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
                mqtt_disconnect();                             
                ESP_LOGI(TAG, "MQTT kapatildi, OTA basliyor."); 
                
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


static void mqtt_test()
{
    char        json_payload[128]                       ;
    int         json_len                                ;
    const char *topic = "hbs_smart_bike_2026_xyz123/gps";

    ESP_LOGI(TAG, "===== MQTT =====");

    if ( MQTT_OK != mqtt_connect("broker.hivemq.com", 1883, "esp32_bold", 60) )
    {
        ESP_LOGE(TAG, "MQTT connect basarisiz");
        return;
    }

    /* ──── Callback + subscribe + receiver baslat ──── */
    mqtt_set_message_callback(mqtt_message_handler);

    if ( MQTT_OK != mqtt_subscribe("hbs_smart_bike_2026_xyz123/komut", 0) )
    {
        ESP_LOGE(TAG, "MQTT subscribe basarisiz");
        mqtt_disconnect();
        return;
    }

    mqtt_start_receiver();
    /* ─────────────────────────────────────────────── */

    gps_veri_paketi_st.devices_id_u32  = 42        ;
    gps_veri_paketi_st.gps_altitute_f  = 120.5f    ;
    gps_veri_paketi_st.gps_latitute_f  = 41.0082f  ;
    gps_veri_paketi_st.gps_longitute_f = 28.9784f  ;

    while ( true )
    {
        gps_veri_paketi_st.gps_latitute_f  += 0.0001f;
        gps_veri_paketi_st.gps_longitute_f += 0.0001f;
        gps_veri_paketi_st.gps_altitute_f  += 0.5f   ;

        if ( 0U != yazilim_kontrol_flag_u8)
        {
            uint8_t d_komut_u8 = yazilim_kontrol_flag_u8;
            yazilim_kontrol_flag_u8 = 0U;  
            yazilim_versiyon_kontrol(d_komut_u8);
        }

        json_len = snprintf(json_payload, sizeof(json_payload),
                            "{\"Cihaz_ID\":%u,\"Yukseklik\":%.2f,\"Enlem\":%.6f,\"Boylam\":%.6f}" ,
                                            (unsigned)  gps_veri_paketi_st.devices_id_u32 ,
                                                        gps_veri_paketi_st.gps_altitute_f ,
                                                        gps_veri_paketi_st.gps_latitute_f ,
                                                        gps_veri_paketi_st.gps_longitute_f);
        ESP_LOGI(TAG, "══════════════════════════════════════════════════════════════════════════════════════════════");
        ESP_LOGI(TAG, "JSON (%d byte): %s", json_len, json_payload);

        if ( MQTT_OK != mqtt_publish(topic, (const uint8_t *)json_payload, (uint16_t)json_len) )
        {
            ESP_LOGE(TAG, "PUBLISH basarisiz, donguden cikiliyor");
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    mqtt_disconnect();

    ESP_LOGI(TAG, "===== MQTT TESTI BITTI =====");
}


/* ─────────── MQTT mesaj geldiginde tetiklenen handler ─────────── */
static void mqtt_message_handler(   const   char    *p_topic_ch   ,
                                    const   uint8_t *p_payload_u8 ,
                                            size_t   d_payload_len )
{
    cJSON *p_json_st        = NULL;
    cJSON *p_kontrol_st     = NULL;

        ESP_LOGI(TAG, "══════════════════════════════════════════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, ">>> MQTT MESAJ GELDI <<<");
    ESP_LOGI(TAG, "Topic   : %s"     , p_topic_ch);
    ESP_LOGI(TAG, "Payload : %.*s"   , (int)d_payload_len, (const char *)p_payload_u8);

    p_json_st = cJSON_ParseWithLength((const char *)p_payload_u8, d_payload_len);

    if(NULL != p_json_st)
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

        ESP_LOGI(TAG, "══════════════════════════════════════════════════════════════════════════════════════════════");

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



















