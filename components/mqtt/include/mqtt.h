#ifndef MQTT_H
#define MQTT_H

/*=================== Includes ===================*/
#include <stdint.h>
#include <stddef.h>

/*=================== Defines ===================*/
#define MQTT_MAX_REMAINING_LENGTH                    (0xFFFFFFFU)
#define MQTT_UTF8_STRING_MAX_LEN                     (0xFFFFU   )
#define MQTT_VARIABLE_HEADER_SABIT_KISMIN_UZUNLUGU   (10U       )  // protocol name 6 + level 1 + flags 1 + keep_alive 2

#define MQTT_PACKET_ID_UZUNLUGU                      (2U        )  // Packet ID alani (MSB+LSB)
#define MQTT_STRING_LEN_ALANI_UZUNLUGU               (2U        )  // UTF-8 string uzunluk alani (encode_string'in yazdigi ilk 2 byte)
#define MQTT_SUBSCRIBE_QOS_BYTE_UZUNLUGU             (1U        )  // SUBSCRIBE topic filter sonundaki requested QoS byte
#define MQTT_PROTOCOL_NAME_UZUNLUGU                  (4U        )           

#define MQTT_CONTROL_BYTE_UZUNLUGU                   (1U        )
#define MQTT_CONTROL_BYTE_DIZI_SIRASI                (0U        )
#define MQTT_REMAINING_LENGTH_SIRASI                 (1U        )
#define MQTT_REMAINING_LENGTH_DEVAM_MASKESI          (0x80U     )

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
    uint8_t         d_will_retain          ;
}mqtt_config_t;

/*=================== Callback Tipi ===================*/
typedef void (*mqtt_topic_handler_t)(const uint8_t *p_payload_u8, uint16_t d_payload_len_u16);


/*======================= Transport Interface =======================
 * MQTT katmani altinda TCP'yi nasil konusacagini bilmez.
 * Caller (main.c) sim800c'yi (veya WiFi'i) wrap edip burada verir.
 * Bu sayede mqtt component'i sim800c'den bagimsiz kalir.
 *
 * SOZLESME (yeni bir transport yazarken uyulmasi ZORUNLU):
 *  - send    : 0 = TAMAMI gonderildi, negatif = hata.
 *              Kismi gonderim MQTT katmanina sizdirilmaz — alttaki
 *              soket kismi yazarsa tamamlama dongusu wrapper icinde
 *              cozulur (byte sayisi dondurulmez).
 *  - receive : alinan byte sayisi (>=0), negatif = hata.
 *              Istenen uzunluk dolana veya timeout bitene kadar bekler.
 *  - tcp_open: 0 = basari, -1 = hata.
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
/* NOT: Yeni mimarinin public API'si (mqtt_start/stop, subscribe(topic,qos,handler),
 *      publish(topic,payload,len,qos), unsubscribe) FAZ 4D/5'te buraya eklenecek. */
mqtt_return_t mqtt_init      (const mqtt_transport_t *fp_transport                                                                           );
mqtt_return_t mqtt_subscribe (const char             *p_topic_ch  ,       uint8_t  d_qos_u8    , mqtt_topic_handler_t fp_handler             );
mqtt_return_t mqtt_publish   (const char             *p_topic_ch  , const uint8_t *p_payload_u8, uint16_t d_payload_len_u16, uint8_t d_qos_u8);
mqtt_return_t mqtt_start     (const mqtt_config_t    *p_config_st                                                                            );




#endif