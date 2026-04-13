#ifndef SIM800C_H
#define SIM800C_H

#include <stdint.h>
#include <stddef.h>

#define SIM800C_VERI_VAR           (1U)

/* ── IO Arayüzü (Dependency Injection) ─────────────────────────────
* main.c bu struct'ı doldurup sim800c_init()'e verir.
* ----------------------------------------------------------------- */
typedef struct {
    void (*send)(const uint8_t *data, size_t len);
    int  (*read)(uint8_t *buf, size_t len, uint32_t timeout_ms);
    void (*log )(const char *msg);
} sim800c_io_t;

/* ── State Machine Durumları ────────────────────────────────────── */
typedef enum {
    SIM800C_IDLE       ,  // Komut bekleniyor
    SIM800C_ECHO_BEKLE ,  // Echo dönmesi bekleniyor
    SIM800C_CEVAP_BEKLE,  // OK / ERROR bekleniyor
    SIM800C_CEVAP_HAZIR,  // Cevap hazır, işlenebilir
    SIM800C_HATA       ,  // Hata durumu
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



#endif /* SIM800C_H */