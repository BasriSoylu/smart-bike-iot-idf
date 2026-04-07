/****************************************************************
 * Versiyon       : v1.1.0
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *===============================================================
 * Yazar         : Hasan Basri SOYLU
 * Tarih         : 08.04.2026
 *===============================================================
 * Aciklama      : UART sub-modülü public API. Platform
 *                 bagimsiz mimari — uart_handle_t ile birden
 *                 fazla UART portu yönetilebilir.
 ****************************************************************/

#pragma once

#include "uart_types.h"

/**
 * @brief UART sürücüsünü başlatır, yapılandırır ve kullanıma hazır hale getirir.
 *
 * @param yapilandirma Port numarası, pin numaraları ve baud rate bilgilerini içeren yapı
 * @return Başarılı olursa geçerli UART tanıtıcısı, hata durumunda -1
 */
uart_handle_t uart_baslat(const uart_cfg_t *yapilandirma);

/**
 * @brief UART sürücüsünü durdurur ve ayrılan kaynakları serbest bırakır.
 *
 * @param tanitici uart_baslat() ile alınan UART tanıtıcısı
 */
void uart_durdur(uart_handle_t tanitici);

/**
 * @brief Belirtilen bayt dizisini UART hattı üzerinden gönderir.
 *
 * @param tanitici  uart_baslat() ile alınan UART tanıtıcısı
 * @param veri      Gönderilecek verinin başlangıç adresi
 * @param uzunluk   Gönderilecek bayt sayısı
 * @return Gönderilen bayt sayısı, hata durumunda -1
 */
int uart_gonder(uart_handle_t tanitici, const uint8_t *veri, int uzunluk);

/**
 * @brief UART hattından veri okur, zaman aşımı süresince yeni veri bekler.
 *
 * @param tanitici         uart_baslat() ile alınan UART tanıtıcısı
 * @param tampon           Okunan verinin yazılacağı bellek adresi
 * @param maksimum_uzunluk Tampona yazılabilecek maksimum bayt sayısı
 * @param zaman_asimi_ms   Veri beklenecek maksimum süre (milisaniye)
 * @return Okunan bayt sayısı, zaman aşımında 0, hata durumunda -1
 */
int uart_oku(uart_handle_t tanitici, uint8_t *tampon, int maksimum_uzunluk, uint32_t zaman_asimi_ms);

/**
 * @brief Giriş tamponundaki okunmamış tüm veriyi siler.
 *
 * @param tanitici uart_baslat() ile alınan UART tanıtıcısı
 */
void uart_temizle(uart_handle_t tanitici);

/**
 * @brief Çalışma zamanında UART baud rate'ini değiştirir.
 *
 * @param tanitici    uart_baslat() ile alınan UART tanıtıcısı
 * @param yeni_baud   Yeni baud rate değeri
 */
void uart_baud_degistir(uart_handle_t tanitici, int yeni_baud);
