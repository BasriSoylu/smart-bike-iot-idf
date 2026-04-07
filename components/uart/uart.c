/****************************************************************
 * Versiyon       : v1.1.0
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *===============================================================
 * Yazar         : Hasan Basri SOYLU
 * Tarih         : 08.04.2026
 *===============================================================
 * Aciklama      : UART sub-modulu implementasyonu. ESP-IDF
 *                 driver/uart.h uzerine platform bagimsiz
 *                 sarmalayici fonksiyonlar saglar.
 ****************************************************************/

#include "uart.h"
#include "esp_log.h"

static const char *TAG = "UART";

uart_handle_t uart_baslat(const uart_cfg_t *yapilandirma)
{
    uart_config_t uart_cfg = {
        .baud_rate  = yapilandirma->baud_rate,
        .data_bits  = yapilandirma->veri_bitleri,
        .parity     = yapilandirma->parity,
        .stop_bits  = yapilandirma->stop_bitleri,
        .flow_ctrl  = yapilandirma->akis_kontrolu,
    };

    uart_param_config  (yapilandirma->port_num, &uart_cfg);
    uart_set_pin       (yapilandirma->port_num, yapilandirma->tx_pin, yapilandirma->rx_pin,
                        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(yapilandirma->port_num, yapilandirma->rx_buf_size,
                        yapilandirma->tx_buf_size, 0, NULL, 0);

    ESP_LOGI(TAG, "UART%d baslatildi - %d baud", yapilandirma->port_num, yapilandirma->baud_rate);
    return yapilandirma->port_num;
}

void uart_durdur(uart_handle_t tanitici)
{
    uart_driver_delete(tanitici);
    ESP_LOGI(TAG, "UART%d durduruldu", tanitici);
}

int uart_gonder(uart_handle_t tanitici, const uint8_t *veri, int uzunluk)
{
    return uart_write_bytes(tanitici, (const char *)veri, uzunluk);
}

int uart_oku(uart_handle_t tanitici, uint8_t *tampon, int maksimum_uzunluk, uint32_t zaman_asimi_ms)
{
    return uart_read_bytes(tanitici, tampon, maksimum_uzunluk, pdMS_TO_TICKS(zaman_asimi_ms));
}

void uart_temizle(uart_handle_t tanitici)
{
    uart_flush_input(tanitici);
}

void uart_baud_degistir(uart_handle_t tanitici, int yeni_baud)
{
    uart_set_baudrate(tanitici, yeni_baud);
    ESP_LOGI(TAG, "UART%d baud rate degistirildi: %d", tanitici, yeni_baud);
}
