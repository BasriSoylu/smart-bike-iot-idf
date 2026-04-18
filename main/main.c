#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"
#include "uart.h"
#include "sim800c.h"

static const char *TAG = "MAIN";

/* ── SIM800C UART fiziksel baglanti ──────────────────────────────── */
#define SIM800C_UART_PORT     (UART_NUM_2)
#define SIM800C_TX_PIN        (    17    )
#define SIM800C_RX_PIN        (    16    )
#define SIM800C_RX_BUF_SIZE   (  32768   )
#define SIM800C_TX_BUF_SIZE   (  1024    )

/* sim800c driver UART handle'i callback'lerden erismek icin globalde tutulur */
static uart_handle_t g_sim_uart;


/* ── sim800c_io_t wrapper fonksiyonlari ──────────────────────────── */
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


void app_main(void)
{
    static sim800c_io_t io =
    {
        .send     = sim_send    ,
        .read     = sim_read    ,
        .log      = sim_log     ,
        .set_baud = sim_set_baud,
        .flush    = sim_flush   ,
    };

    uart_cfg_t uart_cfg =
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

    char json_buf[512];
    int  okunan;

    ESP_LOGI(TAG, "Firmware v%s basliyor...", FIRMWARE_VERSION);

    /* 1) UART2'yi baslat (TX=17, RX=16, 115200 baud) */
    g_sim_uart = uart_baslat(&uart_cfg);

    /* 2) sim800c driver'ini baslat (reader task ayaga kalkar) */
    sim800c_init(&io);

    /* 3) Modul ile iletisim testi + baud senkronizasyonu */
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

    /* 5) Smoke test: version.json'i cek */
    okunan = sim800c_http_get_json(OTA_VERSION_URL, json_buf, sizeof(json_buf));
    if ( 0 < okunan )
    {
        ESP_LOGI(TAG, "version.json (%d byte): %s", okunan, json_buf);
    }
    else
    {
        ESP_LOGE(TAG, "version.json alinamadi!");
    }

    ESP_LOGI(TAG, "Normal calisma basliyor...");
    while ( 1 )
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Calisiyorum... v%s", FIRMWARE_VERSION);
    }
}
