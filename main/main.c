#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"
#include "uart.h"
#include "sim800c.h"
#include "ota.h"
#include "versiyon.h"

static const char *TAG = "MAIN";

/* ────────────────────── SIM800C UART fiziksel baglanti ────────────────────── */
#define SIM800C_UART_PORT     (UART_NUM_2)
#define SIM800C_TX_PIN        (    17    )
#define SIM800C_RX_PIN        (    16    )
#define SIM800C_RX_BUF_SIZE   (   32768  )
#define SIM800C_TX_BUF_SIZE   (   1024   )


static uart_handle_t g_sim_uart;

/* ────────── sim800c_io_t wrapper fonksiyon prototipleri ──────────────────── */
static void sim_send    (const uint8_t *data, size_t len                     );
static int  sim_read    (      uint8_t *buf , size_t len, uint32_t timeout_ms);
static void sim_log     (const char    *msg                                  );
static void sim_set_baud(      uint32_t baud                                 );
static void sim_flush    (void                                                );

/* ──────────────────── main fonksiyonlarinin prototipleri ────────────────── */
static void cevresel_ayarla         ();
static void cevresel_baslat         ();
static void yazilim_versiyon_kontrol();
static void tcp_test                ();
static void mqtt_test               ();


void app_main(void)
{
    cevresel_ayarla();

    cevresel_baslat();

    //yazilim_versiyon_kontrol();

    //tcp_test();
    mqtt_test();

    ESP_LOGI(TAG, "Firmware v%s basliyor...", YAZILIM_VERSIYON);

    while ( 1 )
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
    ESP_LOGI(TAG, "===== MQTT TESTI =====");

    /* 1. TCP baglan */
    if ( 0 !=  sim800c_tcp_open("broker.hivemq.com", 80) )
    {
        ESP_LOGE(TAG, "TCP open basarisiz");
        return;
    }
    ESP_LOGI(TAG, "TCP OK, CONNECT gonderiliyor...");

    /* 2. MQTT CONNECT paketi */
    static const uint8_t mqtt_connect[] = {
        0x10 ,
        0x16 ,
        0x00 , 
        0x04 , 
        'M'  , 
        'Q'  , 
        'T'  , 
        'T'  ,
        0x04 ,
        0x02 ,
        0x00, 
        0x3C,
        0x00, 
        0x0A,
        'e' ,
        's' ,
        'p' ,
        '3' ,
        '2' ,
        '_' ,
        'b' ,
        'o' ,
        'l' ,
        'd'
    };

    if ( 0 != sim800c_tcp_send(mqtt_connect, sizeof(mqtt_connect)) )
    {
        ESP_LOGE(TAG, "CONNECT gonderilemedi");
        sim800c_tcp_close();
        return;
    }

    /* 3. CONNACK bekle - 4 byte: 0x20 0x02 0x00 0x00 */
    uint8_t connack[4];
    int n = sim800c_tcp_recv(connack, sizeof(connack), 5000);

    if ( n < 4 )
    {
        ESP_LOGE(TAG, "CONNACK gelmedi (n=%d)", n);
        sim800c_tcp_close();
        return;
    }

    ESP_LOGI(TAG, "CONNACK: %02X %02X %02X %02X",
             connack[0], connack[1], connack[2], connack[3]);

    if ( (0x20 == connack[0]) && (0x00 == connack[3]) )
    {
        ESP_LOGI(TAG, ">>> MQTT BROKER BAGLANTISI BASARILI <<<");
    }
    else
    {
        ESP_LOGE(TAG, "CONNACK ret kodu: 0x%02X", connack[3]);
    }

    sim800c_tcp_close();
    ESP_LOGI(TAG, "===== MQTT TESTI BITTI =====");
}

/* ─────────────── sim800c_io_t wrapper fonksiyonlari ─────────────── */
static void sim_send(const uint8_t *data, size_t len)
{
    uart_gonder(g_sim_uart, data, (int)len);
}
static int sim_read(uint8_t *buf, size_t len, uint32_t timeout_ms)
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