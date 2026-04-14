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
static sim800c_io_t   *sim_arayuz       = NULL        ;   // inject edilen uart arayüzü
static sim800c_state_t sim_durum        = SIM800C_IDLE;   // mevcut state
static char            sim_cevap[4096]                ;   // gelen cevap buffer'ı (cok satirli cevaplar icin)
static char            sim_son_komut[128]             ;   // gönderilen son komut (echo karşılaştırması için)

/* ──────────────────── Static Fonksiyonlar ─────────────────────── */
static void sim800c_reader_task(void *arg);
static void sim800c_logf(const char *fmt, ...);
static void sim800c_process_line(const char *line);
static int  sim800c_cmd_wait(const char *cmd, uint32_t timeout_ms);


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
    sim_arayuz    = io;
    sim_durum = SIM800C_IDLE;
    memset(sim_cevap, 0, sizeof(sim_cevap));
    memset(sim_son_komut, 0, sizeof(sim_son_komut));

    xTaskCreate(sim800c_reader_task, "sim800c_reader", 2048, NULL, 5, NULL);
}

int sim800c_baslat(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));    // Burada sim800c_reader_taskin ayagi kalkmasi bekleniyor.

    if (sim800c_cmd_wait(AT_TEST, 1000) == 0)
    {
        sim800c_logf("Modul baslatildi, Baud Rate->%d", SIM800C_HEDEF_BAUD_RATE);
        return 0;
    }
    else
    {
        sim800c_logf("Modül Baud Rate->%d'de cevap vermedi. Baud Rate'yi %d'e çek:", SIM800C_HEDEF_BAUD_RATE, SIM800C_DEFAULT_BAUD_RATE);
        sim_arayuz->set_baud(SIM800C_DEFAULT_BAUD_RATE);
        
        if(sim800c_cmd_wait(AT_TEST, 1000) == 0)
        {
            sim800c_logf("Modul baslatildi, Baud Rate->%d", SIM800C_DEFAULT_BAUD_RATE);

            if(sim800c_cmd_wait(AT_BAUD_115200_YAP, 2000) == 0)
            {
                sim_arayuz->set_baud(SIM800C_HEDEF_BAUD_RATE);

                if(sim800c_cmd_wait(AT_TEST, 1000) == 0)
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
    char buf[160];
    if(SIM800C_IDLE == sim_durum)
    {
        snprintf(sim_son_komut, sizeof(sim_son_komut), "%s", cmd);  // Echo kontrolu icin sakla
        snprintf(buf, sizeof(buf), "%s\r\n", cmd);

        sim_arayuz->send( (const uint8_t *)buf, strlen(buf) );
        sim_durum = SIM800C_ECHO_BEKLE;

        sim800c_logf("Komut gonderildi: %s", cmd);
    }
    else 
    {
        sim800c_logf("Onceki komut bitmedi, gonderilmedi: %s", cmd);
    }
}

static void sim800c_process_line(const char *line)
{
    if( (NULL != line) && ('\0' != line[0]))
    {
        if(SIM800C_ECHO_BEKLE == sim_durum)
        {
            if( 0 == strncmp(line, sim_son_komut, strlen(sim_son_komut)) )
            {
                sim_durum = SIM800C_CEVAP_BEKLE;
                sim800c_logf("Echo alindi: %s", line);
            }
        }
        else if(SIM800C_CEVAP_BEKLE == sim_durum)
        {
            if( 0 == strcmp(line, "OK") )
            {
                sim_durum = SIM800C_CEVAP_HAZIR;
                sim800c_logf("Cevap hazir");
            }
            else if ( 0 == strcmp(line, "ERROR") )
            {
                sim_durum = SIM800C_HATA;
                sim800c_logf("Hata alindi");
            }
            else
            {
                uint32_t dolu_uzunluk = strlen(sim_cevap);
                uint32_t bos_alan     = sizeof(sim_cevap) - dolu_uzunluk - 1;
                snprintf(sim_cevap + dolu_uzunluk, bos_alan, "%s\n", line);
                sim800c_logf("Veri satiri: %s", line);
            }
        }
    }
}

static void sim800c_reader_task(void *arg)
{
    uint8_t  byte              ;
    char     line_buf[256]     ;
    uint16_t line_pos      = 0 ;
    uint8_t  over_flow     = 0 ;

    while(true)
    {
        int ret = sim_arayuz->read(&byte, 1, 100);

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

static int sim800c_cmd_wait(const char *cmd, uint32_t timeout_ms)
{
    memset(sim_cevap, 0, sizeof(sim_cevap));  // her komut oncesi buffer sifirla
    sim_durum = SIM800C_IDLE;

    sim800c_send_command(cmd);

    uint32_t gecen_sure = 0;
    while (gecen_sure < timeout_ms)
    {
        if (SIM800C_CEVAP_HAZIR == sim_durum)
        {
            sim_durum = SIM800C_IDLE;
            return 0;
        }
        if (SIM800C_HATA == sim_durum)
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

const char *sim800c_get_response(void)
{
    return sim_cevap;
}

int sim800c_gprs_connect(void)
{
    int  geri_donus_degeri = -1;
    char apn_komutu[64];    

    if(sim800c_cmd_wait(AT_GPRS_ATTACH, 5000) == 0)
    {
        sim800c_logf("GPRS'e Baglanildi");
    }
    else
    {
        sim800c_logf("GPRS'e Baglanilamadi!!!");
        return geri_donus_degeri;
    }                                                

    sim800c_logf("Mevcut Bearer durumu temizleniyor...");
    sim800c_cmd_wait(AT_BEARER_CLOSE, 3000);                // Islemci reset yediyse mesela baglanti onceden kalir ve bu sebepten ERROR cevabi alina bilir ? 

    if(sim800c_cmd_wait(AT_BEARER_OPEN, 3000) == 0)  
    {
        sim800c_logf("Bearer Tipi: GPRS");
    }
    else
    {
        sim800c_logf("Bearer Tipi Ayarlanamadi!!!");
        return geri_donus_degeri;
    }

    snprintf(apn_komutu, sizeof(apn_komutu), AT_BEARER_APN, "internet");
    if(sim800c_cmd_wait(apn_komutu, 3000) == 0) 
    {
        sim800c_logf("APN Ayarlandi");
    }
    else
    {
        sim800c_logf("APN Ayarlanamadi!!!");
        return geri_donus_degeri;
    }

    // 100 saniye (100000 ms) olarak güncellediğiniz yer
    if(sim800c_cmd_wait(AT_BEARER_START, 100000) == 0)
    {
        sim800c_logf("Bearer Baslatildi");
    }
    else
    {
        sim800c_logf("Bearer Baslatilamadi!!!");
        return geri_donus_degeri;
    }

    if(sim800c_cmd_wait(AT_BEARER_QUERY, 3000) == 0) 
    {
        sim800c_logf("IP adresi: %s", sim_cevap); // Parsing şimdilik atlandı
    }
    else
    {
        sim800c_logf("IP Ayarlanamadi!!!");
        return geri_donus_degeri;
    }

    geri_donus_degeri = 0;
    return geri_donus_degeri;
}