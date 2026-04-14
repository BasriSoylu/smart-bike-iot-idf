# Smart Bike IoT - ESP32 + SIM800C

ESP32 tabanli akilli bisiklet IoT projesi. SIM800C GSM modulu uzerinden GPRS baglantisi kurarak OTA firmware guncelleme gerceklestirir.

---

## Donanim

| Bilesen   | Detay                          |
|-----------|--------------------------------|
| MCU       | ESP32                          |
| GSM       | SIM800C - UART2 (TX:17, RX:16) |
| Guc       | 3.7V LiPo + 5V regulatoru      |

---

## Yazilim Mimarisi

```
components/
|-- uart/        -> Platform bagimsiz UART surucusu
|-- sim800c/     -> SIM800C AT komut katmani (gelistiriliyor)
|-- ota/         -> OTA firmware guncelleme
|-- config/      -> Sabitler, versiyon bilgisi
main/
|-- main.c       -> Uygulama giris noktasi
```

---

## Versiyon Kurallari

```
v<Major>.<Minor>.<Patch>

Major -> Buyuk mimari degisiklik (ornek: FreeRTOS'a gecis)
Minor -> Yeni modul veya yeni ozellik eklendi
Patch -> Hata duzeltme, kucuk degisiklik
```

Aktif versiyon her zaman `components/config/include/versiyon.h` dosyasinda takip edilir.

---

## Versiyon Gecmisi

### v1.2.1 - 15.04.2026
- `sim800c_gprs_connect()` implement edildi
- GPRS baglanti akisi: AT+CGATT + AT+SAPBR serisi (bearer kapat/tip/APN/baslat/sorgula)
- Atik yorum temizlendi
- Eksik implementasyonlar (gprs_disconnect, http_get_json, http_get_binary, mqtt_publish) dokumante edildi

### v1.2.0 - 13.04.2026
- `sim800c` component eklendi
- Dependency Injection mimarisi ile AT komut surucu yazildi
- FreeRTOS reader task, state machine (IDLE/ECHO_BEKLE/CEVAP_BEKLE/CEVAP_HAZIR/HATA) ve overflow korumasi implement edildi
- `sim800c_baslat()` ile baud rate otomatik ayarlama (9600 ↔ 115200)

### v1.1.0 - 08.04.2026
- UART sub-modulu eklendi (`components/uart/`)
- Platform bagimsiz mimari kuruldu (fonksiyon pointer ile dependency injection)
- `uart_baslat`, `uart_durdur`, `uart_gonder`, `uart_oku`, `uart_temizle`, `uart_baud_degistir` fonksiyonlari implement edildi
- Birden fazla UART portu ayni surucuyle yonetilebilir

### v1.0.0 - 29.03.2026
- Ilk calisan surum
- SIM800C GSM modulu ile GPRS baglantisi
- HTTP GET ile OTA versiyon kontrolu
- 16KB chunk ile firmware indirme ve flash yazma

---

## Kurulum

```bash
# Projeyi derle
idf.py build

# ESP32'ye yukle
idf.py -p PORT flash

# Seri monitor
idf.py -p PORT monitor
```

---

## OTA Sunucusu

| Endpoint            | Aciklama                  |
|---------------------|---------------------------|
| GET /version.json   | Guncel versiyon bilgisi   |
| GET /firmware.bin   | Binary firmware dosyasi   |
| POST /api/upload    | Yeni firmware yukleme     |

Sunucu: `boldmotorbikes.duckdns.org` (Hetzner VPS, Node.js + Nginx)

---

**Yazar:** Hasan Basri SOYLU
