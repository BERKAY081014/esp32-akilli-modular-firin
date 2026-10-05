# Akıllı Modüler Fırın Kontrol Sistemi

ESP32 mikrodenetleyici mimarisi üzerine kurulu, yüksek hassasiyetli endüstriyel fırın sıcaklık kontrol ve uzaktan telemetri izleme sistemi.

## 🚀 Özellikler
- **Hassas Sıcaklık Okuma:** MAX31865 dönüştürücü ve 3 telli PT100 RTD platin direnç ile 0.1°C çözünürlük.
- **Kapalı Çevrim PID Kontrol:** Aşma (overshoot) engelleyici anti-windup mekanizmalı PID algoritması.
- **IoT & MQTT Entegrasyonu:** Gerçek zamanlı telemetri yayını (`berkay/firin/sicaklik`) ve uzaktan komut işleme.
- **Çok Katmanlı Güvenlik:** 260°C üst limit koruma, acil stop butonu ve sensör hatası algılama ile otomatik fırın kapatma.
- **Yerel Arayüz:** SSD1306 0.96" I2C OLED ekranda anlık durum izleme.

## 🛠️ Devre & Pin Bağlantıları
| Donanım | ESP32 Pini | Açıklama |
|---|---|---|
| MAX31865 CS | GPIO 5 | SPI Chip Select |
| MAX31865 SCK | GPIO 18 | SPI Clock |
| MAX31865 MISO | GPIO 19 | SPI Master In |
| MAX31865 MOSI | GPIO 23 | SPI Master Out |
| Solid State Röle (SSR) | GPIO 25 | Isıtıcı PWM Çıkışı |
| Soğutma Fan Rölesi | GPIO 26 | Fan Çıkışı |
| Sesli Uyarı Buzzer | GPIO 27 | Alarm Çıkışı |
| OLED SDA / SCL | GPIO 21 / 22 | I2C İletişim |

## 📦 Kurulum
1. PlatformIO veya Arduino IDE üzerinde ESP32 board paketini kurun.
2. `Adafruit MAX31865`, `PubSubClient`, `Adafruit SSD1306` kütüphanelerini ekleyin.
3. `main.cpp` dosyasındaki WiFi SSID ve şifrenizi güncelleyerek derleyip karta yükleyin.

**Geliştirici:** Berkay Bilgin ([@BERKAY081014](https://github.com/BERKAY081014))\n