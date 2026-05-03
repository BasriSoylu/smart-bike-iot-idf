#ifndef AT_COMMANDS_H
#define AT_COMMANDS_H

/*============ TEMEL AT KOMUTLARI ============*/
#define AT_TEST                 "AT"                                 // Modül hazır mı? Cevap: OK
#define AT_ECHO_ON              "ATE1"                               // Echo aç  → gönderilen komutu geri yazar
#define AT_ECHO_OFF             "ATE0"                               // Echo kapat
#define AT_BAUD_115200_YAP      "AT+IPR=115200"                      // Modul Baud Rate ayarini 115200 yap
#define AT_AUTOBAUD             "AT+IPR=0"                           // Autobaud modu (her baud'u algilar)
#define AT_SAVE_CONFIG          "AT&W"                               // Aktif ayarlari NVM'e kalici yaz

/*=========== SIM / SEBEKE KOMUTLARI ==========*/
#define AT_SIM_STATUS           "AT+CPIN?"                           // SIM PIN durumu → READY / SIM PIN / SIM PUK
#define AT_SIGNAL               "AT+CSQ"                             // Sinyal gücü   → +CSQ: <rssi>,<ber>
#define AT_REG_STATUS           "AT+CREG?"                           // Şebekeye kayıt → +CREG: 0,1 (1=kayıtlı)

/*=========== GPRS KOMUTLARI =================*/
#define AT_GPRS_ATTACH          "AT+CGATT=1"                         // GPRS'e bağlan
#define AT_GPRS_DETACH          "AT+CGATT=0"                         // GPRS'den kopar
#define AT_GPRS_ATTACH_QUERY    "AT+CGATT?"                          // GPRS attach durumu → +CGATT: 0/1
#define AT_BEARER_CLOSE         "AT+SAPBR=0,1"                       // Bearer kapat
#define AT_BEARER_START         "AT+SAPBR=1,1"                       // Bearer başlat (IP al)
#define AT_BEARER_QUERY         "AT+SAPBR=2,1"                       // IP adresini sorgula
#define AT_BEARER_SET_CONTYPE   "AT+SAPBR=3,1,\"Contype\",\"GPRS\""  // Bearer tipi: GPRS
#define AT_BEARER_APN           "AT+SAPBR=3,1,\"APN\",\"%s\""        // APN ayarla (printf ile doldurulacak)

/*=========== HTTP KOMUTLARI =================*/
#define AT_HTTP_INIT            "AT+HTTPINIT"                        // HTTP başlat
#define AT_HTTP_BEARER          "AT+HTTPPARA=\"CID\",1"              // Bearer ID bağla
#define AT_HTTP_URL             "AT+HTTPPARA=\"URL\",\"%s\""         // URL ayarla (printf ile)
#define AT_HTTP_GET             "AT+HTTPACTION=0"                    // GET isteği gönder
#define AT_HTTP_READ            "AT+HTTPREAD"                        // Gelen veriyi oku
#define AT_HTTP_TERM            "AT+HTTPTERM"                        // HTTP sonlandır

/*=========== TCP KOMUTLARI =================*/
#define AT_CIPMUX_SINGLE        "AT+CIPMUX=0"                        // Tek baglanti modu
#define AT_CIPSTART             "AT+CIPSTART"                        // Baglanti ac
#define AT_CIPSEND              "AT+CIPSEND"                         // Veri gonder
#define AT_CIPCLOSE             "AT+CIPCLOSE"                        // Baglanti kapat
#define AT_CIPSHUT              "AT+CIPSHUT"                         // PDP context sifirla


#endif // AT_COMMANDS_H


