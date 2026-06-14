#include <string.h>
#include <stdio.h>
#include "mqtt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

/* =============== Sabitler =============== */
#define MQTT_TAG                  "MQTT"
#define MQTT_PACKET_BUFFER_SIZE   (256U  )
#define MQTT_CONNACK_TIMEOUT_MS   (15000U)

#define MQTT_RX_BUFFER_SIZE       (512U  )
#define MQTT_RX_TOPIC_SIZE        (128U  )
#define MQTT_RX_PAYLOAD_SIZE      (256U  )
#define MQTT_RX_TASK_STACK        (3072U )
#define MQTT_RX_TASK_PRIORITY     (5U    )


/* =============== Static Degiskenler =============== */
static const mqtt_transport_t *fp_transport_st                             = NULL; /* Inject edilen transport */
static       uint8_t           mqtt_packet_buffer[MQTT_PACKET_BUFFER_SIZE]       ; /* Paket insa tamponu */

/* ─────────── Receiver task icin static state ─────────── */
static mqtt_message_callback_t fp_message_callback        = NULL;   /* Kullanici handler'i */
static TaskHandle_t            mqtt_receiver_task_handle  = NULL;   /* Receiver task handle */




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
        mqtt_log("MQTT Katmani hazir.");
        mqtt_return_et = MQTT_OK;
    }

return mqtt_return_et;
}

mqtt_return_t mqtt_connect(const char *p_host_ch    , uint16_t d_port_u16          ,
                           const char *p_client_id  , uint16_t d_keep_alive_sec_u16)
{
    mqtt_return_t mqtt_return_et         = MQTT_ERROR;
    uint16_t      d_client_id_len_u16    = 0         ;
    uint16_t      d_remaining_length_u16 = 0         ;
    uint16_t      d_pkt_idx_u16          = 0         ;
    uint8_t       connack_buffer_au8[4]  = {0}       ;
    int           d_recv_count           = 0         ;


    if ( (NULL != fp_transport_st) && (NULL != p_host_ch) && (NULL != p_client_id) )
    {
        d_client_id_len_u16 = (uint16_t)strlen(p_client_id);

        if ( 0U < d_client_id_len_u16 )
        {
            /*================ 1. TCP'yi ac ================*/
            if ( 0 == fp_transport_st->tcp_open(p_host_ch, d_port_u16) )
            {
                mqtt_log("TCP baglandi, MQTT CONNECT gonderiliyor...");

                /*================ 2. CONNECT paketini insa et ================*/
                d_remaining_length_u16 = 10U + 2U + d_client_id_len_u16;

                /* Fixed Header */
                mqtt_packet_buffer[d_pkt_idx_u16++] = 0x10U;
                mqtt_packet_buffer[d_pkt_idx_u16++] = (uint8_t)d_remaining_length_u16;

                /* Variable Header */
                mqtt_packet_buffer[d_pkt_idx_u16++] = 0x00U;
                mqtt_packet_buffer[d_pkt_idx_u16++] = 0x04U;
                mqtt_packet_buffer[d_pkt_idx_u16++] = 'M';
                mqtt_packet_buffer[d_pkt_idx_u16++] = 'Q';
                mqtt_packet_buffer[d_pkt_idx_u16++] = 'T';
                mqtt_packet_buffer[d_pkt_idx_u16++] = 'T';
                mqtt_packet_buffer[d_pkt_idx_u16++] = 0x04U;
                mqtt_packet_buffer[d_pkt_idx_u16++] = 0x02U;
                mqtt_packet_buffer[d_pkt_idx_u16++] = (uint8_t)(d_keep_alive_sec_u16 >> 8);
                mqtt_packet_buffer[d_pkt_idx_u16++] = (uint8_t)(d_keep_alive_sec_u16 & 0xFFU);

                /* Payload: Client ID */
                mqtt_packet_buffer[d_pkt_idx_u16++] = (uint8_t)(d_client_id_len_u16 >> 8   );
                mqtt_packet_buffer[d_pkt_idx_u16++] = (uint8_t)(d_client_id_len_u16 & 0xFFU);
                memcpy(&mqtt_packet_buffer[d_pkt_idx_u16], p_client_id, d_client_id_len_u16);
                d_pkt_idx_u16 += d_client_id_len_u16;

                /*================ 3. CONNECT'i yolla ================*/
                if ( 0 == fp_transport_st->send(mqtt_packet_buffer, d_pkt_idx_u16) )
                {
                    /*================ 4. CONNACK bekle ================*/
                    d_recv_count = fp_transport_st->receive(connack_buffer_au8, sizeof(connack_buffer_au8), MQTT_CONNACK_TIMEOUT_MS);

                    if ( 4 <= d_recv_count )
                    {
                        /*================ 5. CONNACK dogrula ================*/
                        if ( (0x20U == connack_buffer_au8[0]) && (0x00U == connack_buffer_au8[3]) )
                        {
                            mqtt_log(">>> MQTT broker baglandi <<<");
                            mqtt_return_et = MQTT_OK;
                        }
                        else
                        {
                            mqtt_log("CONNACK return code != 0");
                            fp_transport_st->tcp_close();    /* TCP'yi temizle */
                        }
                    }
                    else
                    {
                        mqtt_log("CONNACK gelmedi (timeout)");
                        fp_transport_st->tcp_close();        /* TCP'yi temizle */
                    }
                }
                else
                {
                    mqtt_log("CONNECT send basarisiz");
                    fp_transport_st->tcp_close();            /* TCP'yi temizle */
                }
            }
            else
            {
                mqtt_log("TCP open basarisiz");
                /* TCP zaten acilmadi, tcp_close gereksiz */
            }
        }
    }

    return mqtt_return_et;
}


mqtt_return_t mqtt_publish(const char *p_topic_ch, const uint8_t *p_payload_u8, uint16_t d_payload_len_u16)
{
    mqtt_return_t mqtt_return_et         = MQTT_ERROR;
    uint16_t      d_topic_len_u16        = 0         ;
    uint16_t      d_remaining_length_u16 = 0         ;
    uint16_t      d_pkt_idx_u16          = 0         ;


    if( (NULL != fp_transport_st) && (NULL != p_topic_ch) && (NULL != p_payload_u8) )
    {

        d_topic_len_u16 = (uint16_t)strlen(p_topic_ch);

        if( ( 0U < d_topic_len_u16) && (0U < d_payload_len_u16) )
        {
            /*================ PUBLISH paketini insa et ================*/
            /* Remaining length = topic_len_field(2) + topic_string + payload */
            /* Qos=0 olduğu için packet_id yok */
            d_remaining_length_u16 = 2U + d_topic_len_u16 + d_payload_len_u16;

            if( MQTT_PACKET_BUFFER_SIZE >= (d_remaining_length_u16 + 2U) )
            {
                /* Fixed Header */
                mqtt_packet_buffer[d_pkt_idx_u16++] = 0x30U;                                  /* PUBLISH, QoS=0, DUP=0, RETAIN=0 */
                mqtt_packet_buffer[d_pkt_idx_u16++] = (uint8_t)d_remaining_length_u16;        /* VLE: tek byte */

                /* Variable Header: Topic */
                mqtt_packet_buffer[d_pkt_idx_u16++] = (uint8_t)(d_topic_len_u16 >> 8);        /* Topic length MSB */
                mqtt_packet_buffer[d_pkt_idx_u16++] = (uint8_t)(d_topic_len_u16 & 0xFFU);     /* Topic length LSB */

                memcpy(&mqtt_packet_buffer[d_pkt_idx_u16], p_topic_ch, d_topic_len_u16);
                d_pkt_idx_u16 += d_topic_len_u16;

                /* Payload */
                memcpy(&mqtt_packet_buffer[d_pkt_idx_u16], p_payload_u8, d_payload_len_u16);
                d_pkt_idx_u16 += d_payload_len_u16;

                /*================ PUBLISH'i yolla ================*/
                if( 0 == fp_transport_st->send(mqtt_packet_buffer, d_pkt_idx_u16) )
                {
                    mqtt_log(">>> PUBLISH Gonderildi <<<");
                    mqtt_return_et = MQTT_OK;
                }
                else
                {
                    mqtt_log("PUBLISH Send Basarisiz!!!");
                }
            }
            else
            {
                mqtt_log("PUBLISH paket buffer'a sigmiyor");
            }
        }
    }
    return mqtt_return_et;
}


mqtt_return_t mqtt_subscribe(const char *p_topic_ch, uint8_t d_qos_u8)
{
    static  uint16_t      d_next_packet_id_u16    = 1U        ;
            mqtt_return_t mqtt_return_et          = MQTT_ERROR;
            uint16_t      d_topic_length_u16      = 0         ;
            uint16_t      d_remaining_length_u16  = 0         ;
            uint16_t      d_pkt_index_u16         = 0         ;
            uint16_t      d_pkt_id_u16            = 0         ;
            uint8_t       subscribe_ack_buffer_au8[5] = {0}   ;
            int           d_recive_count_i        = 0         ;

    if( (NULL != fp_transport_st) && (NULL != p_topic_ch) && (d_qos_u8 <= 2) )
    {
        d_topic_length_u16 = (uint16_t)strlen(p_topic_ch);

        if(d_topic_length_u16 > 0)
        {
            /*================ Packet ID ata ================*/
            d_pkt_id_u16 = d_next_packet_id_u16;                            /* mevcut ID'yi al */
            d_next_packet_id_u16++;                                         /* sonraki sefere arttır */
            if( 0U == d_next_packet_id_u16 )   d_next_packet_id_u16 = 1U;   /* Packet id 0 olamaz! */

            /*================ SUBSCRIBE paketini insa et ================*/
            /* Remaining length = packet_id(2) + topic_len_field(2) + topic + qos(1) */
            d_remaining_length_u16 = 2U + 2U + d_topic_length_u16 + 1U;

            if( MQTT_PACKET_BUFFER_SIZE >= (d_remaining_length_u16 + 2) )
            {
                /* Fixed Header */
                mqtt_packet_buffer[d_pkt_index_u16++] = 0x82;       /* Subscribe (alt 4 bit 0010 olmak zorunlu) */
                mqtt_packet_buffer[d_pkt_index_u16++] = (uint8_t)(d_remaining_length_u16 & 0x00FFU);

                /* Variable Header: Packet ID */
                mqtt_packet_buffer[d_pkt_index_u16++] = (uint8_t)(d_pkt_id_u16 >> 8);
                mqtt_packet_buffer[d_pkt_index_u16++] = (uint8_t)(d_pkt_id_u16 & 0xFFU);

                /* Payload: Topic */
                mqtt_packet_buffer[d_pkt_index_u16++] = (uint8_t)(d_topic_length_u16 >> 8);
                mqtt_packet_buffer[d_pkt_index_u16++] = (uint8_t)(d_topic_length_u16 & 0xFFU);
                memcpy(&mqtt_packet_buffer[d_pkt_index_u16], p_topic_ch, d_topic_length_u16);
                d_pkt_index_u16 += d_topic_length_u16;

                /* Payload: Requested QoS */
                mqtt_packet_buffer[d_pkt_index_u16++] = d_qos_u8;

                mqtt_log("SUBSCRIBE paketi gonderiliyor...");

                /*================ SUBSCRIBE'i yolla ================*/
                if( 0 == fp_transport_st->send(mqtt_packet_buffer, d_pkt_index_u16) )
                {
                    /*================ SUBACK bekle (5 byte: 0x90 0x03 pkt_id_msb pkt_id_lsb ret_code) ================*/
                    d_recive_count_i = fp_transport_st->receive(subscribe_ack_buffer_au8, sizeof(subscribe_ack_buffer_au8), MQTT_CONNACK_TIMEOUT_MS);

                    if( 5 <= d_recive_count_i)
                    {
                        uint16_t d_sub_ack_packet_id_u16 = ((uint16_t)subscribe_ack_buffer_au8[2] << 8) | (uint16_t)subscribe_ack_buffer_au8[3];

                        if( (0x90         == subscribe_ack_buffer_au8[0]    ) &&
                            (d_pkt_id_u16 == d_sub_ack_packet_id_u16        ) &&
                            (0x80         != subscribe_ack_buffer_au8[4]    )    )
                        {
                            mqtt_log(">>> SUBSCRIBE basarili <<<");
                            mqtt_return_et = MQTT_OK;
                        }
                        else
                        {
                            mqtt_log("SUBACK reddedildi veya bozuk");
                        }
                        
                    }
                    else
                    {
                    mqtt_log("SUBACK gelmedi (timeout)");
                    }
                }
                else
                {
                    mqtt_log("SUBSCRIBE send basarisiz");
                }
            }
            else
            {
                mqtt_log("SUBSCRIBE paketi buffer'a sigmiyor");
            }
        }
    }

return mqtt_return_et;
}


///* ─────────── Receiver task — arka planda PUBLISH dinler ─────────── */
//static void mqtt_receiver_task(void *p_arg)
//{
//    static  uint8_t  rx_buffer_au8 [MQTT_RX_BUFFER_SIZE ];
//    static  char     rx_topic_ch   [MQTT_RX_TOPIC_SIZE  ];
//    static  uint8_t  rx_payload_au8[MQTT_RX_PAYLOAD_SIZE];
//
//            int      d_recv_count      ;
//            uint8_t  d_control_byte_u8 ;
//            uint8_t  d_remaining_len_u8;
//            uint16_t d_topic_len_u16   ;
//            uint16_t d_payload_len_u16 ;
//
//    (void)p_arg;
//
//    mqtt_log("Receiver task baslatildi");
//
//    while ( true )
//    {
//        /* 1. Control byte oku (blocking, sonsuz bekle) */
//        d_recv_count = fp_transport_st->receive(&d_control_byte_u8, 1U, portMAX_DELAY);
//
//        if ( 1 != d_recv_count )                              continue;   /* veri yok, devam et */
//        if ( 0x30U != (d_control_byte_u8 & 0xF0U) )           continue;   /* PUBLISH degil (sadece 0x3X islenir) */
//
//        /* 2. Remaining length (VLE - simdilik tek byte varsay) */
//        d_recv_count = fp_transport_st->receive(&d_remaining_len_u8, 1U, 1000U);
//        if ( 1 != d_recv_count )                              continue;
//
//        /* 3. Tum paketi al */
//        if ( d_remaining_len_u8 > sizeof(rx_buffer_au8) )
//        {
//            mqtt_log("PUBLISH paketi buffer'dan buyuk, atlandi");
//            continue;
//        }
//
//        d_recv_count = fp_transport_st->receive(rx_buffer_au8, d_remaining_len_u8, 1000U);
//        if ( d_remaining_len_u8 != d_recv_count )             continue;
//
//        /* 4. Topic length (big-endian) */
//        d_topic_len_u16 = ((uint16_t)rx_buffer_au8[0] << 8) | (uint16_t)rx_buffer_au8[1];
//
//        if ( d_topic_len_u16 >= sizeof(rx_topic_ch) )         continue;
//
//        /* 5. Topic string'i kopyala */
//        memcpy(rx_topic_ch, &rx_buffer_au8[2], d_topic_len_u16);
//        rx_topic_ch[d_topic_len_u16] = '\0';
//
//        /* 6. Payload (QoS=0 oldugu icin packet_id yok) */
//        d_payload_len_u16 = d_remaining_len_u8 - 2U - d_topic_len_u16;
//
//        if ( d_payload_len_u16 >= sizeof(rx_payload_au8) )    continue;
//
//        memcpy(rx_payload_au8, &rx_buffer_au8[2 + d_topic_len_u16], d_payload_len_u16);
//
//        /* 7. Callback'i cagir */
//        if ( NULL != fp_message_callback )
//        {
//            fp_message_callback(rx_topic_ch, rx_payload_au8, d_payload_len_u16);
//        }
//    }
//}


mqtt_return_t mqtt_set_message_callback(mqtt_message_callback_t fp_callback)
{
    mqtt_return_t mqtt_return_et = MQTT_ERROR;

    if ( NULL != fp_callback )
    {
        fp_message_callback = fp_callback;
        mqtt_return_et = MQTT_OK;
    }

    return mqtt_return_et;
}


mqtt_return_t mqtt_disconnect(void)
{
    mqtt_return_t mqtt_return_et = MQTT_ERROR;
    const uint8_t disconnect_pkt_au8[2]  = { 0xE0U, 0x00U };

    if( NULL != fp_transport_st)
    {
        if( 0 == fp_transport_st->send(disconnect_pkt_au8, sizeof(disconnect_pkt_au8)) )
        {
            mqtt_log("DISCONNECT Gonderildi.");
            mqtt_return_et = MQTT_OK;
        }
        else
        {
            mqtt_log("DISCONNECT Send Basarisiz!!!");
        }

        fp_transport_st->tcp_close();
    }

    return mqtt_return_et;
}