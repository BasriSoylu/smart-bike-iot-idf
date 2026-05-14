#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "sim800c.h"
#include "at_commands.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"


typedef enum 
{
    SIM_RX_LINE        ,
    SIM_RX_HTTP_BINARY ,
    SIM_RX_TCP_BINARY
}sim_rx_modu_t;


/* ──────────────────── Static Degiskenler ───────────────────────
* Modül dışından erişilemez, sadece bu .c içinde kullanılır.
* STM32'de global değişkeni static yapmak gibi düşün.
* -------------------------------------------------------------- */
static sim800c_io_t            *sim_arayuz         = NULL                        ;   // inject edilen uart arayüzü
static volatile sim800c_state_t sim_durum          = SIM800C_IDLE                ;   // mevcut state (volatile: iki task aynı anda okur/yazar)
static char                     sim_cevap[4096]                                  ;   // gelen tüm satırlar (her komut öncesi sıfırlanır)
static char                     sim_son_komut[128]                               ;   // gönderilen son komut (echo karşılaştırması için)
static portMUX_TYPE             sim_mux            = portMUX_INITIALIZER_UNLOCKED;   // critical section kilidi (dual-core koruması)

static volatile sim_rx_modu_t        sim_rx_modu   = SIM_RX_LINE      ;   // reader_task mod
static volatile uint32_t             sim_rx_kalan  = 0                ;   // binary modda kalan byte
static          StreamBufferHandle_t sim_http_sb   = NULL             ;   // HTTP binary stream
static          StreamBufferHandle_t sim_tcp_sb    = NULL             ;   // TCP  binary stream
static volatile sim_tcp_durum_t      sim_tcp_durum = TCP_DISCONNECTED ;

/* ──────────────────── Static Fonksiyonlar ─────────────────────── */
static void sim800c_reader_task(void *arg);
static void sim800c_logf(const char *fmt, ...);
static void sim800c_process_line(const char *line);
static int  sim800c_http_open_adimlari(const char *url, int *total_len);
static int  sim800c_http_read_adimlari(int offset, uint8_t *out_buf, int size, int *out_len);
static void urc_httpread_isle(const char *line);
static void urc_receive_isle(const char *line);
static void urc_ipd_isle           (const char *line);
static void urc_connect_ok_isle    (const char *line);
static void urc_connect_fail_isle  (const char *line);
static void urc_already_conn_isle  (const char *line);
static void urc_closed_isle        (const char *line);


/* ──────────────────── URC Tablo Yapilari ─────────────────────── */
typedef void (*urc_isleyici_t)(const char *line);

typedef struct {
    const char     *prefix;
    urc_isleyici_t  fonksiyon;
} sim800c_urc_satiri_t;

/* ──────────────────── URC Fihristimiz (Tablo) ─────────────────── */
static const sim800c_urc_satiri_t urc_tablosu[] = {
    { "+HTTPREAD:"     , urc_httpread_isle     },
    { "+RECEIVE,"      , urc_receive_isle      },
    { "+IPD,"          , urc_ipd_isle          },
    { "CONNECT OK"     , urc_connect_ok_isle   },
    { "CONNECT FAIL"   , urc_connect_fail_isle },
    { "ALREADY CONNECT", urc_already_conn_isle },
    { "CLOSED"         , urc_closed_isle       },
};

#define URC_TABLO_BOYUTU (sizeof(urc_tablosu) / sizeof(urc_tablosu[0]))


/* ──────────────────── Dahili Log Yardimcisi ─────────────────── */
static void sim800c_logf(const char *fmt, ...)
{
    if ( (NULL != sim_arayuz) && (NULL != sim_arayuz->log) )
    {
        char message[256];
        char message_out[280];
        va_list args;

        va_start(args, fmt);
        vsnprintf(message, sizeof(message), fmt, args);
        va_end(args);

        snprintf(message_out, sizeof(message_out), "[SIM800C] %s", message);

        sim_arayuz->log(message_out);
    }
}

/* ─────────────────── Public API Implementasyonlari ──────────────────── */
void sim800c_init(sim800c_io_t *io)
{
    sim_arayuz = io;
    sim_durum  = SIM800C_IDLE;
    
    memset(sim_cevap,     0, sizeof(sim_cevap));
    memset(sim_son_komut, 0, sizeof(sim_son_komut));

    sim_http_sb = xStreamBufferCreate(2048, 1);   // 2KB buffer, 1 byte trigger
    sim_tcp_sb  = xStreamBufferCreate(2048, 1);

    if ( (NULL == sim_http_sb) || (NULL == sim_tcp_sb) )
    {
        sim800c_logf("HATA: StreamBuffer olusturulamadi (RAM yetersiz?)");
        return;
    }

    xTaskCreate(sim800c_reader_task, "sim800c_reader", 4096, NULL, 5, NULL);
}

int sim800c_baslat(void)
{
    static const uint32_t baud_listesi[] = { 115200U, 9600U, 19200U, 38400U, 57600U };
    const    int          baud_sayisi    = sizeof(baud_listesi) / sizeof(baud_listesi[0]);
    uint32_t              bulunan_baud   = 0U;
    uint8_t               autobaud_ok    = 0U;
    int                   i;

    vTaskDelay(pdMS_TO_TICKS(3000));

    for ( i = 0; i < baud_sayisi; i++ )
    {
        int j;

        sim800c_logf("Baud taraniyor: %u", (unsigned)baud_listesi[i]);
        sim_arayuz->set_baud(baud_listesi[i]);
        vTaskDelay(pdMS_TO_TICKS(100));

        /* Autobaud kilitlenmesi icin 3 deneme + 3sn timeout (modul yavas cevap verebilir) */
        for ( j = 0; j < 3; j++ )
        {
            if ( 0 == sim800c_cmd_wait(AT_TEST, "OK", BEKLE_3_SN) )
            {
                bulunan_baud = baud_listesi[i];
                sim800c_logf(">>> Modul bulundu: Baud=%u (deneme %d)", (unsigned)bulunan_baud, j + 1);
                break;
            }
        }

        if ( 0U != bulunan_baud )
        {
            break;
        }
    }

    if ( 0U == bulunan_baud )
    {
        sim800c_logf("HATA: Modul hicbir baud'da cevap vermedi! Donanim/guc/kablolama kontrolu gerek.");
        return -1;
    }

    if ( 0 == sim800c_cmd_wait(AT_AUTOBAUD, "OK", BEKLE_2_SN) )
    {
        sim800c_logf("Autobaud modu acildi (AT+IPR=0)");
        autobaud_ok = 1U;

        if ( 0 == sim800c_cmd_wait(AT_SAVE_CONFIG, "OK", BEKLE_2_SN) )
        {
            sim800c_logf("Modul autobaud'a kalici kaydedildi (AT&W)");
        }
        else
        {
            sim800c_logf("UYARI: AT&W basarisiz — autobaud ayari gecici (reset sonrasi kaybolur)");
        }
    }
    else
    {
        sim800c_logf("UYARI: AT+IPR=0 basarisiz — modul hala sabit %u baud'da, hedef baud'a gecis yapilmayacak", (unsigned)bulunan_baud);
    }

    if ( (0U != autobaud_ok) && (SIM800C_HEDEF_BAUD_RATE != bulunan_baud) )
    {
        sim_arayuz->set_baud(SIM800C_HEDEF_BAUD_RATE);
        vTaskDelay(pdMS_TO_TICKS(100));

        if ( 0 != sim800c_cmd_wait(AT_TEST, "OK", BEKLE_1_SN) )
        {
            sim800c_logf("HATA: %u -> %u baud gecisi basarisiz!", (unsigned)bulunan_baud, SIM800C_HEDEF_BAUD_RATE);
            return -1;
        }
        sim800c_logf("Baud gecisi: %u -> %u OK", (unsigned)bulunan_baud, SIM800C_HEDEF_BAUD_RATE);
    }

    if ( 0U != autobaud_ok )
    {
        sim800c_logf("Modul hazir: Baud=%u (autobaud aktif)", SIM800C_HEDEF_BAUD_RATE);
    }
    else
    {
        sim800c_logf("Modul hazir: Baud=%u (autobaud yok, tarama baud'unda devam)", (unsigned)bulunan_baud);
    }
    return 0;
}

void sim800c_send_command(const char *cmd)
{
    char    buf[160];
    uint8_t idle_mi ;

    snprintf(sim_son_komut, sizeof(sim_son_komut), "%s", cmd);  // Echo kontrolu icin sakla
    snprintf(buf, sizeof(buf), "%s\r\n", cmd);

    portENTER_CRITICAL(&sim_mux);
    idle_mi = (uint8_t)(SIM800C_IDLE == sim_durum);
    if(idle_mi)
    {
        sim_durum = SIM800C_ECHO_BEKLE;
    }
    portEXIT_CRITICAL(&sim_mux);

    if(idle_mi)
    {
        sim_arayuz->send( (const uint8_t *)buf, strlen(buf) );
        sim800c_logf("Komut gonderildi: %s", cmd);
    }
    else
    {
        sim800c_logf("Onceki komut bitmedi, gonderilmedi: %s", cmd);
    }
}

/* ──────────────────── URC İşleyici Fonksiyonlar ─────────────────── */
static void urc_httpread_isle(const char *line)
{
    int beklenen_len = 0;
    
    // Gelen metnin içinden sadece sayıyı (kaç byte geleceğini) çekiyoruz
    sscanf(strstr(line, "+HTTPREAD:"), "+HTTPREAD: %d", &beklenen_len);
    
    if( 0 < beklenen_len )
    {
        sim_rx_kalan = (uint32_t)beklenen_len;
        sim_rx_modu  = SIM_RX_HTTP_BINARY;
        // İstersen buraya da bir log ekleyebilirsin: 
        // sim800c_logf("HTTP binary mod: %d byte bekleniyor", beklenen_len);
    }
}

static void urc_receive_isle(const char *line)
{
    int beklenen_len = 0;

    // TCP üzerinden gelecek verinin boyutunu okuyoruz
    sscanf(line, "+RECEIVE,%d", &beklenen_len);

    if( 0 < beklenen_len)
    {
        sim_rx_kalan = (uint32_t)beklenen_len;
        sim_rx_modu  = SIM_RX_TCP_BINARY;
        sim800c_logf("TCP binary mod: %d byte bekleniyor", beklenen_len);
    }
}

/* +IPD,<len>: prefix - AT+CIPHEAD=1 ile aktif olur, +RECEIVE'nin alternatifi */
static void urc_ipd_isle(const char *line)
{
    int beklenen_len = 0;

    sscanf(line, "+IPD,%d", &beklenen_len);

    if ( 0 < beklenen_len )
    {
        sim_rx_kalan = (uint32_t)beklenen_len;
        sim_rx_modu  = SIM_RX_TCP_BINARY;
        sim800c_logf("TCP binary mod (+IPD): %d byte bekleniyor", beklenen_len);
    }
}

static void urc_connect_ok_isle    (const char *line)
{
    sim_tcp_durum = TCP_CONNECTED;
    sim800c_logf("TCP baglandi");
}

static void urc_connect_fail_isle  (const char *line)
{
    sim_tcp_durum = TCP_DISCONNECTED;
    sim800c_logf("TCP baglanti hatasi");
}

static void urc_already_conn_isle  (const char *line)
{
    sim_tcp_durum = TCP_CONNECTED;
    sim800c_logf("TCP zaten bagli");
}

static void urc_closed_isle        (const char *line)
{
    sim_tcp_durum = TCP_DISCONNECTED;
    sim800c_logf("TCP baglanti kapandi");
}

static void sim800c_process_line(const char *line)
{
    uint32_t dolu_uzunluk    ;
    uint32_t bos_alan        ;
    bool     urc_yakalandi = false; // Yeni ekledik: Eğer satır bir URC ise bunu bilelim

    if( (NULL == line) || ('\0' == line[0]) )
    {
        return;
    }

    /* ─────────── 1. URC Tablosunu Kontrol Et (Yeni Fihrist Sistemi) ─────────── */
    for (int i = 0; i < URC_TABLO_BOYUTU; i++)
    {
        // Gelen satırın içinde tablodaki prefix (kelime) var mı?
        if (NULL != strstr(line, urc_tablosu[i].prefix))
        {
            // Kelime bulundu! İlgili fonksiyonu çalıştır.
            urc_tablosu[i].fonksiyon(line);
            urc_yakalandi = true;
            
            // Eğer gelen veri "+RECEIVE," veya "+IPD," gibi binary bir datanın habercisiyse,
            // bunu standart "sim_cevap" buffer'ına YAZMAMAK için fonksiyondan çıkıyoruz.
            if ( (0 == strcmp(urc_tablosu[i].prefix, "+RECEIVE,")) ||
                 (0 == strcmp(urc_tablosu[i].prefix, "+IPD,"     )) )
            {
                return;
            }
            break; // Eşleşmeyi bulduk, tablonun geri kalanına bakmaya gerek yok.
        }
    }

/* ─────────── 2. Mevcut State Machine (Echo ve Yanıt Bekleme) ─────────── */
    if ( SIM800C_ECHO_BEKLE == sim_durum )
    {
        if ( 0 == strncmp(line, sim_son_komut, strlen(sim_son_komut)) )
        {
            portENTER_CRITICAL(&sim_mux);
            sim_durum = SIM800C_CEVAP_BEKLE;
            portEXIT_CRITICAL(&sim_mux);
            sim800c_logf("Echo alindi: %s", line);
        }
    }
    else if ( SIM800C_CEVAP_BEKLE == sim_durum )
    {
        dolu_uzunluk = strlen(sim_cevap);
        bos_alan     = sizeof(sim_cevap) - dolu_uzunluk - 1;
        snprintf( (sim_cevap + dolu_uzunluk), bos_alan, "%s\n", line );
        
        // Eğer bu satır bir URC değilse (yani normal bir AT cevabıysa) logla
        if (!urc_yakalandi) {
            sim800c_logf("Satir alindi: %s", line);
        }
    }
}

static void sim800c_reader_task(void *arg)
{
    uint8_t  byte              ;
    char     line_buf[256]     ;
    uint16_t line_pos      = 0 ;
    uint8_t  over_flow     = 0 ;
    int      ret               ;

    while(true)
    {
        ret = sim_arayuz->read(&byte, 1, 100);

        if(SIM800C_VERI_VAR != ret)
        {
            continue;
        }

        /* GECICI DEBUG: raw byte log */
        sim800c_logf("RX: 0x%02X '%c' modu=%d",
                     byte,
                     ((byte >= 32) && (byte < 127)) ? (char)byte : '.',
                     (int)sim_rx_modu);

        switch (sim_rx_modu)
        {

            /* ─────────── LINE MODU: mevcut davranis ─────────── */
            case SIM_RX_LINE:
            {
                if('\r' == byte)
                {
                    /* yok say*/
                }
                else if('\n' == byte)
                {
                    if ( false == over_flow )
                    {
                        line_buf[line_pos] = '\0';
                        sim800c_process_line(line_buf);
                    }
                    line_pos  = 0    ;
                    over_flow = false;
                }
                else if ( ('>' == byte) && (0 == line_pos) )
                {
                    /* SIM800 CIPSEND/HTTP veri prompt'u - \r\n ile bitmiyor,
                     * tek karakter geliyor. Hemen process_line'a teslim et
                     * ki cmd_wait ">" karakterini sim_cevap'ta gorsun. */
                    line_buf[0] = '>';
                    line_buf[1] = '\0';
                    sim800c_process_line(line_buf);
                    /* line_pos zaten 0 */
                }
                else
                {
                    line_buf[line_pos] = byte;
                    line_pos++;

                    if ( line_pos >= sizeof(line_buf) )
                    {
                        sim800c_logf("Buffer overFlow, satir sifirlandi");
                        line_pos  = 0;
                        over_flow = true;
                    }
                }
                break;
            }
             /* ─────────── HTTP BINARY MODU: N byte → http_sb ─────────── */
            case SIM_RX_HTTP_BINARY:
            {
                xStreamBufferSend(sim_http_sb, &byte, 1, 0);
                sim_rx_kalan--;

                if( 0 == sim_rx_kalan)
                {
                    sim_rx_modu = SIM_RX_LINE;
                }
                break;
            }

            /* ─────────── TCP BINARY MODU: N byte → tcp_sb ─────────── */
            case SIM_RX_TCP_BINARY:
            {
                xStreamBufferSend(sim_tcp_sb, &byte, 1, 0);
                sim_rx_kalan--;

                if(0 == sim_rx_kalan)
                {
                    sim_rx_modu = SIM_RX_LINE;
                }
                break;
            }

            /* ─────────── HATALI MOD ATAMASI ─────────── */
            default:
            {
                sim800c_logf("Hatali Okuma Modu Atandi");
                break;
            }
        }
    }
}

sim800c_state_t sim800c_get_state(void)
{
    return sim_durum;
}

const char *sim800c_get_response(void)
{
    return sim_cevap;
}

int sim800c_cmd_wait(const char *cmd, const char *beklenen, uint32_t timeout_ms)
{
    uint32_t gecen_sure = 0;

    portENTER_CRITICAL(&sim_mux);
    memset(sim_cevap, 0, sizeof(sim_cevap));
    sim_durum = SIM800C_IDLE;
    portEXIT_CRITICAL(&sim_mux);

    sim800c_send_command(cmd);

    while(gecen_sure < timeout_ms)
    {
        if(NULL != strstr(sim_cevap, beklenen))
        {
            sim_durum = SIM800C_IDLE;
            return 0;
        }
        if( (0 != strcmp(beklenen, "ERROR")) && (NULL != strstr(sim_cevap, "ERROR")) )
        {
            sim_durum = SIM800C_IDLE;
            sim800c_logf("Hata: %s", cmd);
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        gecen_sure += 10;
    }

    sim_durum = SIM800C_IDLE;
    sim800c_logf("Timeout: %s", cmd);
    return -1;
}

int sim800c_gprs_connect(void)
{
    int  geri_donus_degeri = -1;
    char apn_komutu[64];

    if(sim800c_cmd_wait(AT_GPRS_ATTACH_QUERY, "OK", BEKLE_3_SN) == 0)
    {
        if(NULL != strstr(sim800c_get_response(), "+CGATT: 1"))
        {
            sim800c_logf("GPRS Zaten Bagli");
        }
        else if(sim800c_cmd_wait(AT_GPRS_ATTACH, "OK", BEKLE_5_SN) != 0)
        {
            sim800c_logf("GPRS Baglantisi Basarisiz!!!");
            return geri_donus_degeri;
        }
        else
        {
            sim800c_logf("GPRS'e Baglanildi");
        }
    }
    else
    {
        sim800c_logf("GPRS Baglantisi Sorgulanamadi!!!");
        return geri_donus_degeri;
    }

    sim800c_logf("Mevcut Bearer durumu temizleniyor...");
    sim800c_cmd_wait(AT_BEARER_CLOSE, "OK", BEKLE_3_SN);

    if(sim800c_cmd_wait(AT_BEARER_SET_CONTYPE, "OK", BEKLE_3_SN) == 0)
    {
        sim800c_logf("Bearer Tipi: GPRS");
    }
    else
    {
        sim800c_logf("Bearer Tipi Ayarlanamadi!!!");
        return geri_donus_degeri;
    }

    snprintf(apn_komutu, sizeof(apn_komutu), AT_BEARER_APN, "internet");
    if(sim800c_cmd_wait(apn_komutu, "OK", BEKLE_3_SN) == 0)
    {
        sim800c_logf("APN Ayarlandi");
    }
    else
    {
        sim800c_logf("APN Ayarlanamadi!!!");
        return geri_donus_degeri;
    }

    if(sim800c_cmd_wait(AT_BEARER_START, "OK", BEKLE_30_SN) == 0)
    {
        sim800c_logf("Bearer Baslatildi");
    }
    else
    {
        sim800c_logf("Bearer Baslatilamadi!!!");
        return geri_donus_degeri;
    }

    if(sim800c_cmd_wait(AT_BEARER_QUERY, "OK", BEKLE_3_SN) == 0)
    {
        sim800c_logf("IP adresi: %s", sim_cevap); // Parsing şimdilik atlandı
    }
    else
    {
        sim800c_logf("IP Sorgulanamadi!!!");
        return geri_donus_degeri;
    }

    geri_donus_degeri = 0;
    return geri_donus_degeri;
}

int sim800c_gprs_disconnect(void)
{
    int geri_donus_degeri = -1;

    if(sim800c_cmd_wait(AT_BEARER_CLOSE, "OK", BEKLE_3_SN) == 0)
    {
        sim800c_logf("Bearer Kapatildi");
    }
    else
    {
        sim800c_logf("Bearer Kapatilamadi!!!");
        return geri_donus_degeri;
    }

    if(sim800c_cmd_wait(AT_GPRS_DETACH, "OK", BEKLE_3_SN) == 0)
    {
        sim800c_logf("GPRS Baglantisi Koparildi");
    }
    else
    {
        sim800c_logf("GPRS Baglantisi Koparilamadi!!!");
        return geri_donus_degeri;
    }

    geri_donus_degeri = 0;
    return geri_donus_degeri;
}

int sim800c_http_get_json(const char *url, char *out_buf, int out_max)
{
    int        geri_donus_degeri = -1  ;
    char       url_komutu[256]   = {0} ;
    const char *json_bas               ;
    const char *json_son               ;
    int        uzunluk           = 0   ;

    sim800c_cmd_wait(AT_HTTP_TERM, "OK", BEKLE_3_SN);

    if(sim800c_cmd_wait(AT_HTTP_INIT, "OK", BEKLE_3_SN) == 0)
    {
        sim800c_logf("HTTP Baslatildi.");
    }
    else
    {
        sim800c_logf("HTTP Baslatilamadi!!!");
        return geri_donus_degeri;
    }

    if(sim800c_cmd_wait(AT_HTTP_BEARER, "OK", BEKLE_3_SN) == 0)
    {
        sim800c_logf("HTTP Bearer Ayarlandi");
    }
    else
    {
        sim800c_logf("HTTP Bearer Ayarlanamadi!!!");
        sim800c_cmd_wait(AT_HTTP_TERM, "OK", BEKLE_3_SN);
        return geri_donus_degeri;
    }

    snprintf(url_komutu, sizeof(url_komutu), AT_HTTP_URL, url);
    if(sim800c_cmd_wait(url_komutu, "OK", BEKLE_3_SN) == 0)
    {
        sim800c_logf("URL Ayarlandi");
    }
    else
    {
        sim800c_logf("URL Ayarlanamadi!!!");
        sim800c_cmd_wait(AT_HTTP_TERM, "OK", BEKLE_3_SN);
        return geri_donus_degeri;
    }

    if(sim800c_cmd_wait(AT_HTTP_GET, "+HTTPACTION:", BEKLE_30_SN) == 0)
    {
        sim800c_logf("HTTP GET Cevabi Alindi: %s", sim_cevap);
    }
    else
    {
        sim800c_logf("HTTP GET Basarisiz!!!");
        sim800c_cmd_wait(AT_HTTP_TERM, "OK", BEKLE_3_SN);
        return geri_donus_degeri;
    }

    if(NULL == strstr(sim_cevap, ",200,"))
    {
        sim800c_logf("HTTP Durum Kodu Hatali: %s", sim_cevap);
        sim800c_cmd_wait(AT_HTTP_TERM, "OK", BEKLE_3_SN);
        return geri_donus_degeri;
    }   

    if ( 0 == sim800c_cmd_wait(AT_HTTP_READ, "OK", BEKLE_5_SN) )
    {
        /* JSON icerik stream buffer'da (reader_task +HTTPREAD URC'sini yakalayip oraya pushladi) */
        size_t alinan = xStreamBufferReceive  ( sim_http_sb,
                                                (uint8_t *)out_buf,
                                                (size_t)(out_max - 1),
                                                pdMS_TO_TICKS(1000));
        out_buf[alinan] = '\0';
        sim800c_logf("HTTP Veri Okundu (%u byte)", (unsigned)alinan);
    }
    else
    {
        sim800c_logf("HTTP Veri Okunamadi!!!");
        sim800c_cmd_wait(AT_HTTP_TERM, "OK", BEKLE_3_SN);
        return geri_donus_degeri;
    }

    json_bas = strchr(out_buf, '{');
    json_son = strrchr(out_buf, '}');

    if ( (NULL != json_bas) && (NULL != json_son) && (json_son > json_bas) )
    {
        uzunluk = (int)(json_son - json_bas) + 1;
        if ( uzunluk < out_max )
        {
            memmove(out_buf, json_bas, uzunluk);
            out_buf[uzunluk] = '\0';
            geri_donus_degeri = uzunluk;
        }
    }

    sim800c_cmd_wait(AT_HTTP_TERM, "OK", BEKLE_3_SN);

    return geri_donus_degeri;
}

static int sim800c_http_open_adimlari(const char *url, int *total_len)
{
    char        url_komutu[256]      ;
    const char *urc_bas              ;
    int         mode                 ;
    int         kod                  ;
    int         uzunluk              ;

    sim800c_cmd_wait(AT_HTTP_TERM, "OK", BEKLE_3_SN);

    if ( sim800c_cmd_wait(AT_HTTP_INIT, "OK", BEKLE_3_SN) != 0 )
    {
        sim800c_logf("HTTP Baslatilamadi!!!");
        return -1;
    }
    sim800c_logf("HTTP Baslatildi");

    if ( sim800c_cmd_wait(AT_HTTP_BEARER, "OK", BEKLE_3_SN) != 0 )
    {
        sim800c_logf("HTTP Bearer Ayarlanamadi!!!");
        return -1;
    }
    sim800c_logf("HTTP Bearer Ayarlandi");

    snprintf(url_komutu, sizeof(url_komutu), AT_HTTP_URL, url);
    if ( sim800c_cmd_wait(url_komutu, "OK", BEKLE_3_SN) != 0 )
    {
        sim800c_logf("URL Ayarlanamadi!!!");
        return -1;
    }
    sim800c_logf("URL Ayarlandi: %s", url);

    if ( sim800c_cmd_wait(AT_HTTP_GET, "+HTTPACTION:", BEKLE_120_SN) != 0 )
    {
        sim800c_logf("HTTP GET Basarisiz!!!");
        return -1;
    }

    urc_bas = strstr(sim800c_get_response(), "+HTTPACTION:");
    if ( NULL == urc_bas )
    {
        sim800c_logf("HTTPACTION URC Bulunamadi!!!");
        return -1;
    }

    if ( 3 != sscanf(urc_bas, "+HTTPACTION: %d,%d,%d", &mode, &kod, &uzunluk) )
    {
        sim800c_logf("HTTPACTION Parse Hatasi!!!");
        return -1;
    }

    if ( 200 != kod )
    {
        sim800c_logf("HTTP Durum Kodu Hatali: %d", kod);
        return -1;
    }

    *total_len = uzunluk;
    sim800c_logf("HTTP Oturumu Acildi, Icerik Boyutu: %d byte", uzunluk);

    return 0;
}

int sim800c_http_open(const char *url, int *total_len)
{
    int geri_donus_degeri;

    geri_donus_degeri = sim800c_http_open_adimlari(url, total_len);

    if ( 0 != geri_donus_degeri )
    {
        sim800c_cmd_wait(AT_HTTP_TERM, "OK", BEKLE_3_SN);
    }

    return geri_donus_degeri;
}

static int sim800c_http_read_adimlari(int offset, uint8_t *out_buf, int size, int *out_len)
{
    char      komut[64]         ;
    int       okunan        = 0 ;
    int       kalan             ;
    uint8_t  *p                 ;

    /* Eski verileri at — bu okuma icin temiz bir SB ile basla */
    xStreamBufferReset(sim_http_sb);

    /* AT+HTTPREAD komutunu gonder (cevap beklemeden, sadece send) */
    snprintf(komut, sizeof(komut), "%s=%d,%d\r\n", AT_HTTP_READ, offset, size);
    sim_arayuz->send( (const uint8_t *)komut, strlen(komut) );

    /* Reader_task otomatik:
     * 1) "+HTTPREAD: N" URC'sini yakalar
     * 2) sim_rx_modu = SIM_RX_HTTP_BINARY yapar
     * 3) N byte'i sim_http_sb'ye pushlar
     * 4) LINE moda geri doner
     * Biz sadece SB'den okuyoruz. */

    p     = out_buf;
    kalan = size;

    while(kalan > 0)
    {
        size_t alinan = xStreamBufferReceive(sim_http_sb, p, (size_t)kalan, pdMS_TO_TICKS(5000));
        if(0 == alinan)
        {
            sim800c_logf("HTTP SB Timeout: okunan=%u beklenen=%d", (unsigned)okunan, size);
            return -1;
        }
        p      += alinan    ;
        okunan += alinan    ;
        kalan  -=(int)alinan;
    }

    *out_len = (int)okunan;

    return 0;
}

int sim800c_http_read(int offset, uint8_t *out_buf, int size, int *out_len)
{
    int geri_donus_degeri;

    /* Yeni mimaride flush gereksiz: reader_task surekli aktif,
     * byte'lar otomatik stream buffer'a akiyor. Eski sim_binary_modu
     * dizayninda kalan flush() cagrisi reader ile race'e girip
     * uart_flush_input deadlock'una yol aciyordu - kaldirildi. */
    vTaskDelay(pdMS_TO_TICKS(150));

    geri_donus_degeri = sim800c_http_read_adimlari(offset, out_buf, size, out_len);

    return geri_donus_degeri;
}

int sim800c_http_close(void)
{
    int geri_donus_degeri = -1;

    if ( sim800c_cmd_wait(AT_HTTP_TERM, "OK", BEKLE_3_SN) == 0 )
    {
        sim800c_logf("HTTP Oturumu Kapatildi");
        geri_donus_degeri = 0;
    }
    else
    {
        sim800c_logf("HTTP Oturumu Kapatilamadi!!!");
    }

    return geri_donus_degeri;   
}

/* ──────────────────── TCP / IP Katmanı ─────────────────────── */

int sim800c_tcp_open(const char *ip, int port)
{
    char komut[128];

    /* Onceki IP state'ini sifirla - CIPMUX ancak temiz state'te degistirilebilir */
    sim800c_cmd_wait("AT+CIPSHUT", "SHUT OK", BEKLE_5_SN);

    // 1. Tek bağlantı moduna geç
    if (sim800c_cmd_wait(AT_CIPMUX_SINGLE, "OK", BEKLE_3_SN) != 0)
    {
        sim800c_logf("TCP: CIPMUX ayarlanamadi!");
        return -1;
    }

    /* TCP verisi geldiginde "+IPD,<len>:" prefix eklenmesi icin */
    sim800c_cmd_wait("AT+CIPHEAD=1", "OK", BEKLE_3_SN);

    /* NOT: AT+CIPRXGET=1 (manuel mod) CIKARILDI.
     * Default mod (CIPRXGET=0 - otomatik) gerekli cunku +RECEIVE,N: URC'si
     * bizim reader_task'in HTTP_BINARY/TCP_BINARY state machine'i ile
     * uyumlu. Manuel mod +CIPRXGET: 1,N notification doner, yeniden
     * AT+CIPRXGET=2,N ile cekmek gerek - mevcut mimariyle uyumsuz. */

    // 2. TCP Bağlantısını Başlatıyoruz (Senin AT_CIPSTART makronu kullanarak)
    snprintf(komut, sizeof(komut), "%s=\"TCP\",\"%s\",\"%d\"", AT_CIPSTART, ip, port);
    sim800c_logf("TCP Baglaniliyor: %s:%d", ip, port);
    
    if (sim800c_cmd_wait(komut, "CONNECT OK", BEKLE_30_SN) == 0)
    {
        sim800c_logf("TCP Baglantisi Basarili!");
        return 0;
    }

    sim800c_logf("TCP Baglantisi Kurulamadi!");
    return -1;
}

int sim800c_tcp_send(const uint8_t *data, int len)
{
    char komut[32];
    uint32_t gecen_sure = 0;

    // Senin AT_CIPSEND makronu kullanarak uzunluğu ekliyoruz
    snprintf(komut, sizeof(komut), "%s=%d", AT_CIPSEND, len);
    
    if (sim800c_cmd_wait(komut, ">", BEKLE_5_SN) != 0)
    {
        sim800c_logf("TCP: Veri gonderim istegi reddedildi!");
        return -1;
    }

    portENTER_CRITICAL(&sim_mux);
    sim_durum = SIM800C_CEVAP_BEKLE; 
    memset(sim_cevap, 0, sizeof(sim_cevap));
    portEXIT_CRITICAL(&sim_mux);

    sim_arayuz->send(data, len);

    while (gecen_sure < 10000)
    {
        if (strstr(sim_cevap, "SEND OK") != NULL)
        {
            sim_durum = SIM800C_IDLE;
            sim800c_logf("TCP: %d byte basariyla gonderildi.", len);
            return 0;
        }
        if (strstr(sim_cevap, "SEND FAIL") != NULL || strstr(sim_cevap, "ERROR") != NULL)
        {
            sim_durum = SIM800C_IDLE;
            sim800c_logf("TCP: Gonderim HATASI!");
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        gecen_sure += 10;
    }

    sim_durum = SIM800C_IDLE;
    sim800c_logf("TCP: Gonderim Zaman Asimi!");
    return -1;
}

int sim800c_tcp_recv(uint8_t *out_buf, int max_size, uint32_t timeout_ms)
{
    size_t okunan = 0;

    // Stream buffer henüz oluşturulmadıysa hata dön
    if (sim_tcp_sb == NULL)
    {
        return -1;
    }

    // URC tablomuzun arka planda sim_tcp_sb içine doldurduğu verileri 
    // doğrudan buradan MQTT veya main katmanı için çekiyoruz.
    okunan = xStreamBufferReceive(  sim_tcp_sb, 
                                    out_buf, 
                                    (size_t)max_size, 
                                    pdMS_TO_TICKS(timeout_ms));

    // Kaç byte okuduğunu döndürür (0 dönerse timeout olmuştur)
    return (int)okunan; 
}

int sim800c_tcp_close(void)
{
    // Senin AT_CIPCLOSE makronu doğrudan kullanıyoruz
    if (sim800c_cmd_wait(AT_CIPCLOSE, "CLOSE OK", BEKLE_5_SN) == 0)
    {
        sim800c_logf("TCP Baglantisi Kapatildi.");
        return 0;
    }
    return -1;
}

sim_tcp_durum_t sim800c_get_tcp_state(void)
{
    return sim_tcp_durum;
}