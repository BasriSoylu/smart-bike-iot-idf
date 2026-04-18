#pragma once

/*================================================================
 * AKTIF VERSIYON
 *================================================================*/
#define YAZILIM_VERSIYON_MAJOR  1
#define YAZILIM_VERSIYON_MINOR  5
#define YAZILIM_VERSIYON_PATCH  0
#define YAZILIM_VERSIYON        "1.5.0"

/*================================================================
 * VERSIYON GECMISI - en yeni ustte
 *================================================================*/

/*******************************************************************
 * Versiyon       : v1.5.0
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *=================================================================
* Yazar           : Hasan Basri SOYLU
* Tarih           : 19.04.2026
*==================================================================
* Aciklama        : OTA component eklendi. ota_kontrol +
*                   ota_guncelle API'si Tasarim 2 (struct + iki
*                   asamali) ile implement edildi. cJSON ile
*                   version.json parse, mbedtls MD5 ile chunk bazli
*                   hash dogrulama, esp_ota_* API ile dual-bank
*                   flash yazma. sim800c HTTPINIT oncesi defansif
*                   HTTPTERM eklendi, BEKLE_120_SN (binary download
*                   icin) tanimlandi. FIRMWARE_VERSION makrosu
*                   YAZILIM_VERSIYON olarak yeniden adlandirildi.
*                   Main task stack 3584 -> 8192.
*******************************************************************/

/*******************************************************************
 * Versiyon       : v1.4.0
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *=================================================================
* Yazar           : Hasan Basri SOYLU
* Tarih           : 16.04.2026
*==================================================================
* Aciklama        : sim800c HTTP binary indirme API'si eklendi.
*                   sim800c_http_open / sim800c_http_read /
*                   sim800c_http_close fonksiyonlari implement edildi.
*                   reader_task'e sim_binary_modu flag'i eklendi,
*                   binary okuma sirasinda task yield ediyor.
*                   http_open_adimlari ve http_read_adimlari inner
*                   helper + wrapper pattern ile yazildi. URC parse
*                   sscanf ile yapildi (+HTTPACTION / +HTTPREAD).
*******************************************************************/

/*******************************************************************
 * Versiyon       : v1.3.0
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *=================================================================
* Yazar           : Hasan Basri SOYLU
* Tarih           : 16.04.2026
*==================================================================
* Aciklama        : sim800c mimarisi yeniden tasarlandi. cmd_wait
*                   "beklenen" parametresi eklendi, process_line
*                   sadeleştirildi. sim800c_gprs_disconnect(),
*                   sim800c_http_get_json() implement edildi.
*                   Thread safety (portMUX), BEKLE_x_SN makrolari,
*                   AT_BEARER_SET_CONTYPE, AT_GPRS_DETACH eklendi.
*******************************************************************/

/*******************************************************************
 * Versiyon       : v1.2.1
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *=================================================================
* Yazar           : Hasan Basri SOYLU
* Tarih           : 15.04.2026
*==================================================================
* Aciklama        : sim800c_gprs_connect() implement edildi.
*                   GPRS baglanti akisi: AT+CGATT + AT+SAPBR
*                   serisi (close/type/apn/start/query).
*                   Atik yorum temizlendi. Eksikler not alindi:
*                   APN Kconfig, IP parsing, disconnect/http/mqtt.
*******************************************************************/

/*******************************************************************
 * Versiyon       : v1.2.0
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *=================================================================
* Yazar           : Hasan Basri SOYLU
* Tarih           : 13.04.2026
*==================================================================
* Aciklama        : sim800c component eklendi. Dependency Injection
*                   mimarisi ile AT komut surucu yazildi. FreeRTOS
*                   reader task, state machine ve overflow korumasi
*                   implement edildi.
*******************************************************************/

/****************************************************************
 * Versiyon       : v1.1.1
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *===============================================================
 * Yazar         : Hasan Basri SOYLU
 * Tarih         : 08.04.2026
 *===============================================================
 * Aciklama      : Versiyon blogu sadece .c dosyalarinda olur
 *                 kurali belirlendi. uart.h ve uart_types.h
 *                 dosyalarindan baslık blogu kaldirildi.
 ****************************************************************/

/****************************************************************
 * Versiyon       : v1.1.0
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *===============================================================
 * Yazar         : Hasan Basri SOYLU
 * Tarih         : 08.04.2026
 *===============================================================
 * Aciklama      : UART sub-modulu eklendi. Platform bagimsiz
 *                 mimari kuruldu. uart_baslat, uart_durdur,
 *                 uart_gonder, uart_oku, uart_temizle ve
 *                 uart_baud_degistir fonksiyonlari implement
 *                 edildi. SIM800C modulu bir sonraki adimda.
 ****************************************************************/

/****************************************************************
 * Versiyon       : v1.0.0
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *===============================================================
 * Yazar         : Hasan Basri SOYLU
 * Tarih         : 29.03.2026
 *===============================================================
 * Aciklama      : Ilk calisan surum. SIM800C GSM modulu ile
 *                 GPRS uzerinden OTA firmware guncelleme
 *                 basariyla gerceklestirildi.
 ****************************************************************/
