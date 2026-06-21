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
static void yazilim_versiyon_kontrol();
static void tcp_test                ();
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


void app_main(void)
{
    cevresel_ayarla();

    cevresel_baslat();

    //yazilim_versiyon_kontrol();

    //tcp_test();
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

static void yazilim_versiyon_kontrol()
{
    ota_firmware_bilgi_t firmware_bilgi_st;
    ota_sonuc_t          ota_sonuc        ;
    
    ota_sonuc = ota_kontrol(&firmware_bilgi_st);
    
    switch ( ota_sonuc )
    {
        case OTA_OK:
        ESP_LOGI(TAG, "Yeni firmware bulundu, guncellemeye baslaniyor...");
        ota_sonuc = ota_guncelle(&firmware_bilgi_st);
        if ( OTA_OK != ota_sonuc )
        {
            ESP_LOGE(TAG, "OTA guncelleme basarisiz, kod=%d", ota_sonuc);
        }
        break;
        
        case OTA_GUNCEL:
        ESP_LOGI(TAG, "Firmware zaten guncel, devam ediliyor.");
        break;
        
        default:
        ESP_LOGE(TAG, "OTA kontrol hatasi, kod=%d", ota_sonuc);
        break;
    }
}

static void tcp_test()
{
    ESP_LOGI(TAG, "===== TCP RAW HTTP TESTI =====");

    if ( 0 != sim800c_tcp_open("ifconfig.me", 80) )
    {
        ESP_LOGE(TAG, "TCP open basarisiz");
        return;
    }
    ESP_LOGI(TAG, "TCP open OK");

    /* Basit HTTP GET - cevap olarak public IP gelmeli */
    const char *req = "GET /ip HTTP/1.0\r\nHost: ifconfig.me\r\nConnection: close\r\n\r\n";
    int req_len = (int)strlen(req);

    if ( 0 != sim800c_tcp_send((const uint8_t *)req, req_len) )
    {
        ESP_LOGE(TAG, "HTTP GET gonderim basarisiz");
        sim800c_tcp_close();
        return;
    }
    ESP_LOGI(TAG, "HTTP GET gonderildi (%d byte), cevap bekleniyor", req_len);

    /* Cevap oku - 10 sn timeout */
    uint8_t resp[256];
    int n = sim800c_tcp_recv(resp, sizeof(resp) - 1, 10000);

    if ( 0 < n )
    {
        resp[n] = '\0';
        ESP_LOGI(TAG, ">>> %d byte CEVAP GELDI <<<\n%s", n, (char*)resp);
    }
    else
    {
        ESP_LOGW(TAG, "Cevap yok (n=%d) - TCP recv timeout", n);
    }

    sim800c_tcp_close();
    ESP_LOGI(TAG, "===== TCP TESTI BITTI =====");
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
    ESP_LOGI(TAG, "══════════════════════════════════════════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, ">>> MQTT MESAJ GELDI <<<");
    ESP_LOGI(TAG, "Topic   : %s"     , p_topic_ch);
    ESP_LOGI(TAG, "Payload : %.*s"   , (int)d_payload_len, (const char *)p_payload_u8);
    ESP_LOGI(TAG, "Boyut   : %u byte", (unsigned)d_payload_len);
    ESP_LOGI(TAG, "══════════════════════════════════════════════════════════════════════════════════════════════\n");
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



















