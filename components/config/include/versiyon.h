#pragma once

/*================================================================
 * AKTIF VERSIYON
 *================================================================*/
#define YAZILIM_VERSIYON_MAJOR  1
#define YAZILIM_VERSIYON_MINOR  4
#define YAZILIM_VERSIYON_PATCH  0
#define YAZILIM_VERSIYON        "v1.4.0"

/*================================================================
 * VERSIYON GECMISI - en yeni ustte
 *================================================================*/

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
