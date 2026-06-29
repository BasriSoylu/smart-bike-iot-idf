#ifndef MQTT_PKT_H
#define MQTT_PKT_H


/*=================== Includes ===================*/
#include <stdint.h>
#include <stddef.h>

/*=================== Defination ===================*/
#define MQTT_VERSION_TYPE            (0x4U)   /* (MQTT 3.1.1) */

/* =============== Fixed Header Flag Sabitleri (alt 4 bit) =============== */
#define MQTT_FLAGS_CONNECT          (0x0U)   /* hep 0 */
#define MQTT_FLAGS_CONNACK          (0x0U)
#define MQTT_FLAGS_PUBLISH_DEFAULT  (0x0U)   /* DUP=0, QoS=0, RETAIN=0 */
#define MQTT_FLAGS_PUBACK           (0x0U)
#define MQTT_FLAGS_PUBREC           (0x0U)
#define MQTT_FLAGS_PUBREL           (0x2U)   /* spec gereği reserved = 2 */
#define MQTT_FLAGS_PUBCOMP          (0x0U)
#define MQTT_FLAGS_SUBSCRIBE        (0x2U)   /* spec gereği reserved = 2 */
#define MQTT_FLAGS_SUBACK           (0x0U)
#define MQTT_FLAGS_UNSUBSCRIBE      (0x2U)   /* spec gereği reserved = 2 */
#define MQTT_FLAGS_UNSUBACK         (0x0U)
#define MQTT_FLAGS_PINGREQ          (0x0U)
#define MQTT_FLAGS_PINGRESP         (0x0U)
#define MQTT_FLAGS_DISCONNECT       (0x0U)


/* =============== Paket Tipleri Enum =============== */
typedef enum
{
    MQTT_PKT_CONNECT     = 1U  ,
    MQTT_PKT_CONNACK     = 2U  ,
    MQTT_PKT_PUBLISH     = 3U  ,
    MQTT_PKT_PUBACK      = 4U  ,
    MQTT_PKT_PUBREC      = 5U  ,
    MQTT_PKT_PUBREL      = 6U  ,
    MQTT_PKT_PUBCOMP     = 7U  ,
    MQTT_PKT_SUBSCRIBE   = 8U  ,
    MQTT_PKT_SUBACK      = 9U  ,
    MQTT_PKT_UNSUBSCRIBE = 10U ,
    MQTT_PKT_UNSUBACK    = 11U ,
    MQTT_PKT_PINGREQ     = 12U ,
    MQTT_PKT_PINGRESP    = 13U ,
    MQTT_PKT_DISCONNECT  = 14U ,
}mqtt_packet_type_t;


/*  ====================== Fixed Header ====================== */
/** ┌───────────────────────────────────────────────────────────┐
*   │ Control Byte (1 byte)                                     │
*   │  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐        │
*   │  │ bit │ bit │ bit │ bit │ bit │ bit │ bit │ bit │        │
*   │  │  7  │  6  │  5  │  4  │  3  │  2  │  1  │  0  │        │
*   │  └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘        │
*   │  └────── packet_type (4 bit) ─────┘└── flags (4 bit) ─┘   │
*   │   1=CONNECT, 2=CONNACK, 3=PUBLISH                         │
*   │   4=PUBACK, 5=PUBREC, 6=PUBREL, 7=PUBCOMP                 │
*   │   8=SUBSCRIBE, 9=SUBACK, 10=UNSUBSCRIBE                   │
*   │   11=UNSUBACK, 12=PINGREQ, 13=PINGRESP, 14=DISCONNECT     │
*   ├───────────────────────────────────────────────────────────┤
*   │ Remaining Length (1-4 byte, VLE)                          │
*   │  [byte_0] (MSB = devam var bayrağı)                       │
*   │  [byte_1] (opsiyonel, byte_0'ın MSB'si 1 ise)             │
*   │  [byte_2] (opsiyonel)                                     │
*   │  [byte_3] (opsiyonel)                                     │
*   │  → encode_remaining_length() ile çevrilir                 │
*   │  Max değer: 268,435,455 (256 MB)                          │
*   └───────────────────────────────────────────────────────────┘ */
typedef struct
{
    union
    {
        struct
        {
            uint8_t flags       : 4;   /* LSB - paket tipine göre değişir */
            uint8_t packet_type : 4;   /* MSB - mqtt_packet_type_t */
        }bits_st;
        uint8_t byte_u8;
    }control_byte_ut;
    uint32_t d_remaining_length_u32;   /* Encode anında VLE'ye çevrilir */
}fixed_header_t;


/*  ====================== PUBLISH Flags (Control Byte alt 4 bit) ====================== */
/** ┌───────────────────────────────────────────────────────────┐
*   │ Sadece PUBLISH paketinde anlamlı                          │
*   │  ┌─────┬─────┬─────┬─────┐                                │
*   │  │ bit │ bit │ bit │ bit │                                │
*   │  │  3  │  2  │  1  │  0  │                                │
*   │  └─────┴─────┴─────┴─────┘                                │
*   │    DUP   QoS-MSB QoS-LSB RETAIN                           │
*   │                                                           │
*   │  DUP    : 1 = bu paket yeniden yollandı (QoS > 0 için)    │
*   │  QoS    : 00=0, 01=1, 10=2                                │
*   │  RETAIN : 1 = broker bu mesajı kalıcı tutsun              │
*   └───────────────────────────────────────────────────────────┘ */
typedef union                          
{
    struct
    {
        uint8_t retain : 1;
        uint8_t qos    : 2;
        uint8_t dup    : 1;
    }bits_st;
    uint8_t value_u8;
}publish_flags_t;


/*  ============ CONNECT Flags (Variable Header'da 1 byte) ============ */
/** ┌────────────────────────────────────────────────────────────┐
*   │  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐         │
*   │  │ bit │ bit │ bit │ bit │ bit │ bit │ bit │ bit │         │
*   │  │  7  │  6  │  5  │  4  │  3  │  2  │  1  │  0  │         │
*   │  └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘         │
*   │   USER  PASS  RET   ── QOS ──   WILL  CLEAN  RSV (0)       │
*   │                                                            │
*   │  Username Flag : 1 = payload'da username var               │
*   │  Password Flag : 1 = payload'da password var               │
*   │  Will Retain   : 1 = LWT mesajını broker retain'lasın      │
*   │  Will QoS      : LWT mesajının QoS seviyesi (00/01/10)     │
*   │  Will Flag     : 1 = payload'da will_topic + will_payload  │
*   │  Clean Start   : 1 = yeni temiz oturum, eski state silinsin│
*   │  Reserved      : hep 0                                     │
*   └────────────────────────────────────────────────────────────┘ */
typedef struct
{
    union
    {
        struct
        {
            uint8_t reserved      : 1;   /* bit 0 */
            uint8_t clean_start   : 1;   /* bit 1 */
            uint8_t will_flag     : 1;   /* bit 2 */
            uint8_t will_qos      : 2;   /* bit 3-4 */
            uint8_t will_retain   : 1;   /* bit 5 */
            uint8_t password_flag : 1;   /* bit 6 */
            uint8_t username_flag : 1;   /* bit 7 */
        }bits_st;
        uint8_t byte_u8;
    }connect_flags_ut;
}connect_flags_t;


/*  ========= CONNECT Variable Header (10 byte sabit) ============= */
/** ┌───────────────────────────────────────────────────────────┐
*   │ Protocol Name (6 byte)                                    │
*   │  [0x00][0x04]['M']['Q']['T']['T']                         │
*   │   └ len = 4 ┘└──── "MQTT" ────────┘                       │
*   ├───────────────────────────────────────────────────────────┤
*   │ Protocol Level (1 byte)                                   │
*   │  [0x04]   ← MQTT 3.1.1                                    │
*   ├───────────────────────────────────────────────────────────┤
*   │ Connect Flags (1 byte)                                    │
*   │  [flags]   ← connect_flags_t                              │
*   ├───────────────────────────────────────────────────────────┤
*   │ Keep Alive (2 byte)                                       │
*   │  [MSB][LSB]   ← saniye, big-endian                        │
*   │   Örn: 60 saniye → [0x00][0x3C]                           │
*   └───────────────────────────────────────────────────────────┘ */
typedef struct
{
    uint16_t        d_protocol_name_len_u16 ;   /* sabit 4 */
    char            protocol_name_ch[4]     ;   /* sabit "MQTT" */
    uint8_t         d_protocol_level_u8     ;   /* sabit 0x04 (MQTT 3.1.1) */
    connect_flags_t flags_st                ;
    uint16_t        d_keep_alive_u16        ;
}connect_variable_header_t;


/*  ====================== CONNECT Payload ====================== */
/** ┌───────────────────────────────────────────────────────────┐
*   │ Client ID                  ← ZORUNLU                      │
*   │  [len_MSB][len_LSB][client_id bytes]                      │
*   ├───────────────────────────────────────────────────────────┤
*   │ Will Topic                 ← Will Flag=1 ise              │
*   │  [len_MSB][len_LSB][topic bytes]                          │
*   ├───────────────────────────────────────────────────────────┤
*   │ Will Payload               ← Will Flag=1 ise              │
*   │  [len_MSB][len_LSB][payload bytes]                        │
*   ├───────────────────────────────────────────────────────────┤
*   │ Username                   ← Username Flag=1 ise          │
*   │  [len_MSB][len_LSB][username bytes]                       │
*   ├───────────────────────────────────────────────────────────┤
*   │ Password                   ← Password Flag=1 ise          │
*   │  [len_MSB][len_LSB][password bytes]                       │
*   └───────────────────────────────────────────────────────────┘ */
typedef struct
{
    const char    *p_client_id_ch         ;
    const char    *p_will_topic_ch        ;   /* NULL = Will yok */
    const uint8_t *p_will_payload_u8      ;
    uint16_t       d_will_payload_len_u16 ;
    const char    *p_username_ch          ;   /* NULL = username yok */
    const char    *p_password_ch          ;   /* NULL = password yok */
}connect_payload_t;

/*  ====================== CONNECT Paketi (tüm parçalar) ====================== */
/** ┌───────────────────────────────────────────────────────────┐
*   │ Fixed Header                                              │
*   │  └ fixed_header_t                                         │
*   ├───────────────────────────────────────────────────────────┤
*   │ Variable Header (10 byte sabit)                           │
*   │  └ connect_variable_header_t                              │
*   ├───────────────────────────────────────────────────────────┤
*   │ Payload (değişken)                                        │
*   │  └ connect_payload_t                                      │
*   └───────────────────────────────────────────────────────────┘ */
typedef struct
{
    fixed_header_t            fixed_header_st;
    connect_variable_header_t var_header_st  ;
    connect_payload_t         payload_st     ;
}connect_packet_t;













#endif