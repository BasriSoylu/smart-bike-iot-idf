#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "sim800c.h"
#include "at_commands.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


/* ──────────────────── Static Degiskenler ───────────────────────
* Modül dışından erişilemez, sadece bu .c içinde kullanılır.
* STM32'de global değişkeni static yapmak gibi düşün.
* -------------------------------------------------------------- */
static sim800c_io_t            *sim_arayuz         = NULL                        ;   // inject edilen uart arayüzü
static volatile sim800c_state_t sim_durum          = SIM800C_IDLE                ;   // mevcut state (volatile: iki task aynı anda okur/yazar)
static char                     sim_cevap[4096]                                  ;   // gelen tüm satırlar (her komut öncesi sıfırlanır)
static char                     sim_son_komut[128]                               ;   // gönderilen son komut (echo karşılaştırması için)
static portMUX_TYPE             sim_mux            = portMUX_INITIALIZER_UNLOCKED;   // critical section kilidi (dual-core koruması)
static volatile uint8_t         sim_binary_modu    = 0                           ;


/* ──────────────────── Static Fonksiyonlar ─────────────────────── */
static void sim800c_reader_task(void *arg);
static void sim800c_logf(const char *fmt, ...);
static void sim800c_process_line(const char *line);
static int  sim800c_cmd_wait(const char *cmd, const char *beklenen, uint32_t timeout_ms);
static int  sim800c_http_open_adimlari(const char *url, int *total_len);
static int  sim800c_http_read_adimlari(int offset, uint8_t *out_buf, int size, int *out_len);


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

    xTaskCreate(sim800c_reader_task, "sim800c_reader", 2048, NULL, 5, NULL);
}


int sim800c_baslat(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));    // sim800c_reader_task'in ayaga kalkmasi bekleniyor

    if(sim800c_cmd_wait(AT_TEST, "OK", BEKLE_1_SN) == 0)
    {
        sim800c_logf("Modul baslatildi, Baud Rate->%d", SIM800C_HEDEF_BAUD_RATE);
        return 0;
    }
    else
    {
        sim800c_logf("Modul Baud Rate->%d'de cevap vermedi. Baud Rate'yi %d'e cek:", SIM800C_HEDEF_BAUD_RATE, SIM800C_DEFAULT_BAUD_RATE);
        sim_arayuz->set_baud(SIM800C_DEFAULT_BAUD_RATE);

        if(sim800c_cmd_wait(AT_TEST, "OK", BEKLE_1_SN) == 0)
        {
            sim800c_logf("Modul baslatildi, Baud Rate->%d", SIM800C_DEFAULT_BAUD_RATE);

            if(sim800c_cmd_wait(AT_BAUD_115200_YAP, "OK", BEKLE_2_SN) == 0)
            {
                sim_arayuz->set_baud(SIM800C_HEDEF_BAUD_RATE);

                if(sim800c_cmd_wait(AT_TEST, "OK", BEKLE_1_SN) == 0)
                {
                    sim800c_logf("Modul Baud Rate Degistirildi, Baud Rate->%d", SIM800C_HEDEF_BAUD_RATE);
                    return 0;
                }
                else
                {
                    sim800c_logf("Modul ile %d Baud Rate uzerinden iletisime gecilemedi!!!", SIM800C_HEDEF_BAUD_RATE);
                    return -1;
                }
            }
            else
            {
                sim800c_logf("Modul Baud Rate Degistirelemedi!!!, Baud Rate->%d", SIM800C_DEFAULT_BAUD_RATE);
                return -1;
            }
        }
        else
        {
            sim800c_logf("Module Erisim Saglanamadi!!!");
            return -1;
        }
    }
}


void sim800c_send_command(const char *cmd)
{
    char    buf[160];
    uint8_t idle_mi ;

    portENTER_CRITICAL(&sim_mux);
    idle_mi = (uint8_t)(SIM800C_IDLE == sim_durum);
    if(idle_mi)
    {
        snprintf(sim_son_komut, sizeof(sim_son_komut), "%s", cmd);  // Echo kontrolu icin sakla
        snprintf(buf, sizeof(buf), "%s\r\n", cmd);
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


static void sim800c_process_line(const char *line)
{
    uint32_t dolu_uzunluk;
    uint32_t bos_alan    ;

    if( (NULL != line) && ('\0' != line[0]) )
    {
        if(SIM800C_ECHO_BEKLE == sim_durum)
        {
            if( 0 == strncmp(line, sim_son_komut, strlen(sim_son_komut)) )
            {
                portENTER_CRITICAL(&sim_mux);
                sim_durum = SIM800C_CEVAP_BEKLE;
                portEXIT_CRITICAL(&sim_mux);
                sim800c_logf("Echo alindi: %s", line);
            }
        }
        else if(SIM800C_CEVAP_BEKLE == sim_durum)
        {
            portENTER_CRITICAL(&sim_mux);
            dolu_uzunluk = strlen(sim_cevap);
            bos_alan     = sizeof(sim_cevap) - dolu_uzunluk - 1;
            snprintf(sim_cevap + dolu_uzunluk, bos_alan, "%s\n", line);
            portEXIT_CRITICAL(&sim_mux);
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
        if ( 0 != sim_binary_modu )
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        ret = sim_arayuz->read(&byte, 1, 100);

        if(SIM800C_VERI_VAR == ret)
        {
            if('\r' == byte)
            {
                // Yok Say
            }
            else if('\n' == byte)
            {
                if(false == over_flow)
                {
                    line_buf[line_pos] = '\0';
                    sim800c_process_line(line_buf);
                }
                line_pos  = 0;
                over_flow = false;
            }
            else
            {
                line_buf[line_pos] = byte;
                line_pos++;

                if(line_pos >= sizeof(line_buf))
                {
                    sim800c_logf("Buffer overFlow, satir sifirlandi");
                    line_pos  = 0;
                    over_flow = true;
                }
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


static int sim800c_cmd_wait(const char *cmd, const char *beklenen, uint32_t timeout_ms)
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

    if(sim800c_cmd_wait(AT_HTTP_READ, "OK", BEKLE_5_SN) == 0)
    {
        sim800c_logf("HTTP Veri Okundu");
    }
    else
    {
        sim800c_logf("HTTP Veri Okunamadi!!!");
        sim800c_cmd_wait(AT_HTTP_TERM, "OK", BEKLE_3_SN);
        return geri_donus_degeri;
    }

    json_bas = strchr(sim_cevap, '{');
    json_son = strrchr(sim_cevap, '}');

    if( (NULL != json_bas) && (NULL != json_son) && (json_son > json_bas) )
    {
        uzunluk = (int)(json_son - json_bas) + 1;
        if(uzunluk < out_max)
        {
            memcpy(out_buf, json_bas, uzunluk);
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

    if ( sim800c_cmd_wait(AT_HTTP_GET, "+HTTPACTION:", BEKLE_30_SN) != 0 )
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
    uint8_t   byte              ;
    char      header_buf[64]    ;
    int       header_pos    = 0 ;
    int       beklenen_len  = 0 ;
    int       okunan        = 0 ;
    int       kalan             ;
    int       ret               ;
    uint8_t  *p                 ;

    /* Komut: "AT+HTTPREAD=<offset>,<size>\r\n" — reader_task bypass, manuel gonderim */
    snprintf(komut, sizeof(komut), "%s=%d,%d\r\n", AT_HTTP_READ, offset, size);
    sim_arayuz->send( (const uint8_t *)komut, strlen(komut) );

    /* "+HTTPREAD: N\r\n" basligini ara (echo satirini ve bos satirlari atla) */
    while ( true )
    {
        ret = sim_arayuz->read(&byte, 1, 5000);
        if ( SIM800C_VERI_VAR != ret )
        {
            sim800c_logf("HTTPREAD Header Timeout!!!");
            return -1;
        }

        if ( '\n' == byte )
        {
            header_buf[header_pos] = '\0';

            if ( NULL != strstr(header_buf, "+HTTPREAD:") )
            {
                sscanf(strstr(header_buf, "+HTTPREAD:"), "+HTTPREAD: %d", &beklenen_len);
                break;
            }
            header_pos = 0;
        }
        else if ( '\r' != byte )
        {
            if ( header_pos < (int)sizeof(header_buf) - 1 )
            {
                header_buf[header_pos] = (char)byte;
                header_pos++;
            }
        }
    }

    if ( beklenen_len <= 0 )
    {
        sim800c_logf("HTTPREAD Uzunluk Hatali: %d", beklenen_len);
        return -1;
    }

    if ( beklenen_len > size )
    {
        sim800c_logf("Beklenenden Fazla Binary: %d > %d", beklenen_len, size);
        return -1;
    }

    /* Binary'i toplu halde out_buf'a kopyala */
    p     = out_buf;
    kalan = beklenen_len;

    while ( kalan > 0 )
    {
        ret = sim_arayuz->read(p, kalan, 5000);
        if ( ret <= 0 )
        {
            sim800c_logf("Binary Okuma Timeout: okunan=%d beklenen=%d", okunan, beklenen_len);
            return -1;
        }
        p      += ret;
        okunan += ret;
        kalan  -= ret;
    }

    *out_len = okunan;
    sim800c_logf("HTTP Chunk Okundu: offset=%d, %d byte", offset, okunan);

    return 0;
}


int sim800c_http_read(int offset, uint8_t *out_buf, int size, int *out_len)
{
    int geri_donus_degeri;

    /* Binary moduna gec; reader_task'in yield'a girmesi icin 150ms bekle
     * (read timeout'u 100ms, bir sonraki iterasyonda flag'i gorur) */
    sim_binary_modu = 1;
    vTaskDelay(pdMS_TO_TICKS(150));

    geri_donus_degeri = sim800c_http_read_adimlari(offset, out_buf, size, out_len);

    sim_binary_modu = 0;

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
