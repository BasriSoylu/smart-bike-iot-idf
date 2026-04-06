#ifndef CONFIG_H
#define CONFIG_H

// Firmware versiyonu
#define FIRMWARE_VERSION  "1.0.0"

// OTA sunucu
#define OTA_SERVER_HOST   "boldmotorbikes.duckdns.org"
#define OTA_SERVER_PORT   80
#define OTA_VERSION_URL   "http://boldmotorbikes.duckdns.org/version.json"
#define OTA_FIRMWARE_URL  "http://boldmotorbikes.duckdns.org/firmware.bin"

// MQTT broker
#define MQTT_BROKER_HOST  "test.mosquitto.org"
#define MQTT_BROKER_PORT  1883
#define MQTT_CLIENT_ID    "smartbike_001"

// MQTT topic'ler
#define TOPIC_TELEMETRY   "smartbike/001/telemetry"
#define TOPIC_COMMAND     "smartbike/001/command"
#define TOPIC_OTA         "smartbike/001/ota"






#endif // CONFIG_H
