#ifndef MQTT_H
#define MQTT_H

/*=================== Includes ===================*/
#include <stdint.h>
#include <stddef.h>

/*=================== Defines ===================*/
#define MQTT_MAX_REMAINING_LENGTH                    (0xFFFFFFFU)
#define MQTT_UTF8_STRING_MAX_LEN                     (0xFFFFU   )
#define MQTT_VARIABLE_HEADER_SABIT_KISMIN_UZUNLUGU   (10U       )           // protocol name 6 + level 1 + flags 1 + keep_alive 2

#define MQTT_PACKET_ID_UZUNLUGU                      (2U        )           // Packet ID alani (MSB+LSB)
#define MQTT_STRING_LEN_ALANI_UZUNLUGU               (2U        )           // UTF-8 string uzunluk alani (encode_string'in yazdigi ilk 2 byte)
#define MQTT_SUBSCRIBE_QOS_BYTE_UZUNLUGU             (1U        )           // SUBSCRIBE topic filter sonundaki requested QoS byte
#define MQTT_PROTOCOL_NAME_UZUNLUGU                  (4U        )           

/*=================== Typedef ===================*/
typedef enum
{
    MQTT_ERROR = -1,
    MQTT_OK    =  0,
}mqtt_return_t;

typedef struct
{
    const char     *p_host_ch              ;
    uint16_t        d_port_u16             ;
    const char     *p_client_id_ch         ;
    uint16_t        d_keep_alive_sec_u16   ;

    const char     *p_will_topic_ch        ;
    const uint8_t  *p_will_payload_u8      ;
    uint16_t        d_will_payload_len_u16 ;
    uint8_t         d_will_qos_u8          ;
    uint8_t         b_will_retain          ;
}mqtt_config_t;

/*=================== Callback Tipi ===================*/
typedef void (*mqtt_message_callback_t)(const char *p_topic_ch, const uint8_t *p_payload_u8, size_t d_payload_len);


/*======================= Transport Interface ======================= 
 * MQTT katmani altinda TCP'yi nasil konusacagini bilmez.
 * Caller (main.c) sim800c'yi (veya WiFi'i) wrap edip burada verir.
 * Bu sayede mqtt component'i sim800c'den bagimsiz kalir.
 *=================================================================== */
typedef struct 
{
    int  (*tcp_open )(const char    *p_host_ch,   uint16_t d_port_num_u16                              );
    int  (*tcp_close)(void                                                                             );
    int  (*send     )(const uint8_t *p_data_u8,   size_t   d_data_length                               );
    int  (*receive  )(      uint8_t *p_data_u8,   size_t   d_max_data_length, uint32_t d_timeout_ms_u32);
    void (*log      )(const char    *p_message_ch                                                      );
}mqtt_transport_t;


/* =========================== Public API =========================== */
/*====================================== 1. Parametre ========================, 2. Parametre ============== , 3. Parametre ================ , 4. Parametre ================ */
mqtt_return_t mqtt_init                 (const mqtt_transport_t  *fp_transport                                                                                              );
mqtt_return_t mqtt_connect              (const char              *p_host_ch   ,       uint16_t  d_port_u16  , const char *p_client_id       , uint16_t d_keep_alive_sec_u16 );
mqtt_return_t mqtt_publish              (const char              *p_topic_ch  , const uint8_t  *p_payload_u8, uint16_t    d_payload_len_u16                                 );
mqtt_return_t mqtt_subscribe            (const char              *p_topic_ch  ,       uint8_t   d_qos_u8                                                                    );
mqtt_return_t mqtt_set_message_callback (mqtt_message_callback_t  fp_callback                                                                                               );
mqtt_return_t mqtt_start_receiver       (void);
mqtt_return_t mqtt_disconnect           (void);




#endif