#ifndef SIM800C_H
#define SIM800C_H

#include <stdint.h>
#include <stddef.h>
#include "config.h"

#define DEBUG_PAKET_CIKTISI        (0U)

#define SIM800C_VERI_VAR           (1U)
#define SIM800C_HEDEF_BAUD_RATE    (115200U)
#define SIM800C_DEFAULT_BAUD_RATE  (9600U)

/* ──────────────── IO Arayüzü (Dependency Injection) ──────────────── */
/* main.c bu struct'ı doldurup sim800c_init()'e verir.*/
/* ----------------------------------------------------------------- */
typedef struct {
    void (*send)    (const uint8_t  *data , size_t len                     );
    int  (*read)    (      uint8_t  *buf  , size_t len, uint32_t timeout_ms);
    void (*log )    (const char     *msg                                   );
    void (*set_baud)(      uint32_t  baud                                  );
    void (*flush)    (      void                                           );
} sim800c_io_t;

/* ─────────────────── State Machine Durumları ───────────────────── */
typedef enum {
    SIM800C_IDLE       ,  // Komut bekleniyor
    SIM800C_ECHO_BEKLE ,  // Echo dönmesi bekleniyor
    SIM800C_CEVAP_BEKLE,  // Cevap satırları bekleniyor (beklenen string gelene kadar)
} sim800c_state_t;

typedef enum
{
    TCP_DISCONNECTED ,
    TCP_CONNECTING   ,
    TCP_CONNECTED
} sim_tcp_durum_t;

/* ── Public API ─────────────────────────────────────────────────── */

/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : sim800c_init
 * Parametre     : io → UART io struct'i
 * Donus Degeri  : void
 * Aciklama      : SIM800C modulunu baslatir.
 ****************************************************************/
void sim800c_init(sim800c_io_t *io);


/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : sim800c_baslat
 * Parametre     : void
 * Donus Degeri  : 0 → basarili, -1 → hata
 * Aciklama      : AT testi yapar, gerekirse baud hizini ayarlar.
 ****************************************************************/
int sim800c_baslat(void);


/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : sim800c_send_command
 * Parametre     : cmd → AT komutu
 * Donus Degeri  : void
 * Aciklama      : AT komutunu UART uzerinden gonderir.
 ****************************************************************/
void sim800c_send_command(const char *cmd);


/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : sim800c_cmd_wait
 * Parametre     : cmd, beklenen, timeout_ms
 * Donus Degeri  : 0 -> beklenen string geldi, -1 -> timeout/ERROR
 * Aciklama      : AT komutu gonderir, cevap buffer'inda "beklenen"
 *                 string'i gorunene kadar bekler. ERROR gorurse
 *                 erken cikar.
 ****************************************************************/
int sim800c_cmd_wait(const char *cmd, const char *beklenen, uint32_t timeout_ms);


/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : sim800c_get_state
 * Parametre     : void
 * Donus Degeri  : mevcut state
 * Aciklama      : State machine durumunu doner.
 ****************************************************************/
sim800c_state_t sim800c_get_state(void);


/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : sim800c_get_response
 * Parametre     : void
 * Donus Degeri  : son AT cevabi
 * Aciklama      : Son gelen AT cevabinin buffer'ini doner.
 ****************************************************************/
const char *sim800c_get_response(void);


/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : sim800c_gprs_connect
 * Parametre     : void
 * Donus Degeri  : 0 → basarili, -1 → hata
 * Aciklama      : GPRS baglantisini kurar.
 ****************************************************************/
int sim800c_gprs_connect(void);


/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : sim800c_gprs_disconnect
 * Parametre     : void
 * Donus Degeri  : 0 → basarili, -1 → hata
 * Aciklama      : Aktif GPRS bearer'ini kapatir.
 ****************************************************************/
int sim800c_gprs_disconnect(void);


/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : sim800c_http_get_json
 * Parametre     : url, out_buf, out_max
 * Donus Degeri  : okunan byte, hata durumunda -1
 * Aciklama      : HTTP GET ile kucuk metin/JSON alir.
 ****************************************************************/
int sim800c_http_get_json(const char *url, char *out_buf, int out_max);


/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : sim800c_http_open
 * Parametre     : url, total_len (out)
 * Donus Degeri  : 0 → basarili, -1 → hata
 * Aciklama      : HTTP oturumunu acar, icerik boyutunu doldurur.
 ****************************************************************/
int sim800c_http_open(const char *url, int *total_len);


/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : sim800c_http_read
 * Parametre     : offset, out_buf, size, out_len (out)
 * Donus Degeri  : 0 → basarili, -1 → hata
 * Aciklama      : Acik HTTP oturumundan binary chunk okur.
 ****************************************************************/
int sim800c_http_read(int offset, uint8_t *out_buf, int size, int *out_len);


/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : sim800c_http_close
 * Parametre     : void
 * Donus Degeri  : 0 → basarili, -1 → hata
 * Aciklama      : HTTP oturumunu kapatir.
 ****************************************************************/
int sim800c_http_close(void);

/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : 
 * Parametre     : 
 * Donus Degeri  : 0 → basarili, -1 → hata
 * Aciklama      :
 ****************************************************************/
int sim800c_tcp_open(const char *ip, int port);

/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : 
 * Parametre     : 
 * Donus Degeri  : 0 → basarili, -1 → hata
 * Aciklama      : 
 ****************************************************************/
int sim800c_tcp_send(const uint8_t *data, int len);

/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : 
 * Parametre     : 
 * Donus Degeri  : 0 → basarili, -1 → hata
 * Aciklama      : 
 ****************************************************************/
int sim800c_tcp_recv(uint8_t *out_buf, int max_size, uint32_t timeout_ms);

/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : 
 * Parametre     : 
 * Donus Degeri  : 0 → basarili, -1 → hata
 * Aciklama      : 
 ****************************************************************/
int sim800c_tcp_close(void);

/****************************************************************
 * Yazan         : Hasan Basri Soylu
 * Fonksiyon     : 
 * Parametre     : 
 * Donus Degeri  : TCP_DISCONNECTED, TCP_CONNECTING, TCP_CONNECTED.
 * Aciklama      : 
 ****************************************************************/
sim_tcp_durum_t sim800c_get_tcp_state(void);



#endif /* SIM800C_H */
