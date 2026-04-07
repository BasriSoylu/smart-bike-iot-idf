#pragma once

/*================================================================
 * AKTIF VERSIYON
 *================================================================*/
#define YAZILIM_VERSIYON_MAJOR  1
#define YAZILIM_VERSIYON_MINOR  1
#define YAZILIM_VERSIYON_PATCH  0
#define YAZILIM_VERSIYON        "v1.1.0"

/*================================================================
 * VERSIYON GECMISI - en yeni ustte
 *================================================================*/

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
