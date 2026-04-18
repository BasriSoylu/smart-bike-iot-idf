#pragma once
#include <stdint.h>

/* ── OTA Sonuc Kodlari ──────────────────────────────────────────── */
typedef enum
{
    OTA_OK         =  0 ,   // kontrol: yeni var / guncelle: yazildi
    OTA_GUNCEL     =  1 ,   // mevcut surum guncel (sadece ota_kontrol)
    OTA_HATA_NULL  = -1 ,   // NULL pointer
    OTA_HATA_HTTP  = -2 ,   // http cekilemedi / indirilemedi
    OTA_HATA_JSON  = -3 ,   // JSON parse / field hatasi
    OTA_HATA_FLASH = -4 ,   // esp_ota_* hatasi
    OTA_HATA_MD5   = -5 ,   // md5 uyumsuz, firmware bozuk
} ota_sonuc_t;

/* ── Firmware Bilgisi (JSON'dan parse edilir) ───────────────────── */
typedef struct
{
    char     surum[16] ;    // "1.5.0"
    char     md5  [33] ;    // 32 hex + '\0'
    char     url  [256];    // firmware path (relative veya absolute)
    uint32_t boyut     ;    // byte
} ota_firmware_bilgi_t;

/* ── Public API ─────────────────────────────────────────────────── */

/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : ota_kontrol
 * Parametre     : p_bilgi_st → sunucu bilgisi dolacak struct
 * Donus Degeri  : OTA_OK        → yeni firmware var
 *                 OTA_GUNCEL    → mevcut surum guncel
 *                 OTA_HATA_*    → hata
 * Aciklama      : version.json'u ceker, parse eder ve mevcut
 *                 YAZILIM_VERSIYON ile karsilastirir. Caller
 *                 OTA_OK donerse ota_guncelle'yi cagirabilir.
 ****************************************************************/
ota_sonuc_t ota_kontrol(ota_firmware_bilgi_t *p_bilgi_st);


/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : ota_guncelle
 * Parametre     : p_bilgi_st → ota_kontrol'den gelen bilgi
 * Donus Degeri  : OTA_OK     → basarili (fonksiyon geri donmez,
 *                              esp_restart ile cihaz yeniden baslar)
 *                 OTA_HATA_* → hata
 * Aciklama      : firmware.bin'i chunk bazli indirip pasif
 *                 OTA partition'a yazar. Indirirken paralel MD5
 *                 hesaplar, sunucudaki MD5 ile karsilastirir.
 *                 Uyumluysa esp_ota_set_boot_partition + restart.
 ****************************************************************/
ota_sonuc_t ota_guncelle(const ota_firmware_bilgi_t *p_bilgi_st);
