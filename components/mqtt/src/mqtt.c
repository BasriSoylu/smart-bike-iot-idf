#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "mqtt.h"
#include "mqtt_pkt.h"

/* =============== Sabitler =============== */
#define MQTT_TAG                        ("MQTT")
#define MQTT_PACKET_BUFFER_SIZE         (256U  )
#define MQTT_CONNACK_TIMEOUT_MS         (15000U)
#define MQTT_ACK_TIMEOUT_MS             (10000U)
#define MQTT_ACK_PAKET_MAX_UZUNLUGU     (8U    )   // En buyuk ACK = SUBACK (5 byte), pay birakildi
#define MQTT_ACK_KUYRUK_DERINLIGI       (4U    )   // Ayni anda beklenen ACK sayisi az, 4 yeterli
#define MQTT_OKUYUCU_BEKLEME_MS         (1000U )   // Okuyucu task'in her dinleme turunun suresi (dongu nabzi)

#define MQTT_RX_BUFFER_SIZE             (512U  )
#define MQTT_RX_TASK_STACK              (3072U )
#define MQTT_RX_TASK_PRIORITY           (5U    )

#define MQTT_MAX_TOPIC_SAYISI           (8U    )

/*=================== Typedef ===================*/
typedef struct
{
    uint8_t data_au8[MQTT_ACK_PAKET_MAX_UZUNLUGU];   /* ham ACK byte'lari */
    uint8_t d_len_u8                             ;   /* kac byte gecerli  */
}mqtt_ack_kuyruk_eleman_t;

typedef struct
{
    const char           *p_topic_ch;
    uint8_t               d_qos_u8  ;
    mqtt_topic_handler_t  fp_handler;
}mqtt_topic_tablosu_eleman_t;

/* ──────────────────────────────────────── Static Degiskenler ──────────────────────────────────────── */
static const mqtt_transport_t            *fp_transport_st                             = NULL;
static       uint8_t                      mqtt_packet_buffer[MQTT_PACKET_BUFFER_SIZE]       ;
static       uint16_t                     s_next_packet_id_u16                        = 1U  ;   /* QoS>0 paket kimligi, 0 gecersiz */
static       QueueHandle_t                mqtt_ack_kuyrugu                            = NULL;
static       mqtt_topic_tablosu_eleman_t  topic_tablosu_ast[MQTT_MAX_TOPIC_SAYISI]          ;
static       SemaphoreHandle_t            mqtt_tx_mutex                               = NULL;
static       TaskHandle_t                 mqtt_okuyucu_task_handle                    = NULL;   /* okuyucu task kimligi (calisiyorsa != NULL) */

/* =========================================================================================================== */
/* ================================= Static Fonksiyon Prototipleri =========================================== */
/* =========================================================================================================== */
/* ───────────────────────────────── Encode Fonksiyon Prototipleri ─────────────────────────────────────────── */ 
static int encode_remaining_length    (      uint8_t *p_buf_u8 ,       uint32_t              d_value_u32     ); 
static int encode_string              (      uint8_t *p_buf_u8 , const char                 *p_str_ch        ); 
static int encode_connect_packet      (      uint8_t *p_buf_u8 , const connect_packet_t     *p_pkt_st        ); 
static int encode_publish_packet      (      uint8_t *p_buf_u8 , const publish_packet_t     *p_pkt_st        ); 
static int encode_subscribe_packet    (      uint8_t *p_buf_u8 , const subscribe_packet_t   *p_pkt_st        ); 
static int encode_unsubscribe_packet  (      uint8_t *p_buf_u8 , const unsubscribe_packet_t *p_pkt_st        ); 
static int encode_ack_packet          (      uint8_t *p_buf_u8 , const ack_packet_t         *p_pkt_st        ); 
static int encode_control_packet      (      uint8_t *p_buf_u8 , const control_packet_t     *p_pkt_st        ); 
/* ───────────────────────────────── Decode Fonksiyon Prototipleri ─────────────────────────────────────────── */
static int decode_remaining_length    (const uint8_t *p_buf_u8 ,       uint32_t             *p_value_u32     );
static int parse_connack_packet       (const uint8_t *p_buf_u8 ,       connack_packet_t     *p_pkt_st        );
static int parse_ack_packet           (const uint8_t *p_buf_u8 ,       ack_packet_t         *p_pkt_st        );
static int parse_suback_packet        (const uint8_t *p_buf_u8 ,       suback_packet_t      *p_pkt_st        );
static int parse_publish_packet       (const uint8_t *p_buf_u8 ,       publish_packet_t     *p_pkt_st        );
/* ───────────────────────────────── Transport Yardimci Fonksiyon Prototipleri ─────────────────────────────── */
static int           receive_packet     (      uint8_t *p_buf_u8    , uint32_t d_timeout_ms_u32  );
static int           send_korumali      (const uint8_t *p_data_u8   , size_t   d_data_length     );
static void          mqtt_topic_dispatch(const char    *p_topic_ch  , uint16_t d_topic_len_u16  , 
                                         const uint8_t *p_payload_u8, uint16_t d_payload_len_u16 );
static mqtt_return_t mqtt_log           (const char    *p_message_ch                             );

/* ───────────────────────────────── Akis (Choreography) Fonksiyon Prototipleri ────────────────────────────── */
static mqtt_return_t mqtt_do_connect  (const mqtt_config_t *p_config_st                                                                           );
static mqtt_return_t mqtt_do_publish  (const char          *p_topic_ch , const uint8_t *p_payload_u8, uint16_t d_payload_len_u16, uint8_t d_qos_u8);
static mqtt_return_t mqtt_do_subscribe(const char          *p_topic_ch ,       uint8_t  d_qos_u8                                                  );

/* =========================================================================================================== */

/* =============== Internal Helpers =============== */
static int encode_remaining_length(uint8_t *p_buf_u8, uint32_t d_value_u32)
{
    int     yazilan_byte_s32 = 0;
    uint8_t byte_u8             ;

    if ( d_value_u32 <= MQTT_MAX_REMAINING_LENGTH)
    {
        do{
            byte_u8     = (uint8_t)(d_value_u32 % 128U);
            d_value_u32 = d_value_u32 / 128;

            if( d_value_u32 > 0U)
            {
                byte_u8 |= 0x80; 
            }

            p_buf_u8[yazilan_byte_s32] = byte_u8;
            yazilan_byte_s32++;

        }while(d_value_u32 > 0U);

    }else   return -1;

    return yazilan_byte_s32;
}

static int encode_string(uint8_t *p_buf_u8, const char *p_str_ch)
{
    int    yazilan_byte_s32 = 0;
    size_t d_str_len_sz        ;

    if ( NULL != p_str_ch )
    {
        d_str_len_sz = strlen(p_str_ch);

        if ( d_str_len_sz <= MQTT_UTF8_STRING_MAX_LEN )
        {
            p_buf_u8[0] = (uint8_t)(d_str_len_sz >> 8     );
            p_buf_u8[1] = (uint8_t)(d_str_len_sz &  0xFFU );

            memcpy(&p_buf_u8[2], p_str_ch, d_str_len_sz);

            yazilan_byte_s32 = (int)(2U + d_str_len_sz);

        }else   return -1;

    }else   return -1;

    return yazilan_byte_s32;
}

static int encode_connect_packet(uint8_t *p_buf_u8, const connect_packet_t *p_pkt_st)
{
    int      d_idx_i             = -1 ;
    uint32_t d_remaining_length_u32   ;

    /* Guard: NULL kontrol */
    if ( (NULL != p_buf_u8) && (NULL != p_pkt_st) )
    {
        d_idx_i = 0;

        /* TODO 1: remaining_length hesabı            */
        d_remaining_length_u32 = (MQTT_VARIABLE_HEADER_SABIT_KISMIN_UZUNLUGU + MQTT_STRING_LEN_ALANI_UZUNLUGU + strlen(p_pkt_st->payload_st.p_client_id_ch) );

        if(NULL != p_pkt_st->payload_st.p_will_topic_ch)
        {
            d_remaining_length_u32 += ( MQTT_STRING_LEN_ALANI_UZUNLUGU + (uint32_t)strlen(p_pkt_st->payload_st.p_will_topic_ch) ); 
            d_remaining_length_u32 += ( MQTT_STRING_LEN_ALANI_UZUNLUGU + p_pkt_st->payload_st.d_will_payload_len_u16 ); 
        }

        if(NULL != p_pkt_st->payload_st.p_username_ch)
        {
            d_remaining_length_u32 += ( MQTT_STRING_LEN_ALANI_UZUNLUGU + (uint32_t)strlen(p_pkt_st->payload_st.p_username_ch) );
        }

        if(NULL != p_pkt_st->payload_st.p_password_ch)
        {
            d_remaining_length_u32 += ( MQTT_STRING_LEN_ALANI_UZUNLUGU + (uint32_t)strlen(p_pkt_st->payload_st.p_password_ch) );
        }

        /* TODO 2: Control byte yaz                   */
        p_buf_u8[d_idx_i++] = p_pkt_st->fixed_header_st.control_byte_ut.byte_u8;
        
        /* TODO 3: Remaining length yaz (VLE)         */
        d_idx_i += encode_remaining_length(&p_buf_u8[d_idx_i], d_remaining_length_u32);

        /* TODO 4: Variable header yaz                */
        p_buf_u8[d_idx_i++] = (uint8_t)(p_pkt_st->var_header_st.d_protocol_name_len_u16 >> 8    );
        p_buf_u8[d_idx_i++] = (uint8_t)(p_pkt_st->var_header_st.d_protocol_name_len_u16 & 0xFFU );

        memcpy(&p_buf_u8[d_idx_i], p_pkt_st->var_header_st.protocol_name_ch, MQTT_PROTOCOL_NAME_UZUNLUGU);
        d_idx_i += MQTT_PROTOCOL_NAME_UZUNLUGU;

        p_buf_u8[d_idx_i++] = p_pkt_st->var_header_st.d_protocol_level_u8;
        p_buf_u8[d_idx_i++] = p_pkt_st->var_header_st.flags_st.connect_flags_ut.byte_u8;

        p_buf_u8[d_idx_i++] = (uint8_t)(p_pkt_st->var_header_st.d_keep_alive_u16 >> 8   );
        p_buf_u8[d_idx_i++] = (uint8_t)(p_pkt_st->var_header_st.d_keep_alive_u16 & 0xFFU);

        /* TODO 5: Payload — Client ID                */
        d_idx_i += encode_string(&p_buf_u8[d_idx_i], p_pkt_st->payload_st.p_client_id_ch);

        /* TODO 6: Payload — LWT (varsa)              */
        if ( NULL != p_pkt_st->payload_st.p_will_topic_ch )
        {
            d_idx_i += encode_string(&p_buf_u8[d_idx_i], p_pkt_st->payload_st.p_will_topic_ch);

            p_buf_u8[d_idx_i++] = (uint8_t)(p_pkt_st->payload_st.d_will_payload_len_u16 >> 8   );
            p_buf_u8[d_idx_i++] = (uint8_t)(p_pkt_st->payload_st.d_will_payload_len_u16 & 0xFFU);

            memcpy(&p_buf_u8[d_idx_i], p_pkt_st->payload_st.p_will_payload_u8, p_pkt_st->payload_st.d_will_payload_len_u16);
            d_idx_i += p_pkt_st->payload_st.d_will_payload_len_u16;
        }

        /* TODO 7: Payload — Username/Password (varsa)*/
        if ( NULL != p_pkt_st->payload_st.p_username_ch )
        {
            d_idx_i += encode_string(&p_buf_u8[d_idx_i], p_pkt_st->payload_st.p_username_ch);
        }

        if ( NULL != p_pkt_st->payload_st.p_password_ch )
        {
            d_idx_i += encode_string(&p_buf_u8[d_idx_i], p_pkt_st->payload_st.p_password_ch);
        }
    }

    return d_idx_i;
}

static int encode_publish_packet(uint8_t *p_buf_u8, const publish_packet_t *p_pkt_st)
{
    int             d_idx_i                 = -1;
    uint32_t        d_remaining_length_u32      ;
    publish_flags_t pflags_st                   ;

    if( (NULL != p_buf_u8) && (NULL != p_pkt_st) )
    {
        d_idx_i = 0;
    
        pflags_st.value_u8 = p_pkt_st->fixed_header_st.control_byte_ut.bits_st.flags;

        /* TODO 1: remaining_length hesabı */
        d_remaining_length_u32 = MQTT_STRING_LEN_ALANI_UZUNLUGU  + (uint32_t)strlen(p_pkt_st->var_header_st.p_topic_ch);
        if( pflags_st.bits_st.qos  > 0U)
        {
            d_remaining_length_u32 += MQTT_PACKET_ID_UZUNLUGU;
        }
        d_remaining_length_u32 += p_pkt_st->payload_st.d_payload_len_u16;

        /* TODO 2: Control byte yaz */
        p_buf_u8[d_idx_i++] = p_pkt_st->fixed_header_st.control_byte_ut.byte_u8;

        /* TODO 3: Remaining length yaz (VLE) */
        d_idx_i += encode_remaining_length(&p_buf_u8[d_idx_i], d_remaining_length_u32);

        /* TODO 4: Topic yaz (encode_string) */
        d_idx_i += encode_string(&p_buf_u8[d_idx_i], p_pkt_st->var_header_st.p_topic_ch);

        /* TODO 5: Packet ID yaz (SADECE QoS > 0 ise) */
        if( pflags_st.bits_st.qos > 0U)
        {
            p_buf_u8[d_idx_i++] = (uint8_t)(p_pkt_st->var_header_st.d_packet_id_u16 >> 8   );
            p_buf_u8[d_idx_i++] = (uint8_t)(p_pkt_st->var_header_st.d_packet_id_u16 & 0xFFU);
        }

        /* TODO 6: Payload yaz (memcpy) */
        memcpy(&p_buf_u8[d_idx_i], p_pkt_st->payload_st.p_payload_u8, p_pkt_st->payload_st.d_payload_len_u16);
        d_idx_i += p_pkt_st->payload_st.d_payload_len_u16;

    }

    return d_idx_i;
}

static int encode_subscribe_packet(uint8_t *p_buf_u8, const subscribe_packet_t *p_pkt_st)
{
    int      d_idx_i                = -1;
    uint32_t d_remaining_length_u32     ;

    if( (NULL != p_buf_u8) && (NULL != p_pkt_st) )
    {
        d_idx_i = 0;

        /* TODO 1: remaining_length hesabı */
        d_remaining_length_u32 =    MQTT_PACKET_ID_UZUNLUGU          + 
                                    MQTT_STRING_LEN_ALANI_UZUNLUGU   + 
                                    MQTT_SUBSCRIBE_QOS_BYTE_UZUNLUGU + 
                                    (uint32_t)strlen(p_pkt_st->topic_filter_st.p_topic_ch);

        /* TODO 2: Control byte yaz */
        p_buf_u8[d_idx_i++] = p_pkt_st->fixed_header_st.control_byte_ut.byte_u8;

        /* TODO 3: Remaining length yaz (VLE) */
        d_idx_i += encode_remaining_length(&p_buf_u8[d_idx_i], d_remaining_length_u32);

        /* TODO 4: Packet ID yaz (HER ZAMAN var, PUBLISH'ten farkı) */
        p_buf_u8[d_idx_i++] = (uint8_t)( (p_pkt_st->d_packet_id_u16 >> 8) & 0xFF ); 
        p_buf_u8[d_idx_i++] = (uint8_t)( (p_pkt_st->d_packet_id_u16     ) & 0xFF );

        /* TODO 5: Topic filter yaz (encode_string) */
        d_idx_i += encode_string(&p_buf_u8[d_idx_i], p_pkt_st->topic_filter_st.p_topic_ch);

        /* TODO 6: QoS byte yaz */
        p_buf_u8[d_idx_i++] = p_pkt_st->topic_filter_st.d_qos_u8;
    }

    return d_idx_i;
}

static int encode_unsubscribe_packet(uint8_t *p_buf_u8, const unsubscribe_packet_t *p_pkt_st)
{
    int      d_idx_i             = -1;
    uint32_t d_remaining_length_u32  ;

    if ( (NULL != p_buf_u8) && (NULL != p_pkt_st) )
    {
        d_idx_i = 0;

        /* TODO 1: remaining_length hesabı */
        d_remaining_length_u32 = MQTT_PACKET_ID_UZUNLUGU + MQTT_STRING_LEN_ALANI_UZUNLUGU + (uint32_t)strlen(p_pkt_st->p_topic_ch);

        /* TODO 2: Control byte yaz */
        p_buf_u8[d_idx_i++] = p_pkt_st->fixed_header_st.control_byte_ut.byte_u8;

        /* TODO 3: Remaining length yaz (VLE) */
        d_idx_i += encode_remaining_length(&p_buf_u8[d_idx_i], d_remaining_length_u32);

        /* TODO 4: Packet ID yaz (HER ZAMAN var) */
        p_buf_u8[d_idx_i++] = (uint8_t)( (p_pkt_st->d_packet_id_u16 >> 8) & 0xFF ); 
        p_buf_u8[d_idx_i++] = (uint8_t)( (p_pkt_st->d_packet_id_u16     ) & 0xFF );

        /* TODO 5: Topic yaz (encode_string) */
        d_idx_i += encode_string(&p_buf_u8[d_idx_i], p_pkt_st->p_topic_ch);

    }

    return d_idx_i;
}

static int encode_ack_packet(uint8_t *p_buf_u8, const ack_packet_t *p_pkt_st)
{
    int d_idx_i = -1;

    if ( (NULL != p_buf_u8) && (NULL != p_pkt_st) )
    {
        d_idx_i = 0;

        /* TODO 1: Control byte yaz */
        p_buf_u8[d_idx_i++] = p_pkt_st->fixed_header_st.control_byte_ut.byte_u8;

        /* TODO 2: Remaining length yaz (VLE) — her zaman = MQTT_PACKET_ID_UZUNLUGU */
        d_idx_i += encode_remaining_length(&p_buf_u8[d_idx_i], MQTT_PACKET_ID_UZUNLUGU);

        /* TODO 3: Packet ID yaz */
        p_buf_u8[d_idx_i++] = (uint8_t)( (p_pkt_st->d_packet_id_u16 >> 8) & 0xFF ); 
        p_buf_u8[d_idx_i++] = (uint8_t)( (p_pkt_st->d_packet_id_u16     ) & 0xFF );
    }

    return d_idx_i;
}

static int encode_control_packet(uint8_t *p_buf_u8, const control_packet_t *p_pkt_st)
{
    int d_idx_i = -1;

    if ( (NULL != p_buf_u8) && (NULL != p_pkt_st) )
    {
        d_idx_i = 0;

        /* TODO 1: Control byte yaz */
        p_buf_u8[d_idx_i++] = p_pkt_st->fixed_header_st.control_byte_ut.byte_u8;

        /* TODO 2: Remaining length yaz — her zaman 0 */
        d_idx_i += encode_remaining_length(&p_buf_u8[d_idx_i], 0U);
    }

    return d_idx_i;
}

static int decode_remaining_length(const uint8_t *p_buf_u8, uint32_t *p_value_u32)
{
    int      d_idx_i        = -1;
    uint32_t d_multiplier_u32   ;
    uint32_t d_value_u32        ;
    uint8_t  byte_u8            ;

    if ( (NULL != p_buf_u8) && (NULL != p_value_u32) )
    {
        d_idx_i        = 0;
        d_multiplier_u32 = 1U;
        d_value_u32      = 0U;

        /* TODO 1: do-while döngüsü ile byte'ları oku ve değeri hesapla */
        do
        {
            byte_u8 = p_buf_u8[d_idx_i];

            d_value_u32 += (uint32_t)(byte_u8 & 0x7FU) * d_multiplier_u32;
            d_multiplier_u32 *= 128U;

            d_idx_i++;

            if ( d_multiplier_u32 > (128U * 128U * 128U) )
            {
                return -1;   /* 4 byte'tan fazla, malformed */
            }
        }
        while ( 0U != (byte_u8 & 0x80U) );

        /* TODO 2: p_value_u32'ye sonucu yaz */
        *p_value_u32 = d_value_u32;
    }

    return d_idx_i;
}

static int parse_connack_packet(const uint8_t *p_buf_u8, connack_packet_t *p_pkt_st)
{
    int d_idx_s32     = -1;

    if ( (NULL != p_buf_u8) && (NULL != p_pkt_st) )
    {
        d_idx_s32 = 0;

        p_pkt_st->fixed_header_st.control_byte_ut.byte_u8 = p_buf_u8[d_idx_s32++];
        if(MQTT_PKT_CONNACK != p_pkt_st->fixed_header_st.control_byte_ut.bits_st.packet_type){ return -1; }

        d_idx_s32 += decode_remaining_length(&p_buf_u8[d_idx_s32],  &p_pkt_st->fixed_header_st.d_remaining_length_u32);
        if(p_pkt_st->fixed_header_st.d_remaining_length_u32 != 2){ return -1; } /* (CONNACK'in remaining_length'i HER ZAMAN 2'dir) */

        p_pkt_st->var_header_st.ack_flags_ut.byte_u8 = p_buf_u8[d_idx_s32++];

        p_pkt_st->var_header_st.d_return_code_u8 = p_buf_u8[d_idx_s32++];
    }

    return d_idx_s32;
}

static int parse_ack_packet(const uint8_t *p_buf_u8, ack_packet_t *p_pkt_st)
{
    int d_idx_s32 = -1;

    if ( (NULL != p_buf_u8) && (NULL != p_pkt_st) )
    {
        d_idx_s32 = 0;

        p_pkt_st->fixed_header_st.control_byte_ut.byte_u8 = p_buf_u8[d_idx_s32++];

        d_idx_s32 += decode_remaining_length(&p_buf_u8[d_idx_s32], &p_pkt_st->fixed_header_st.d_remaining_length_u32);
        if(p_pkt_st->fixed_header_st.d_remaining_length_u32 != 2){ return -1; }

        p_pkt_st->d_packet_id_u16  = (uint16_t)(p_buf_u8[d_idx_s32++] << 8);
        p_pkt_st->d_packet_id_u16 |=            p_buf_u8[d_idx_s32++]      ;
    }

    return d_idx_s32;
}

static int parse_suback_packet(const uint8_t *p_buf_u8, suback_packet_t *p_pkt_st)
{
    int d_idx_s32 = -1;

    if ( (NULL != p_buf_u8) && (NULL != p_pkt_st) )
    {
        d_idx_s32 = 0;

        p_pkt_st->fixed_header_st.control_byte_ut.byte_u8 = p_buf_u8[d_idx_s32++];

        d_idx_s32 += decode_remaining_length(&p_buf_u8[d_idx_s32], &p_pkt_st->fixed_header_st.d_remaining_length_u32);
        if ( (MQTT_PACKET_ID_UZUNLUGU + 1U) != p_pkt_st->fixed_header_st.d_remaining_length_u32 ){ return -1; }

        p_pkt_st->d_packet_id_u16  = (uint16_t)(p_buf_u8[d_idx_s32++] << 8);
        p_pkt_st->d_packet_id_u16 |=            p_buf_u8[d_idx_s32++]      ;

        p_pkt_st->d_return_code_u8 = p_buf_u8[d_idx_s32++];
    }

    return d_idx_s32;
}

static int parse_publish_packet(const uint8_t *p_buf_u8, publish_packet_t *p_pkt_st)
{
    int             d_idx_s32 = -1;
    publish_flags_t flags_ut          ;
    uint16_t        d_qos_u16         ;

    if ( (NULL != p_buf_u8) && (NULL != p_pkt_st) )
    {
        d_idx_s32 = 0;

        p_pkt_st->fixed_header_st.control_byte_ut.byte_u8 = p_buf_u8[d_idx_s32++];
        if ( MQTT_PKT_PUBLISH != p_pkt_st->fixed_header_st.control_byte_ut.bits_st.packet_type ){ return -1; }

        flags_ut.value_u8 = p_pkt_st->fixed_header_st.control_byte_ut.bits_st.flags;
        d_qos_u16          = flags_ut.bits_st.qos;

        d_idx_s32 += decode_remaining_length(&p_buf_u8[d_idx_s32], &p_pkt_st->fixed_header_st.d_remaining_length_u32);

        p_pkt_st->var_header_st.d_topic_len_u16  = (uint16_t)(p_buf_u8[d_idx_s32++] << 8);
        p_pkt_st->var_header_st.d_topic_len_u16 |=            p_buf_u8[d_idx_s32++]      ;

        p_pkt_st->var_header_st.p_topic_ch = (const char *)&p_buf_u8[d_idx_s32];
        d_idx_s32 += p_pkt_st->var_header_st.d_topic_len_u16;

        if ( d_qos_u16 > 0U )
        {
            p_pkt_st->var_header_st.d_packet_id_u16  = (uint16_t)(p_buf_u8[d_idx_s32++] << 8);
            p_pkt_st->var_header_st.d_packet_id_u16 |=            p_buf_u8[d_idx_s32++]      ;
        }

        p_pkt_st->payload_st.d_payload_len_u16 = (uint16_t)(  p_pkt_st->fixed_header_st.d_remaining_length_u32
                                                            - MQTT_STRING_LEN_ALANI_UZUNLUGU
                                                            - p_pkt_st->var_header_st.d_topic_len_u16
                                                            - ( (d_qos_u16 > 0U) ? MQTT_PACKET_ID_UZUNLUGU : 0U ) );

        p_pkt_st->payload_st.p_payload_u8 = &p_buf_u8[d_idx_s32];
        d_idx_s32 += p_pkt_st->payload_st.d_payload_len_u16;
    }

    return d_idx_s32;
}

static int receive_packet(uint8_t *p_buf_u8, uint32_t d_timeout_ms_u32)
{
    int      d_idx_s32 = -1;
    int      d_recv_count_i;
    uint32_t d_remaining_length_u32;

    if ( (NULL != p_buf_u8) && (NULL != fp_transport_st) )
    {
        d_idx_s32 = 0;

        if(MQTT_CONTROL_BYTE_UZUNLUGU != fp_transport_st->receive(&p_buf_u8[d_idx_s32], MQTT_CONTROL_BYTE_UZUNLUGU, d_timeout_ms_u32)){ return -1; }
        d_idx_s32++;

        do
        {
            if ( d_idx_s32 > 4 ) { return -1; }

            if ( 1 != fp_transport_st->receive(&p_buf_u8[d_idx_s32], 1U, d_timeout_ms_u32) ) { return -1; }

            d_idx_s32++;

        } while ( 0U != (p_buf_u8[d_idx_s32 - 1] & MQTT_REMAINING_LENGTH_DEVAM_MASKESI) );

        if ( decode_remaining_length(&p_buf_u8[MQTT_REMAINING_LENGTH_SIRASI], &d_remaining_length_u32) < 0 ) { return -1; }

        d_recv_count_i = fp_transport_st->receive(&p_buf_u8[d_idx_s32], d_remaining_length_u32, d_timeout_ms_u32);
        if ( (int)d_remaining_length_u32 != d_recv_count_i ) { return -1; }

        d_idx_s32 += (int)d_remaining_length_u32;
    }

    return d_idx_s32;
}

static int send_korumali(const uint8_t *p_data_u8, size_t d_data_length)
{
    int d_sonuc_s32 = -1;

    if ( (NULL != fp_transport_st) && (NULL != mqtt_tx_mutex) )
    {
        xSemaphoreTake(mqtt_tx_mutex, portMAX_DELAY);

        d_sonuc_s32 = fp_transport_st->send(p_data_u8, d_data_length);

        xSemaphoreGive(mqtt_tx_mutex);
    }

    return d_sonuc_s32;
}

static mqtt_return_t mqtt_do_connect(const mqtt_config_t *p_config_st)
{
    mqtt_return_t            mqtt_return_et = MQTT_ERROR;
    connect_packet_t         connect_pkt_st = {0};
    connack_packet_t         connack_pkt_st = {0};
    mqtt_ack_kuyruk_eleman_t ack_eleman_st       ;
    int                      d_len_s32           ;

    if ( (NULL != p_config_st) && (NULL != fp_transport_st) && (NULL != mqtt_ack_kuyrugu) )
    {
        connect_pkt_st.fixed_header_st.control_byte_ut.bits_st.packet_type = MQTT_PKT_CONNECT;
        connect_pkt_st.fixed_header_st.control_byte_ut.bits_st.flags       = MQTT_FLAGS_CONNECT;

        connect_pkt_st.var_header_st.d_protocol_name_len_u16                       = 4U                                 ;
        memcpy(connect_pkt_st.var_header_st.protocol_name_ch,                       "MQTT", MQTT_PROTOCOL_NAME_UZUNLUGU);
        connect_pkt_st.var_header_st.d_protocol_level_u8                           = MQTT_VERSION_TYPE                  ;
        connect_pkt_st.var_header_st.d_keep_alive_u16                              = p_config_st->d_keep_alive_sec_u16  ;
        connect_pkt_st.var_header_st.flags_st.connect_flags_ut.bits_st.clean_start = 1U                                 ;

        if(NULL != p_config_st->p_will_topic_ch)
        {
            connect_pkt_st.var_header_st.flags_st.connect_flags_ut.bits_st.will_flag   = 1U;
            connect_pkt_st.var_header_st.flags_st.connect_flags_ut.bits_st.will_qos    = p_config_st->d_will_qos_u8;
            connect_pkt_st.var_header_st.flags_st.connect_flags_ut.bits_st.will_retain = p_config_st->d_will_retain;
        }
        connect_pkt_st.payload_st.p_client_id_ch = p_config_st->p_client_id_ch;

        if(NULL != p_config_st->p_will_topic_ch)
        {
            connect_pkt_st.payload_st.p_will_topic_ch        = p_config_st->p_will_topic_ch       ;
            connect_pkt_st.payload_st.p_will_payload_u8      = p_config_st->p_will_payload_u8     ;
            connect_pkt_st.payload_st.d_will_payload_len_u16 = p_config_st->d_will_payload_len_u16;
        }

        d_len_s32 = encode_connect_packet(mqtt_packet_buffer, &connect_pkt_st);
        if(d_len_s32 < 0){ return MQTT_ERROR; }

        xQueueReset(mqtt_ack_kuyrugu);   /* bayat ACK kalmasin: gondermeden HEMEN once temizle */

        if(0 != send_korumali(mqtt_packet_buffer, (size_t)d_len_s32)){ return MQTT_ERROR; }

        /* Cevabi transport'tan DEGIL, okuyucu task'in doldurdugu kuyruktan bekle */
        if( pdTRUE != xQueueReceive(mqtt_ack_kuyrugu, &ack_eleman_st, pdMS_TO_TICKS(MQTT_CONNACK_TIMEOUT_MS)) ){ return MQTT_ERROR; }

        if(parse_connack_packet(ack_eleman_st.data_au8, &connack_pkt_st) < 0){ return MQTT_ERROR; }

        if(MQTT_CONNACK_ACCEPTED == connack_pkt_st.var_header_st.d_return_code_u8)
        {
            mqtt_return_et = MQTT_OK;
        }
    }

    return mqtt_return_et;
}

static mqtt_return_t mqtt_do_publish(const char *p_topic_ch, const uint8_t *p_payload_u8, uint16_t d_payload_len_u16, uint8_t d_qos_u8)
{
    mqtt_return_t            mqtt_return_et = MQTT_ERROR;
    publish_packet_t         publish_pkt_st = {0}       ;
    ack_packet_t             puback_pkt_st  = {0}       ;
    publish_flags_t          pflags_ut      = {0}       ;
    mqtt_ack_kuyruk_eleman_t ack_eleman_st              ;
    int                      d_len_s32                  ;

    if( (NULL     != p_topic_ch      ) && 
        (NULL     != p_payload_u8    ) && 
        (NULL     != fp_transport_st ) && 
        (NULL     != mqtt_ack_kuyrugu) && 
        (d_qos_u8 <= 1U              )    )
    {
        publish_pkt_st.fixed_header_st.control_byte_ut.bits_st.packet_type = MQTT_PKT_PUBLISH   ;
        pflags_ut.bits_st.qos                                              = d_qos_u8           ;
        publish_pkt_st.fixed_header_st.control_byte_ut.bits_st.flags       = pflags_ut.value_u8 ;

        publish_pkt_st.var_header_st.p_topic_ch = p_topic_ch;

        if ( d_qos_u8 > 0U )
        {
            publish_pkt_st.var_header_st.d_packet_id_u16 = s_next_packet_id_u16;
            s_next_packet_id_u16++;
            if ( 0U == s_next_packet_id_u16 )
            { 
                s_next_packet_id_u16 = 1U;
            }

            xQueueReset(mqtt_ack_kuyrugu);
        }

        publish_pkt_st.payload_st.p_payload_u8      = p_payload_u8     ;
        publish_pkt_st.payload_st.d_payload_len_u16 = d_payload_len_u16;

        d_len_s32 = encode_publish_packet(mqtt_packet_buffer, &publish_pkt_st);
        if(d_len_s32 < 0){ return MQTT_ERROR; }

        if(0 != send_korumali(mqtt_packet_buffer, (size_t)d_len_s32)){ return MQTT_ERROR; }

        if(0 == d_qos_u8)
        {
            mqtt_return_et = MQTT_OK;
            return mqtt_return_et;
        }
        else
        {
            if( pdTRUE == xQueueReceive(mqtt_ack_kuyrugu, &ack_eleman_st, pdMS_TO_TICKS(MQTT_ACK_TIMEOUT_MS)) )
            {
                if( 0 <= parse_ack_packet(ack_eleman_st.data_au8, &puback_pkt_st) )
                {
                    if( (MQTT_PKT_PUBACK               == puback_pkt_st.fixed_header_st.control_byte_ut.bits_st.packet_type) && 
                        (puback_pkt_st.d_packet_id_u16 == publish_pkt_st.var_header_st.d_packet_id_u16                     )    )
                    {
                        mqtt_return_et = MQTT_OK;
                    }
                }
            }
        }
    }
    return mqtt_return_et;
}

static mqtt_return_t mqtt_do_subscribe(const char *p_topic_ch, uint8_t d_qos_u8)
{
    mqtt_return_t            mqtt_return_et   = MQTT_ERROR;
    subscribe_packet_t       subscribe_pkt_st = {0}       ;
    suback_packet_t          suback_pkt_st    = {0}       ;
    mqtt_ack_kuyruk_eleman_t ack_eleman_st                ;
    int                      d_len_s32                    ;

    if( (NULL     != p_topic_ch      ) &&
        (NULL     != fp_transport_st ) &&
        (NULL     != mqtt_ack_kuyrugu) &&
        (d_qos_u8 <= 1U              )    )
    {
        subscribe_pkt_st.fixed_header_st.control_byte_ut.bits_st.packet_type = MQTT_PKT_SUBSCRIBE;
        subscribe_pkt_st.fixed_header_st.control_byte_ut.bits_st.flags       = MQTT_FLAGS_SUBSCRIBE;

        subscribe_pkt_st.d_packet_id_u16 = s_next_packet_id_u16;
        s_next_packet_id_u16++;
        if ( 0U == s_next_packet_id_u16 ){ s_next_packet_id_u16 = 1U; }

        subscribe_pkt_st.topic_filter_st.p_topic_ch = p_topic_ch;
        subscribe_pkt_st.topic_filter_st.d_qos_u8   = d_qos_u8  ;

        d_len_s32 = encode_subscribe_packet(mqtt_packet_buffer, &subscribe_pkt_st);
        if(d_len_s32 < 0){ return MQTT_ERROR; }

        xQueueReset(mqtt_ack_kuyrugu);   /* SUBACK hep beklenir: bayat ACK kalmasin */

        if(0 != send_korumali(mqtt_packet_buffer, (size_t)d_len_s32)){ return MQTT_ERROR; }

        if( pdTRUE == xQueueReceive(mqtt_ack_kuyrugu, &ack_eleman_st, pdMS_TO_TICKS(MQTT_ACK_TIMEOUT_MS)) )
        {
            if( 0 <= parse_suback_packet(ack_eleman_st.data_au8, &suback_pkt_st) )
            {
                if( (MQTT_PKT_SUBACK                  == suback_pkt_st.fixed_header_st.control_byte_ut.bits_st.packet_type) &&
                    (subscribe_pkt_st.d_packet_id_u16 == suback_pkt_st.d_packet_id_u16                                    ) &&
                    (MQTT_SUBACK_FAILURE              != suback_pkt_st.d_return_code_u8                                   )    )
                {
                    mqtt_return_et = MQTT_OK;
                }
            }
        }
    }

    return mqtt_return_et;
}

static mqtt_return_t mqtt_do_ping(void)
{
    mqtt_return_t            mqtt_return_et = MQTT_ERROR;
    control_packet_t         ping_pkt_st    = {0}       ;
    fixed_header_t           resp_hdr_st    = {0}       ;
    mqtt_ack_kuyruk_eleman_t ack_eleman_st              ;
    int                      d_len_s32                  ;

    if ( (NULL != fp_transport_st) && (NULL != mqtt_ack_kuyrugu) )
    {
        ping_pkt_st.fixed_header_st.control_byte_ut.bits_st.packet_type = MQTT_PKT_PINGREQ  ;
        ping_pkt_st.fixed_header_st.control_byte_ut.bits_st.flags       = MQTT_FLAGS_PINGREQ;

        d_len_s32 = encode_control_packet(mqtt_packet_buffer, &ping_pkt_st);
        if(d_len_s32 < 0){ return MQTT_ERROR; }

        xQueueReset(mqtt_ack_kuyrugu);   /* PINGRESP beklenir: bayat ACK kalmasin */

        if(0 != send_korumali(mqtt_packet_buffer, (size_t)d_len_s32)){ return MQTT_ERROR; }

        if( pdTRUE == xQueueReceive(mqtt_ack_kuyrugu, &ack_eleman_st, pdMS_TO_TICKS(MQTT_ACK_TIMEOUT_MS)) )
        {
            resp_hdr_st.control_byte_ut.byte_u8 = ack_eleman_st.data_au8[0];   /* kuyruktan cikan kopyanin ilk byte'i */

            if( MQTT_PKT_PINGRESP == resp_hdr_st.control_byte_ut.bits_st.packet_type )
            {
                mqtt_return_et = MQTT_OK;
            }
        }
    }

    return mqtt_return_et;
}

static void mqtt_okuyucu_task(void *p_arg)
{
    static  uint8_t                  rx_buffer_au8 [MQTT_RX_BUFFER_SIZE ];
            uint8_t                  puback_buf_au8[MQTT_ACK_PAKET_MAX_UZUNLUGU];
            fixed_header_t           hdr_st          = {0};
            publish_packet_t         publish_pkt_st       ;
            ack_packet_t             puback_pkt_st        ;
            mqtt_ack_kuyruk_eleman_t kuyruk_eleman_st     ;
            publish_flags_t          pflags_ut            ;
            int                      d_len_s32            ;

    (void)p_arg;

    while ( true )
    {
        d_len_s32 = receive_packet(rx_buffer_au8, MQTT_OKUYUCU_BEKLEME_MS);

        if ( d_len_s32 > 0 )
        {
            hdr_st.control_byte_ut.byte_u8 = rx_buffer_au8[0];
            if ( MQTT_PKT_PUBLISH == hdr_st.control_byte_ut.bits_st.packet_type )
            {
                if( 0 < parse_publish_packet(rx_buffer_au8, &publish_pkt_st) )
                {
                    mqtt_topic_dispatch(publish_pkt_st.var_header_st.p_topic_ch     ,
                                        publish_pkt_st.var_header_st.d_topic_len_u16,
                                        publish_pkt_st.payload_st.p_payload_u8      ,
                                        publish_pkt_st.payload_st.d_payload_len_u16 );

                    pflags_ut.value_u8 = hdr_st.control_byte_ut.bits_st.flags;

                    if ( pflags_ut.bits_st.qos > 0U )
                    {
                        puback_pkt_st.fixed_header_st.control_byte_ut.bits_st.packet_type = MQTT_PKT_PUBACK                             ;
                        puback_pkt_st.fixed_header_st.control_byte_ut.bits_st.flags       = MQTT_FLAGS_PUBACK                           ;
                        puback_pkt_st.d_packet_id_u16                                     = publish_pkt_st.var_header_st.d_packet_id_u16;

                        d_len_s32 = encode_ack_packet(puback_buf_au8, &puback_pkt_st);

                        if ( d_len_s32 > 0 )
                        {
                            send_korumali(puback_buf_au8, (size_t)d_len_s32);
                        }
                    }
                }
            }
            else
            {
                if ( d_len_s32 <= (int)MQTT_ACK_PAKET_MAX_UZUNLUGU )
                {
                    memcpy(kuyruk_eleman_st.data_au8, rx_buffer_au8, (size_t)d_len_s32);
                    kuyruk_eleman_st.d_len_u8 = (uint8_t)d_len_s32;

                    xQueueSend(mqtt_ack_kuyrugu, &kuyruk_eleman_st, 0);
                }
            }
        }

    }
}


static void mqtt_topic_dispatch(const char *p_topic_ch, uint16_t d_topic_len_u16, const uint8_t *p_payload_u8, uint16_t d_payload_len_u16)
{
    int d_idx_s32;

    for ( d_idx_s32 = 0; d_idx_s32 < MQTT_MAX_TOPIC_SAYISI; d_idx_s32++ )
    {
        if ( NULL != topic_tablosu_ast[d_idx_s32].p_topic_ch ) 
        { 
            if( (d_topic_len_u16 == strlen(topic_tablosu_ast[d_idx_s32].p_topic_ch)                              ) &&
                (0               == strncmp(p_topic_ch, topic_tablosu_ast[d_idx_s32].p_topic_ch, d_topic_len_u16))    )
            {
                if ( NULL != topic_tablosu_ast[d_idx_s32].fp_handler )
                {
                    topic_tablosu_ast[d_idx_s32].fp_handler(p_payload_u8, d_payload_len_u16);
                }
                else
                {
                    mqtt_log("Callback Fonksiyonu Atanmamis Topicden Mesaj Geldi!!!");
                }
                return;
            }
        }
    }
    mqtt_log("Dispatch: bilinmeyen topic'ten mesaj geldi");
}

/* =============== Dahili Log Yardimcisi =============== */
static mqtt_return_t mqtt_log(const char *p_message_ch)
{
    mqtt_return_t mqtt_return_et = MQTT_ERROR;

    if( (NULL != fp_transport_st) && (NULL != fp_transport_st->log))
    {
        fp_transport_st->log(p_message_ch);
        mqtt_return_et = MQTT_OK;
    }
return mqtt_return_et;
}


/* =============== Public API Implementasyonu =============== */
mqtt_return_t mqtt_init(const mqtt_transport_t *p_transport_st)
{
    mqtt_return_t mqtt_return_et = MQTT_ERROR;

    if(NULL != p_transport_st)
    {
        fp_transport_st = p_transport_st;

        if ( NULL == mqtt_tx_mutex )
        {
            mqtt_tx_mutex = xSemaphoreCreateMutex();
        }

        if ( NULL == mqtt_ack_kuyrugu )
        {
            mqtt_ack_kuyrugu = xQueueCreate(MQTT_ACK_KUYRUK_DERINLIGI, sizeof(mqtt_ack_kuyruk_eleman_t));
        }

        if ( (NULL != mqtt_tx_mutex) && (NULL != mqtt_ack_kuyrugu) )
        {
            mqtt_log("MQTT Katmani hazir.");
            mqtt_return_et = MQTT_OK;
        }
    }

return mqtt_return_et;
}

mqtt_return_t mqtt_subscribe(const char *p_topic_ch, uint8_t d_qos_u8, mqtt_topic_handler_t fp_handler)
{
    mqtt_return_t mqtt_return_et = MQTT_ERROR;
    int           d_bos_slot_s32 = -1        ;
    int           d_idx_s32                  ;

    if( (NULL != p_topic_ch) && (NULL != fp_handler) && (d_qos_u8 <= 1U) )
    {
        /* Tabloyu gez: cift kayit var mi + ilk bos slot hangisi */
        for ( d_idx_s32 = 0; d_idx_s32 < MQTT_MAX_TOPIC_SAYISI; d_idx_s32++ )
        {
            if ( NULL != topic_tablosu_ast[d_idx_s32].p_topic_ch )
            {
                if ( 0 == strcmp(p_topic_ch, topic_tablosu_ast[d_idx_s32].p_topic_ch) )
                {
                    mqtt_log("Subscribe: bu topic zaten kayitli!");
                    return MQTT_ERROR;
                }
            }
            else
            {
                if ( -1 == d_bos_slot_s32 )
                {
                    d_bos_slot_s32 = d_idx_s32;   /* ilk bos slotu not et, aramaya devam (cift kayit kontrolu icin) */
                }
            }
        }

        if ( -1 == d_bos_slot_s32 )
        {
            mqtt_log("Subscribe: topic tablosu dolu!");
            return MQTT_ERROR;
        }

        topic_tablosu_ast[d_bos_slot_s32].d_qos_u8   = d_qos_u8  ;
        topic_tablosu_ast[d_bos_slot_s32].fp_handler = fp_handler;
        topic_tablosu_ast[d_bos_slot_s32].p_topic_ch = p_topic_ch;

        if ( MQTT_OK == mqtt_do_subscribe(p_topic_ch, d_qos_u8) )
        {
            mqtt_return_et = MQTT_OK;
        }
        else
        {
            topic_tablosu_ast[d_bos_slot_s32].p_topic_ch = NULL;   /* ROLLBACK: broker kabul etmedi, kayit gecersiz */
        }
    }

    return mqtt_return_et;
}

mqtt_return_t mqtt_publish(const char *p_topic_ch, const uint8_t *p_payload_u8, uint16_t d_payload_len_u16, uint8_t d_qos_u8)
{
    return mqtt_do_publish(p_topic_ch, p_payload_u8, d_payload_len_u16, d_qos_u8);
}

mqtt_return_t mqtt_start(const mqtt_config_t *p_config_st)
{
    mqtt_return_t mqtt_return_et = MQTT_ERROR;
    BaseType_t    task_sonuc                 ;

    if( (NULL != p_config_st             ) &&
        (NULL != fp_transport_st         ) &&
        (NULL != mqtt_ack_kuyrugu        ) &&
        (NULL == mqtt_okuyucu_task_handle)    )
    {
        if( 0 == fp_transport_st->tcp_open(p_config_st->p_host_ch, p_config_st->d_port_u16) )
        {
            task_sonuc = xTaskCreate(mqtt_okuyucu_task, "mqtt_okuyucu", MQTT_RX_TASK_STACK, NULL, MQTT_RX_TASK_PRIORITY, &mqtt_okuyucu_task_handle);
            if(pdPASS == task_sonuc)
            {
                if( MQTT_OK == mqtt_do_connect(p_config_st) )
                {
                    mqtt_return_et = MQTT_OK;
                }
                else
                {
                    vTaskDelete(mqtt_okuyucu_task_handle);
                    mqtt_okuyucu_task_handle = NULL;
                    fp_transport_st->tcp_close();
                }
                
            }
            else
            {
                fp_transport_st->tcp_close();
            }
        }
    }

    return mqtt_return_et;
}