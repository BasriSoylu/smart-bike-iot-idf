#ifndef SIM800C_H
#define SIM800C_H

#include <stdint.h>
#include <stddef.h>
#include "config.h"

#define SIM800C_VERI_VAR           (1U)
#define SIM800C_HEDEF_BAUD_RATE    (115200U)
#define SIM800C_DEFAULT_BAUD_RATE  (9600U)

/* ── IO Arayüzü (Dependency Injection) ─────────────────────────────
* main.c bu struct'ı doldurup sim800c_init()'e verir.
* ----------------------------------------------------------------- */
typedef struct {
    void (*send)    (const uint8_t  *data , size_t len                     );
    int  (*read)    (      uint8_t  *buf  , size_t len, uint32_t timeout_ms);
    void (*log )    (const char     *msg                                   );
    void (*set_baud)(      uint32_t  baud                                  );
} sim800c_io_t;

/* ── State Machine Durumları ────────────────────────────────────── */
typedef enum {
    SIM800C_IDLE       ,  // Komut bekleniyor
    SIM800C_ECHO_BEKLE ,  // Echo dönmesi bekleniyor
    SIM800C_CEVAP_BEKLE,  // Cevap satırları bekleniyor (beklenen string gelene kadar)
} sim800c_state_t;

/* ── Public API ─────────────────────────────────────────────────── */

/****************************************************************
 * Fonksiyon     : sim800c_init
 * Parametre     : io → uart send/read fonksiyon pointerlarini
 *                      tasıyan struct (main.c tarafindan doldurulur)
 * Donus Degeri  : void
 *---------------------------------------------------------------
 * Aciklama      : SIM800C modulunu baslatir. UART baglantisi
 *                 dogrudan degil, io struct uzerinden yapilir.
 *                 (Dependency Injection)
 ****************************************************************/
void sim800c_init(sim800c_io_t *io);


/****************************************************************
 * Fonksiyon     : sim800c_baslat
 * Parametre     : void
 * Donus Degeri  : 0 → basarili, -1 → modul bulunamadi
 *---------------------------------------------------------------
 * Aciklama      : Modul ile iletisimi test eder, gerekirse
 *                 baud hizini 115200'den 9600'e gecirir.
 *                 sim800c_init() cagrisinin ARDINDAN
 *                 cagrilmalidir (reader_task hazir olmali).
 ****************************************************************/
int sim800c_baslat(void);

/****************************************************************
 * Fonksiyon     : sim800c_send_command
 * Parametre     : cmd → gonderilecek AT komutu string'i
 *                       (at_commands.h makrolari kullanilmali)
 * Donus Degeri  : void
 *---------------------------------------------------------------
 * Aciklama      : AT komutunu UART uzerinden gonderir ve state
 *                 machine'i SIM800C_ECHO_BEKLE durumuna alir.
 *                 Fonksiyon bloklanmaz, cevap state machine
 *                 tarafindan islenir.
 ****************************************************************/
void sim800c_send_command(const char *cmd);

/****************************************************************
 * Fonksiyon     : sim800c_get_state
 * Parametre     : void
 * Donus Degeri  : sim800c_state_t → mevcut state machine durumu
 *---------------------------------------------------------------
 * Aciklama      : Dis moduller (main.c vb.) bu fonksiyon ile
 *                 SIM800C'nin su anki durumunu sorgular.
 *                 Ornegin: cevap hazir mi, hata var mi?
 ****************************************************************/
sim800c_state_t sim800c_get_state(void);

/****************************************************************
 * Fonksiyon     : sim800c_get_response
 * Parametre     : void
 * Donus Degeri  : const char* → son gelen AT cevabinin string'i
 *---------------------------------------------------------------
 * Aciklama      : State SIM800C_CEVAP_HAZIR oldugunda bu
 *                 fonksiyon ile cevap okunur. Bir sonraki komut
 *                 gonderilene kadar buffer gecerlidir.
 ****************************************************************/
const char *sim800c_get_response(void);

/****************************************************************
 * Fonksiyon     : sim800c_gprs_connect
 * Parametre     : void
 * Donus Degeri  : 0 → basarili, -1 → hata
 *---------------------------------------------------------------
 * Aciklama      : AT+CGATT, AT+SAPBR komutlari ile GPRS
 *                 baglantisinı kurar. Basarili bağlantida
 *                 modeme bir IP adresi atanmis olur.
 ****************************************************************/
int sim800c_gprs_connect(void);

/****************************************************************
 * Fonksiyon     : sim800c_gprs_disconnect
 * Parametre     : void
 * Donus Degeri  : 0 → basarili, -1 → hata
 *---------------------------------------------------------------
 * Aciklama      : AT+SAPBR=0,1 komutu ile aktif GPRS bearer'i
 *                 kapatir. Dusuk guc moduna gecmeden once
 *                 cagrilmalidir.
 ****************************************************************/
int sim800c_gprs_disconnect(void);

/****************************************************************
 * Fonksiyon     : sim800c_http_get_json
 * Parametre     : url     → hedef URL (null-terminated string)
 *                 out_buf → JSON verisinin yazilacagi buffer
 *                 out_max → buffer maksimum boyutu (byte)
 * Donus Degeri  : okunan byte sayisi, hata durumunda -1
 *---------------------------------------------------------------
 * Aciklama      : AT+HTTPINIT / AT+HTTPACTION / AT+HTTPREAD
 *                 komutlari ile HTTP GET isteği atar ve kucuk
 *                 metin yaniti (version.json gibi) alir.
 *                 Islem sonunda HTTP stack kapatilir.
 ****************************************************************/
int sim800c_http_get_json(const char *url, char *out_buf, int out_max);

/****************************************************************
 * Fonksiyon     : sim800c_http_get_binary
 * Parametre     : url        → hedef URL (null-terminated string)
 *                 offset     → dosyada baslangic konumu (byte)
 *                 out_buf    → verinin yazilacagi buffer
 *                 chunk_size → tek seferde okunacak max byte
 *                 out_len    → bu cagride okunan byte sayisi
 *                 total_len  → dosyanin toplam boyutu
 *                              (sadece offset=0 da doldurulur)
 * Donus Degeri  : 0 → basarili, -1 → hata
 *---------------------------------------------------------------
 * Aciklama      : Buyuk binary dosyalari (firmware.bin) chunk
 *                 chunk indirmek icin kullanilir. offset=0 da
 *                 HTTP baglantisi kurulur, sonraki cagrılarda
 *                 ayni baglanti uzerinden okuma devam eder.
 ****************************************************************/
int sim800c_http_get_binary(const char *url, int offset,
                            uint8_t *out_buf, int chunk_size,
                            int *out_len, int *total_len);

/****************************************************************
 * Fonksiyon     : sim800c_mqtt_publish
 * Parametre     : topic → mesajin yayinlanacagi MQTT topic
 *                 msg   → yayinlanacak mesaj (null-terminated)
 *                 qos   → kalite seviyesi (0, 1 veya 2)
 * Donus Degeri  : 0 → basarili, -1 → hata
 *---------------------------------------------------------------
 * Aciklama      : AT+CIPSTART ile TCP baglantisi kurarak MQTT
 *                 PUBLISH paketi gonderir. gsm_io_t uzerinden
 *                 cagrilir; dogrudan kullanilmamalidir.
 ****************************************************************/
int sim800c_mqtt_publish(const char *topic, const char *msg, int qos);

#endif /* SIM800C_H */