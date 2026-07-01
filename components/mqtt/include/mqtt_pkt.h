#ifndef MQTT_PKT_H
#define MQTT_PKT_H


/*=================== Includes ===================*/
#include <stdint.h>
#include <stddef.h>

/*=================== Defination ===================*/
#define MQTT_VERSION_TYPE                        ( 0x4U )   /* (MQTT 3.1.1) */

/* =============== Fixed Header Flag Sabitleri (alt 4 bit) =============== */
#define MQTT_FLAGS_CONNECT                       ( 0x0U )   /* hep 0 */
#define MQTT_FLAGS_CONNACK                       ( 0x0U )
#define MQTT_FLAGS_PUBLISH_DEFAULT               ( 0x0U )   /* DUP=0, QoS=0, RETAIN=0 */
#define MQTT_FLAGS_PUBACK                        ( 0x0U )
#define MQTT_FLAGS_PUBREC                        ( 0x0U )
#define MQTT_FLAGS_PUBREL                        ( 0x2U )   /* spec gereği reserved = 2 */
#define MQTT_FLAGS_PUBCOMP                       ( 0x0U )
#define MQTT_FLAGS_SUBSCRIBE                     ( 0x2U )   /* spec gereği reserved = 2 */
#define MQTT_FLAGS_SUBACK                        ( 0x0U )
#define MQTT_FLAGS_UNSUBSCRIBE                   ( 0x2U )   /* spec gereği reserved = 2 */
#define MQTT_FLAGS_UNSUBACK                      ( 0x0U )
#define MQTT_FLAGS_PINGREQ                       ( 0x0U )
#define MQTT_FLAGS_PINGRESP                      ( 0x0U )
#define MQTT_FLAGS_DISCONNECT                    ( 0x0U )


/* ====================== Connack Return Codes ====================== */
#define MQTT_CONNACK_ACCEPTED                    ( 0x00U )   /* Bağlantı kabul edildi */
#define MQTT_CONNACK_BAD_PROTOCOL_VERSION        ( 0x01U )   /* Desteklenmeyen protokol */
#define MQTT_CONNACK_IDENTIFIER_REJECTED         ( 0x02U )   /* Client ID reddedildi */
#define MQTT_CONNACK_SERVER_UNAVAILABLE          ( 0x03U )   /* Sunucu kullanılamıyor */
#define MQTT_CONNACK_BAD_USERNAME_PASSWORD       ( 0x04U )   /* Yanlış kimlik bilgileri */
#define MQTT_CONNACK_NOT_AUTHORIZED              ( 0x05U )   /* Yetkisiz */


/* ====================== SUBACK Return Codes ====================== */
#define MQTT_SUBACK_QOS_0_OK                     ( 0x00U )   /* QoS 0 kabul */
#define MQTT_SUBACK_QOS_1_OK                     ( 0x01U )   /* QoS 1 kabul */
#define MQTT_SUBACK_QOS_2_OK                     ( 0x02U )   /* QoS 2 kabul */
#define MQTT_SUBACK_FAILURE                      ( 0x80U )   /* Reddedildi */




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
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Control Byte (1 byte)                                                 │
*   │  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐                    │
*   │  │ bit │ bit │ bit │ bit │ bit │ bit │ bit │ bit │                    │
*   │  │  7  │  6  │  5  │  4  │  3  │  2  │  1  │  0  │                    │
*   │  └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘                    │
*   │  └────── packet_type (4 bit) ─────┘└── flags (4 bit) ─┘               │
*   │   1=CONNECT, 2=CONNACK, 3=PUBLISH                                     │
*   │   4=PUBACK, 5=PUBREC, 6=PUBREL, 7=PUBCOMP                             │
*   │   8=SUBSCRIBE, 9=SUBACK, 10=UNSUBSCRIBE                               │
*   │   11=UNSUBACK, 12=PINGREQ, 13=PINGRESP, 14=DISCONNECT                 │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Remaining Length (1-4 byte, VLE)                                      │
*   │  [byte_0] (MSB = devam var bayrağı)                                   │
*   │  [byte_1] (opsiyonel, byte_0'ın MSB'si 1 ise)                         │
*   │  [byte_2] (opsiyonel)                                                 │
*   │  [byte_3] (opsiyonel)                                                 │
*   │  → encode_remaining_length() ile çevrilir                             │
*   │  Max değer: 268,435,455 (256 MB)                                      │
*   │============================== KULLANIM ===============================│
*   │  fixed_header_t hdr_st = {0};                                         │
*   │  hdr_st.control_byte_ut.bits_st.packet_type = MQTT_PKT_CONNECT;       │
*   │  hdr_st.control_byte_ut.bits_st.flags       = MQTT_FLAGS_CONNECT;     │
*   │  hdr_st.d_remaining_length_u32 = 14U;                                 │
*   └───────────────────────────────────────────────────────────────────────┘ */
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


/*  ================== PUBLISH Flags (Control Byte alt 4 bit) ================== */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Sadece PUBLISH paketinde anlamlı                                      │
*   │  ┌─────┬─────┬─────┬─────┐                                            │
*   │  │ bit │ bit │ bit │ bit │                                            │
*   │  │  3  │  2  │  1  │  0  │                                            │
*   │  └─────┴─────┴─────┴─────┘                                            │
*   │    DUP   QoS-MSB QoS-LSB RETAIN                                       │
*   │                                                                       │
*   │  DUP    : 1 = bu paket yeniden yollandı (QoS > 0 için)                │
*   │  QoS    : 00=0, 01=1, 10=2                                            │
*   │  RETAIN : 1 = broker bu mesajı kalıcı tutsun                          │
*   │============================== KULLANIM ===============================│
*   │  // QoS 1, retain=1                                                   │
*   │  publish_flags_t pflags_st = {0};                                     │
*   │  pflags_st.bits_st.qos    = 1U;                                       │
*   │  pflags_st.bits_st.retain = 1U;                                       │
*   │  pflags_st.bits_st.dup    = 0U;                                       │
*   │  // fixed_header'a value_u8 olarak yazılır                            │
*   │  hdr_st.control_byte_ut.bits_st.flags = pflags_st.value_u8;           │
*   └───────────────────────────────────────────────────────────────────────┘ */
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
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐                    │
*   │  │ bit │ bit │ bit │ bit │ bit │ bit │ bit │ bit │                    │
*   │  │  7  │  6  │  5  │  4  │  3  │  2  │  1  │  0  │                    │
*   │  └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘                    │
*   │   USER  PASS  RET   ── QOS ──   WILL  CLEAN  RSV (0)                  │
*   │                                                                       │
*   │  Username Flag : 1 = payload'da username var                          │
*   │  Password Flag : 1 = payload'da password var                          │
*   │  Will Retain   : 1 = LWT mesajını broker retain'lasın                 │
*   │  Will QoS      : LWT mesajının QoS seviyesi (00/01/10)                │
*   │  Will Flag     : 1 = payload'da will_topic + will_payload             │
*   │  Clean Start   : 1 = yeni temiz oturum, eski state silinsin           │
*   │  Reserved      : hep 0                                                │
*   │============================== KULLANIM ===============================│
*   │  connect_flags_t cflags_st = {0};                                     │
*   │  cflags_st.connect_flags_ut.bits_st.clean_start = 1U;                 │
*   │  cflags_st.connect_flags_ut.bits_st.will_flag   = 1U;                 │
*   │  cflags_st.connect_flags_ut.bits_st.will_qos    = 1U;                 │
*   │  cflags_st.connect_flags_ut.bits_st.will_retain = 0U;                 │
*   └───────────────────────────────────────────────────────────────────────┘ */
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
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Protocol Name (6 byte)                                                │
*   │  [0x00][0x04]['M']['Q']['T']['T']                                     │
*   │   └ len = 4 ┘└──── "MQTT" ────────┘                                   │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Protocol Level (1 byte)                                               │
*   │  [0x04]   ← MQTT 3.1.1                                                │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Connect Flags (1 byte)                                                │
*   │  [flags]   ← connect_flags_t                                          │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Keep Alive (2 byte)                                                   │
*   │  [MSB][LSB]   ← saniye, big-endian                                    │
*   │   Örn: 60 saniye → [0x00][0x3C]                                       │
*   │============================== KULLANIM ===============================│
*   │  connect_variable_header_t vh_st = {                                  │
*   │      .d_protocol_name_len_u16 = 4U                    ,               │
*   │      .protocol_name_ch        = {'M','Q','T','T'}     ,               │
*   │      .d_protocol_level_u8     = MQTT_VERSION_TYPE     ,               │
*   │      .flags_st                = { 0 }                 ,               │
*   │      .d_keep_alive_u16        = 60U                   ,               │
*   │  };                                                                   │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    uint16_t        d_protocol_name_len_u16 ;   /* sabit 4 */
    char            protocol_name_ch[4]     ;   /* sabit "MQTT" */
    uint8_t         d_protocol_level_u8     ;   /* sabit 0x04 (MQTT 3.1.1) */
    connect_flags_t flags_st                ;
    uint16_t        d_keep_alive_u16        ;
}connect_variable_header_t;


/*  ====================== CONNECT Payload ====================== */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Client ID                  ← ZORUNLU                                  │
*   │  [len_MSB][len_LSB][client_id bytes]                                  │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Will Topic                 ← Will Flag=1 ise                          │
*   │  [len_MSB][len_LSB][topic bytes]                                      │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Will Payload               ← Will Flag=1 ise                          │
*   │  [len_MSB][len_LSB][payload bytes]                                    │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Username                   ← Username Flag=1 ise                      │
*   │  [len_MSB][len_LSB][username bytes]                                   │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Password                   ← Password Flag=1 ise                      │
*   │  [len_MSB][len_LSB][password bytes]                                   │
*   │============================== KULLANIM ===============================│
*   │  // Sadece client_id                                                  │
*   │  connect_payload_t pl_st = {0};                                       │
*   │  pl_st.p_client_id_ch = "esp32_bold";                                 │
*   │                                                                       │
*   │  // LWT'li örnek                                                      │
*   │  pl_st.p_will_topic_ch        = "hbs/devices/xxx/status";             │
*   │  pl_st.p_will_payload_u8      = (const uint8_t*)"{\"online\":false}"; │
*   │  pl_st.d_will_payload_len_u16 = 16U;                                  │
*   └───────────────────────────────────────────────────────────────────────┘ */
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
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Fixed Header                                                          │
*   │  └ fixed_header_t                                                     │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Variable Header (10 byte sabit)                                       │
*   │  └ connect_variable_header_t                                          │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Payload (değişken)                                                    │
*   │  └ connect_payload_t                                                  │
*   │============================== KULLANIM ===============================│
*   │  connect_packet_t conn_st = {0};                                      │
*   │  conn_st.fixed_header_st.control_byte_ut.bits_st.packet_type =        │
*   │                                             MQTT_PKT_CONNECT;         │
*   │  conn_st.fixed_header_st.control_byte_ut.bits_st.flags       =        │
*   │                                             MQTT_FLAGS_CONNECT;       │
*   │  conn_st.var_header_st.d_protocol_name_len_u16 = 4U;                  │
*   │  conn_st.var_header_st.d_protocol_level_u8     = MQTT_VERSION_TYPE;   │
*   │  conn_st.var_header_st.d_keep_alive_u16        = 60U;                 │
*   │  conn_st.payload_st.p_client_id_ch             = "esp32_bold";        │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    fixed_header_t            fixed_header_st;
    connect_variable_header_t var_header_st  ;
    connect_payload_t         payload_st     ;
}connect_packet_t;


/*  ====================== CONNACK Variable Header (2 byte) ====================== */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Acknowledge Flags (1 byte)                                            │
*   │  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐                    │
*   │  │ bit │ bit │ bit │ bit │ bit │ bit │ bit │ bit │                    │
*   │  │  7  │  6  │  5  │  4  │  3  │  2  │  1  │  0  │                    │
*   │  └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘                    │
*   │   └────────── reserved (hep 0) ─────────────┘└─SP─┘                   │
*   │  SP (Session Present): 1 = broker eski oturumu hatirliyor             │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Return Code (1 byte)                                                  │
*   │  0x00 = OK (Connection Accepted)                                      │
*   │  0x01 = Unacceptable Protocol Version                                 │
*   │  0x02 = Identifier Rejected                                           │
*   │  0x03 = Server Unavailable                                            │
*   │  0x04 = Bad User Name or Password                                     │
*   │  0x05 = Not Authorized                                                │
*   │============================== KULLANIM ===============================│
*   │  // parse sonrası kontrol                                             │
*   │  if ( MQTT_CONNACK_ACCEPTED == vh_st.d_return_code_u8 )               │
*   │  {                                                                    │
*   │      // baglantı kabul edildi                                         │
*   │  }                                                                    │
*   │  if ( 1U == vh_st.ack_flags_ut.bits_st.session_present )              │
*   │  {                                                                    │
*   │      // broker eski oturumu hatırlıyor                                │
*   │  }                                                                    │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    union
    {
        struct
        {
            uint8_t session_present : 1;   /* bit 0 */
            uint8_t reserved        : 7;   /* bit 1-7 */
        }bits_st;
        uint8_t byte_u8;
    }ack_flags_ut;
    uint8_t d_return_code_u8;
}connack_variable_header_t;

/*  ====================== CONNACK Paketi (4 byte sabit) ====================== */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Fixed Header                                                          │
*   │  [0x20] [0x02]   ← control byte + remaining length = 2                │
*   │  └ fixed_header_t                                                     │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Variable Header (2 byte)                                              │
*   │  └ connack_variable_header_t                                          │
*   │============================== KULLANIM ===============================│
*   │  connack_packet_t ack_st = {0};                                       │
*   │  // parser doldurur                                                   │
*   │  if ( MQTT_CONNACK_ACCEPTED ==                                        │
*   │             ack_st.var_header_st.d_return_code_u8 )                   │
*   │  {                                                                    │
*   │      // baglandı                                                      │
*   │  }                                                                    │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    fixed_header_t            fixed_header_st;
    connack_variable_header_t var_header_st  ;
}connack_packet_t;


/*  ====================== PUBLISH Variable Header (değişken) ====================== */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Topic Name (UTF-8 string)                                             │
*   │  [len_MSB][len_LSB][topic bytes...]                                   │
*   │  → encode_string() ile yazılır                                        │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Packet ID (2 byte)                                                    │
*   │  [pkt_id_MSB][pkt_id_LSB]                                             │
*   │  SADECE QoS > 0 ise var                                               │
*   │  QoS 0 paketinde packet_id YOKTUR                                     │
*   │============================== KULLANIM ===============================│
*   │  publish_variable_header_t vh_st = {0};                               │
*   │  vh_st.p_topic_ch      = "hbs/bisiklet/gps";                          │
*   │  vh_st.d_packet_id_u16 = 42U;    // sadece QoS > 0 için               │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    const char *p_topic_ch     ;
    uint16_t    d_packet_id_u16;   /* sadece QoS > 0 için kullanılır, QoS 0'da yok say */
}publish_variable_header_t;


/*  ====================== PUBLISH Payload (değişken) ====================== */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Raw payload bytes (uzunluk başında YOK)                               │
*   │  [byte_0][byte_1][byte_2]...[byte_N-1]                                │
*   │  Uzunluk:                                                             │
*   │   payload_len = remaining_length                                      │
*   │                 - (2 + topic_len)                                     │
*   │                 - (QoS > 0 ise 2 byte packet_id)                      │
*   │============================== KULLANIM ===============================│
*   │  publish_payload_t pl_st = {0};                                       │
*   │  pl_st.p_payload_u8      = (const uint8_t*)json_buf;                  │
*   │  pl_st.d_payload_len_u16 = (uint16_t)json_len;                        │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    const uint8_t *p_payload_u8     ;
    uint16_t       d_payload_len_u16;
}publish_payload_t;


/*  ====================== PUBLISH Paketi ====================== */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Fixed Header                                                          │
*   │  control byte: üst 4 bit = 3 (PUBLISH)                                │
*   │                alt 4 bit = DUP | QoS | QoS | RETAIN                   │
*   │  └ fixed_header_t (flags için publish_flags_t kullan)                 │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Variable Header                                                       │
*   │  └ publish_variable_header_t                                          │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Payload                                                               │
*   │  └ publish_payload_t                                                  │
*   │============================== KULLANIM ===============================│
*   │  publish_packet_t pub_st = {0};                                       │
*   │  pub_st.fixed_header_st.control_byte_ut.bits_st.packet_type =         │
*   │                                             MQTT_PKT_PUBLISH;         │
*   │                                                                       │
*   │  publish_flags_t pflags_st = {0};                                     │
*   │  pflags_st.bits_st.qos = 0U;                                          │
*   │  pub_st.fixed_header_st.control_byte_ut.bits_st.flags =               │
*   │                                             pflags_st.value_u8;       │
*   │                                                                       │
*   │  pub_st.var_header_st.p_topic_ch      = "hbs/bisiklet/gps";           │
*   │  pub_st.payload_st.p_payload_u8       = (const uint8_t*)json;         │
*   │  pub_st.payload_st.d_payload_len_u16  = 71U;                          │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    fixed_header_t            fixed_header_st;
    publish_variable_header_t var_header_st  ;
    publish_payload_t         payload_st     ;
}publish_packet_t;


/*  ====================== ACK Paketleri (4 byte sabit) ======================  */
/*  PUBACK(0x40) / PUBREC(0x50) / PUBREL(0x62) / PUBCOMP(0x70) / UNSUBACK(0xB0) */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Fixed Header (2 byte)                                                 │
*   │  [control_byte] [0x02]   ← remaining length = 2                       │
*   │  └ fixed_header_t                                                     │
*   │  Hangi ACK olduğunu control byte belirler:                            │
*   │   MQTT_PKT_PUBACK   → PUBLISH QoS 1 cevabı                            │
*   │   MQTT_PKT_PUBREC   → PUBLISH QoS 2 adım 1                            │
*   │   MQTT_PKT_PUBREL   → PUBLISH QoS 2 adım 2 (flags=2!)                 │
*   │   MQTT_PKT_PUBCOMP  → PUBLISH QoS 2 adım 3                            │
*   │   MQTT_PKT_UNSUBACK → UNSUBSCRIBE cevabı                              │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Variable Header (2 byte)                                              │
*   │  [pkt_id_MSB][pkt_id_LSB]                                             │
*   │  → hangi paketin cevabı olduğunu eşleştirir                           │
*   │============================== KULLANIM ===============================│
*   │  // PUBACK yolla                                                      │
*   │  ack_packet_t ack_st = {0};                                           │
*   │  ack_st.fixed_header_st.control_byte_ut.bits_st.packet_type =         │
*   │                                             MQTT_PKT_PUBACK;          │
*   │  ack_st.fixed_header_st.control_byte_ut.bits_st.flags       =         │
*   │                                             MQTT_FLAGS_PUBACK;        │
*   │  ack_st.d_packet_id_u16 = 42U;                                        │
*   │                                                                       │
*   │  // PUBREL (flags = 2 zorunlu!)                                       │
*   │  ack_packet_t rel_st = {0};                                           │
*   │  rel_st.fixed_header_st.control_byte_ut.bits_st.packet_type =         │
*   │                                             MQTT_PKT_PUBREL;          │
*   │  rel_st.fixed_header_st.control_byte_ut.bits_st.flags       =         │
*   │                                             MQTT_FLAGS_PUBREL;        │
*   │  rel_st.d_packet_id_u16 = 42U;                                        │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    fixed_header_t fixed_header_st;
    uint16_t       d_packet_id_u16;
}ack_packet_t;


/*  ====================== SUBSCRIBE Topic Filter ====================== */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Bir topic filter kaydı (payload'da bir veya daha fazla)               │
*   │                                                                       │
*   │ Topic Filter (UTF-8 string)                                           │
*   │  [len_MSB][len_LSB][topic bytes...]                                   │
*   │                                                                       │
*   │ Requested QoS (1 byte)                                                │
*   │  [qos]   ← 0x00, 0x01, veya 0x02                                      │
*   │============================== KULLANIM ===============================│
*   │  subscribe_topic_filter_t tf_st = {                                   │
*   │      .p_topic_ch = "hbs/+/komut",                                     │
*   │      .d_qos_u8   = 0U,                                                │
*   │  };                                                                   │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    const char *p_topic_ch;
    uint8_t     d_qos_u8  ;   /* istenen QoS: 0/1/2 */
}subscribe_topic_filter_t;


/*  ====================== SUBSCRIBE Paketi ====================== */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Fixed Header                                                          │
*   │  control byte = 0x82 (üst 4 bit = 8, alt 4 bit = 2 reserved)          │
*   │  └ fixed_header_t                                                     │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Variable Header (2 byte)                                              │
*   │  [pkt_id_MSB][pkt_id_LSB]                                             │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Payload                                                               │
*   │  Bir veya daha fazla topic filter                                     │
*   │  Şimdilik TEK topic destekleniyor                                     │
*   │  └ subscribe_topic_filter_t                                           │
*   │============================== KULLANIM ===============================│
*   │  subscribe_packet_t sub_st = {0};                                     │
*   │  sub_st.fixed_header_st.control_byte_ut.bits_st.packet_type =         │
*   │                                             MQTT_PKT_SUBSCRIBE;       │
*   │  sub_st.fixed_header_st.control_byte_ut.bits_st.flags       =         │
*   │                                             MQTT_FLAGS_SUBSCRIBE;     │
*   │  sub_st.d_packet_id_u16              = 1U;                            │
*   │  sub_st.topic_filter_st.p_topic_ch   = "hbs/+/komut";                 │
*   │  sub_st.topic_filter_st.d_qos_u8     = 0U;                            │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    fixed_header_t           fixed_header_st;
    uint16_t                 d_packet_id_u16;
    subscribe_topic_filter_t topic_filter_st;   /* şimdilik tek topic */
}subscribe_packet_t;



/*  ====================== SUBACK Paketi ====================== */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Fixed Header                                                          │
*   │  control byte = 0x90                                                  │
*   │  └ fixed_header_t                                                     │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Variable Header (2 byte)                                              │
*   │  [pkt_id_MSB][pkt_id_LSB]                                             │
*   │  → hangi SUBSCRIBE'ın cevabı                                          │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Payload                                                               │
*   │  Her topic filter için bir return code (1 byte)                       │
*   │  Sıra SUBSCRIBE'daki sırayla aynı                                     │
*   │  Şimdilik TEK topic destekliyor → tek return code                     │
*   │                                                                       │
*   │  MQTT_SUBACK_QOS_0_OK (0x00) / QOS_1_OK (0x01)                        │
*   │  MQTT_SUBACK_QOS_2_OK (0x02) / FAILURE (0x80)                         │
*   │============================== KULLANIM ===============================│
*   │  // parse sonrası kontrol                                             │
*   │  if ( MQTT_SUBACK_FAILURE == suback_st.d_return_code_u8 )             │
*   │  {                                                                    │
*   │      // broker reddetti                                               │
*   │  }                                                                    │
*   │  else                                                                 │
*   │  {                                                                    │
*   │      // kabul: QoS = suback_st.d_return_code_u8                       │
*   │  }                                                                    │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    fixed_header_t fixed_header_st ;
    uint16_t       d_packet_id_u16 ;
    uint8_t        d_return_code_u8;
}suback_packet_t;


/*  ====================== UNSUBSCRIBE Paketi ====================== */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Fixed Header                                                          │
*   │  control byte = 0xA2 (üst 4 bit = 10, alt 4 bit = 2 reserved)         │
*   │  └ fixed_header_t                                                     │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Variable Header (2 byte)                                              │
*   │  [pkt_id_MSB][pkt_id_LSB]                                             │
*   ├───────────────────────────────────────────────────────────────────────┤
*   │ Payload                                                               │
*   │  Bir veya daha fazla topic filter (QoS YOK, SUBSCRIBE'tan             │
*   │  farkı bu)                                                            │
*   │  [len_MSB][len_LSB][topic bytes...]                                   │
*   │  Şimdilik TEK topic destekliyor                                       │
*   │============================== KULLANIM ===============================│
*   │  unsubscribe_packet_t un_st = {0};                                    │
*   │  un_st.fixed_header_st.control_byte_ut.bits_st.packet_type =          │
*   │                                             MQTT_PKT_UNSUBSCRIBE;     │
*   │  un_st.fixed_header_st.control_byte_ut.bits_st.flags       =          │
*   │                                             MQTT_FLAGS_UNSUBSCRIBE;   │
*   │  un_st.d_packet_id_u16 = 5U;                                          │
*   │  un_st.p_topic_ch      = "hbs/bisiklet/gps";                          │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    fixed_header_t  fixed_header_st;
    uint16_t        d_packet_id_u16;
    const char     *p_topic_ch     ;
}unsubscribe_packet_t;


/*  ====================== Gövdesiz Paketler ====================== */
/*  PINGREQ (0xC0) / PINGRESP (0xD0) / DISCONNECT (0xE0) */
/** ┌───────────────────────────────────────────────────────────────────────┐
*   │ Fixed Header (tüm paket, 2 byte)                                      │
*   │  [control_byte] [0x00]   ← remaining length = 0, gövde yok            │
*   │  └ fixed_header_t                                                     │
*   │                                                                       │
*   │  Hangi paket:                                                         │
*   │   MQTT_PKT_PINGREQ    → client "hala buradayım"                       │
*   │   MQTT_PKT_PINGRESP   → broker "ben de buradayım"                     │
*   │   MQTT_PKT_DISCONNECT → client "kibarca kapatıyorum"                  │
*   │                                                                       │
*   │  DISCONNECT gönderilirse broker LWT yayınlamaz                        │
*   │  (kibarca kapatıldı say)                                              │
*   │============================== KULLANIM ===============================│
*   │  // PINGREQ                                                           │
*   │  control_packet_t ping_st = {0};                                      │
*   │  ping_st.fixed_header_st.control_byte_ut.bits_st.packet_type =        │
*   │                                             MQTT_PKT_PINGREQ;         │
*   │  ping_st.fixed_header_st.control_byte_ut.bits_st.flags       =        │
*   │                                             MQTT_FLAGS_PINGREQ;       │
*   │                                                                       │
*   │  // DISCONNECT                                                        │
*   │  control_packet_t disc_st = {0};                                      │
*   │  disc_st.fixed_header_st.control_byte_ut.bits_st.packet_type =        │
*   │                                             MQTT_PKT_DISCONNECT;      │
*   │  disc_st.fixed_header_st.control_byte_ut.bits_st.flags       =        │
*   │                                             MQTT_FLAGS_DISCONNECT;    │
*   └───────────────────────────────────────────────────────────────────────┘ */
typedef struct
{
    fixed_header_t fixed_header_st;
}control_packet_t;



#endif
