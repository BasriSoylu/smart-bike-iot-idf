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
static sim800c_io_t   *s_io       = NULL;         // inject edilen uart arayüzü
static sim800c_state_t s_state    = SIM800C_IDLE; // mevcut state
static char            s_response[512];           // gelen cevap buffer'ı
static char            s_last_cmd[128];           // gönderilen son komut (echo karşılaştırması için)

/* ──────────────────── Static Fonksiyonlar ─────────────────────── */
static void sim800c_reader_task(void *arg);
static void sim800c_logf(const char *fmt, ...);
static void sim800c_process_line(const char *line);


/* ──────────────────── Dahili Log Yardimcisi ─────────────────── */
static void sim800c_logf(const char *fmt, ...)
{
    if ( (NULL != s_io) && (NULL != s_io->log) )
    {
        char message[256];
        char message_out[280];
        va_list args;

        va_start(args, fmt);
        vsnprintf(message, sizeof(message), fmt, args);
        va_end(args);

        snprintf(message_out, sizeof(message_out), "[SIM800C] %s", message);

        s_io->log(message_out);
    } 
}

/* ─────────────────── Public API Implementasyonlari ──────────────────── */
void sim800c_init(sim800c_io_t *io)
{
    s_io    = io;
    s_state = SIM800C_IDLE;
    memset(s_response, 0, sizeof(s_response));
    memset(s_last_cmd, 0, sizeof(s_last_cmd));
    sim800c_logf("Modul baslatildi");

    xTaskCreate(sim800c_reader_task, "sim800c_reader", 2048, NULL, 5, NULL);
}

void sim800c_send_command(const char *cmd)
{
    char buf[160];
    if(SIM800C_IDLE == s_state)
    {
        snprintf(s_last_cmd, sizeof(s_last_cmd), "%s", cmd);  // Echo kontrolu icin sakla
        snprintf(buf, sizeof(buf), "%s\r\n", cmd);

        s_io->send( (const uint8_t *)buf, strlen(buf) );
        s_state = SIM800C_ECHO_BEKLE;

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
        if(SIM800C_ECHO_BEKLE == s_state)
        {
            if( 0 == strncmp(line, s_last_cmd, strlen(s_last_cmd)) )
            {
                s_state = SIM800C_CEVAP_BEKLE;
                sim800c_logf("Echo alindi: %s", line);
            }
        }
        else if(SIM800C_CEVAP_BEKLE == s_state)
        {
            if( 0 == strcmp(line, "OK") )
            {
                s_state = SIM800C_CEVAP_HAZIR;
                sim800c_logf("Cevap hazir");
            }
            else if ( 0 == strcmp(line, "ERROR") )
            {
                s_state = SIM800C_HATA;
                sim800c_logf("Hata alindi");
            }
            else
            {
                snprintf(s_response, sizeof(s_response), "%s", line);
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
        int ret = s_io->read(&byte, 1, 100);

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
    return s_state;
}

const char *sim800c_get_response(void)
{
    return s_response;
}

