/****************************************************************
 * Versiyon       : v1.1.0
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *===============================================================
 * Yazar         : Hasan Basri SOYLU
 * Tarih         : 08.04.2026
 *===============================================================
 * Aciklama      : UART sub-modulu public API. Platform bagimsiz
 *                 mimari - uart_handle_t ile birden fazla UART
 *                 portu yonetilebilir.
 ****************************************************************/

#pragma once

#include "uart_types.h"

/**
 * @brief UART surucusunu baslatir, yapilandirir ve kullanima hazir hale getirir.
 *
 * @param yapilandirma Port numarasi, pin numaralari ve baud rate bilgilerini iceren yapi
 * @return Basarili olursa gecerli UART tanitici, hata durumunda -1
 */
uart_handle_t uart_baslat(const uart_cfg_t *yapilandirma);

/**
 * @brief UART surucusunu durdurur ve ayrilan kaynaklari serbest birakar.
 *
 * @param tanitici uart_baslat() ile alinan UART tanitici
 */
void uart_durdur(uart_handle_t tanitici);

/**
 * @brief Belirtilen bayt dizisini UART hatti uzerinden gonderir.
 *
 * @param tanitici  uart_baslat() ile alinan UART tanitici
 * @param veri      Gonderilecek verinin baslangic adresi
 * @param uzunluk   Gonderilecek bayt sayisi
 * @return Gonderilen bayt sayisi, hata durumunda -1
 */
int uart_gonder(uart_handle_t tanitici, const uint8_t *veri, int uzunluk);

/**
 * @brief UART hattindan veri okur, zaman asimi suresince yeni veri bekler.
 *
 * @param tanitici         uart_baslat() ile alinan UART tanitici
 * @param tampon           Okunan verinin yazilacagi bellek adresi
 * @param maksimum_uzunluk Tampona yazilabilecek maksimum bayt sayisi
 * @param zaman_asimi_ms   Veri beklenecek maksimum sure (milisaniye)
 * @return Okunan bayt sayisi, zaman asiminda 0, hata durumunda -1
 */
int uart_oku(uart_handle_t tanitici, uint8_t *tampon, int maksimum_uzunluk, uint32_t zaman_asimi_ms);

/**
 * @brief Giris tamponundaki okunmamis tum veriyi siler.
 *
 * @param tanitici uart_baslat() ile alinan UART tanitici
 */
void uart_temizle(uart_handle_t tanitici);

/**
 * @brief Calisma zamaninda UART baud rate'ini degistirir.
 *
 * @param tanitici    uart_baslat() ile alinan UART tanitici
 * @param yeni_baud   Yeni baud rate degeri
 */
void uart_baud_degistir(uart_handle_t tanitici, int yeni_baud);
