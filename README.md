# Smart Bike IoT — ESP32 + SIM800C

ESP32 tabanlı akıllı bisiklet IoT projesi. SIM800C GSM modülü üzerinden GPRS bağlantısı kurarak OTA firmware güncelleme gerçekleştirir.

---

## Donanım

| Bileşen   | Detay                          |
|-----------|--------------------------------|
| MCU       | ESP32                          |
| GSM       | SIM800C — UART2 (TX:17, RX:16) |
| Güç       | 3.7V LiPo + 5V regülatör       |

---

## Yazılım Mimarisi

```
components/
├── uart/        → Platform bağımsız UART sürücüsü
├── sim800c/     → SIM800C AT komut katmanı (geliştiriliyor)
├── ota/         → OTA firmware güncelleme
└── config/      → Sabitler, versiyon bilgisi
main/
└── main.c       → Uygulama giriş noktası
```

---

## Versiyon Kuralları

```
v<Major>.<Minor>.<Patch>

Major → Büyük mimari değişiklik (örn: FreeRTOS'a geçiş)
Minor → Yeni modül veya yeni özellik eklendi
Patch → Hata düzeltme, küçük değişiklik
```

Aktif versiyon her zaman `components/config/include/versiyon.h` dosyasında takip edilir.

---

## Versiyon Geçmişi

### v1.1.0 — 08.04.2026
- UART sub-modülü eklendi (`components/uart/`)
- Platform bağımsız mimari kuruldu (fonksiyon pointer ile dependency injection)
- `uart_baslat`, `uart_durdur`, `uart_gonder`, `uart_oku`, `uart_temizle`, `uart_baud_degistir` fonksiyonları implement edildi
- Birden fazla UART portu aynı sürücüyle yönetilebilir

### v1.0.0 — 29.03.2026
- İlk çalışan sürüm
- SIM800C GSM modülü ile GPRS bağlantısı
- HTTP GET ile OTA versiyon kontrolü
- 16KB chunk ile firmware indirme ve flash yazma

---

## Kurulum

```bash
# Projeyi derle
idf.py build

# ESP32'ye yükle
idf.py -p PORT flash

# Seri monitör
idf.py -p PORT monitor
```

---

## OTA Sunucusu

| Endpoint            | Açıklama                  |
|---------------------|---------------------------|
| GET /version.json   | Güncel versiyon bilgisi   |
| GET /firmware.bin   | Binary firmware dosyası   |
| POST /api/upload    | Yeni firmware yükleme     |

Sunucu: `boldmotorbikes.duckdns.org` (Hetzner VPS, Node.js + Nginx)

---

**Yazar:** Hasan Basri SOYLU
