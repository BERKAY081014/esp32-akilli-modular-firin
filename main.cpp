/**
 * ============================================================================
 * Proje: Akıllı Modüler Fırın Kontrol Sistemi (Bitirme Tezi)
 * Geliştirici: Berkay Bilgin (https://github.com/BERKAY081014)
 * Platform: ESP32 NodeMCU-32S / Arduino Framework
 * Sensörler & Modüller: MAX31865 RTD PT100, SSD1306 OLED, Solid State Relay (SSR)
 * Protokol: MQTT / WiFi (WPA2)
 * ============================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_MAX31865.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- WiFi & MQTT Yapılandırması ----------
const char* WIFI_SSID     = "LAB_WIFI_NETWORK";
const char* WIFI_PASS     = "SecurePass1234!";
const char* MQTT_SERVER   = "broker.emqx.io";
const int   MQTT_PORT     = 1883;
const char* MQTT_CLIENT_ID = "ESP32_Firin_Berkay";

// MQTT Konuları
const char* TOPIC_TEMP_PUB  = "berkay/firin/sicaklik";
const char* TOPIC_STATE_PUB = "berkay/firin/durum";
const char* TOPIC_CMD_SUB   = "berkay/firin/komut";

// ---------- Donanım Pin Tanımları ----------
#define PIN_SSR_HEATER   25  // Isıtıcı Solid State Röle (PWM)
#define PIN_FAN_RELAY    26  // Soğutma Fanı Rölesi
#define PIN_BUZZER       27  // Güvenlik Sesli Alarm
#define PIN_EMERGENCY_SW 34  // Acil Stop Butonu (Giriş)

// MAX31865 Donanımsal SPI Pinleri
#define RREF      430.0f     // PT100 için referans direnç (430 Ohm)
#define RNOMINAL  100.0f     // PT100 için nominal 0°C direnç (100 Ohm)
#define MAX_CS_PIN 5
Adafruit_MAX31865 thermo = Adafruit_MAX31865(MAX_CS_PIN);

// OLED Ekran
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ---------- PID Sıcaklık Kontrol Parametreleri ----------
float targetTemperature = 180.0f; // Hedef Sıcaklık (°C)
float currentTemperature = 0.0f;
float kp = 4.2f;
float ki = 0.15f;
float kd = 1.8f;

float integral = 0.0f;
float lastError = 0.0f;
unsigned long lastPIDTime = 0;
bool isSystemActive = false;
bool isEmergencyTriggered = false;

// Fonksiyon Prototipleri
void setupWiFi();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
float computePID(float setpoint, float actual, float dt);
void updateDisplay();
void checkSafetyLimits();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[SİSTEM] Akıllı Modüler Fırın Kontrol Ünitesi Başlatılıyor...");

  pinMode(PIN_SSR_HEATER, OUTPUT);
  pinMode(PIN_FAN_RELAY, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_EMERGENCY_SW, INPUT_PULLUP);

  digitalWrite(PIN_SSR_HEATER, LOW);
  digitalWrite(PIN_FAN_RELAY, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  // MAX31865 RTD Sensörü Başlat (3 Telli PT100)
  thermo.begin(MAX31865_3WIRE);

  // OLED Ekran Başlat
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[HATA] SSD1306 OLED başlatılamadı!");
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("BERKAY BILGIN");
  display.setCursor(10, 35);
  display.println("Akilli Firin v2.4");
  display.display();

  setupWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  Serial.println("[BAŞARILI] Sistem hazır duruma geçti.");
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // 1. PT100 Sıcaklık Ölçümü
  uint16_t rtd = thermo.readRTD();
  float ratio = (float)rtd / 32768.0f;
  currentTemperature = thermo.temperature(RNOMINAL, RREF);

  // RTD Hata Kontrolü
  uint8_t fault = thermo.readFault();
  if (fault) {
    Serial.printf("[KRİTİK HATA] Sensör arızası: 0x%X\n", fault);
    thermo.clearFault();
    isEmergencyTriggered = true;
  }

  // 2. Acil Durum Kontrolü
  checkSafetyLimits();

  // 3. PID Sıcaklık Regülasyonu
  unsigned long now = millis();
  if (now - lastPIDTime >= 500) {
    float dt = (now - lastPIDTime) / 1000.0f;
    lastPIDTime = now;

    if (isSystemActive && !isEmergencyTriggered) {
      float output = computePID(targetTemperature, currentTemperature, dt);
      // PWM / Oransal Röle Tetikleme (0 - 255)
      ledcWrite(0, (uint32_t)output);
      
      // Fan Kontrolü (Histerezis)
      if (currentTemperature > targetTemperature + 5.0f) {
        digitalWrite(PIN_FAN_RELAY, HIGH);
      } else {
        digitalWrite(PIN_FAN_RELAY, LOW);
      }
    } else {
      digitalWrite(PIN_SSR_HEATER, LOW);
      digitalWrite(PIN_FAN_RELAY, isEmergencyTriggered ? HIGH : LOW);
    }

    // Telemetri Gönderimi (MQTT)
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.2f", currentTemperature);
    mqttClient.publish(TOPIC_TEMP_PUB, tempStr);

    char stateJson[128];
    snprintf(stateJson, sizeof(stateJson), 
             "{\"hedef\":%.1f,\"anlik\":%.2f,\"aktif\":%s,\"alarm\":%s}",
             targetTemperature, currentTemperature, 
             isSystemActive ? "true" : "false", 
             isEmergencyTriggered ? "true" : "false");
    mqttClient.publish(TOPIC_STATE_PUB, stateJson);

    // OLED Güncelle
    updateDisplay();
  }
}

float computePID(float setpoint, float actual, float dt) {
  float error = setpoint - actual;
  integral += error * dt;
  integral = constrain(integral, -100.0f, 100.0f); // Anti-windup
  float derivative = (error - lastError) / dt;
  lastError = error;

  float output = (kp * error) + (ki * integral) + (kd * derivative);
  return constrain(output, 0.0f, 255.0f);
}

void checkSafetyLimits() {
  if (digitalRead(PIN_EMERGENCY_SW) == LOW || currentTemperature > 260.0f) {
    isEmergencyTriggered = true;
    digitalWrite(PIN_SSR_HEATER, LOW);
    digitalWrite(PIN_FAN_RELAY, HIGH);
    digitalWrite(PIN_BUZZER, HIGH);
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("FIRIN KONTROL PANEL");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  display.setCursor(0, 16);
  display.printf("Sicaklik: %.1f C\n", currentTemperature);
  display.setCursor(0, 28);
  display.printf("Hedef   : %.1f C\n", targetTemperature);
  display.setCursor(0, 40);
  display.printf("Durum   : %s\n", isEmergencyTriggered ? "! ACIL DURUM !" : (isSystemActive ? "CALISIYOR" : "BEKLEMEDE"));
  
  display.setCursor(0, 52);
  display.printf("WiFi/MQTT: %s", mqttClient.connected() ? "BAGLI" : "KOPUK");
  display.display();
}

void setupWiFi() {
  Serial.printf("[WIFI] %s agina baglaniliyor...", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.printf("\n[WIFI] Baglandi! IP Adresi: %s\n", WiFi.localIP().toString().c_str());
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("[MQTT] Baglanti kuruluyor...");
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.println(" Baglandi!");
      mqttClient.subscribe(TOPIC_CMD_SUB);
    } else {
      Serial.printf(" Hata kitle kodu: %d. 4sn sonra tekrar...\n", mqttClient.state());
      delay(4000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  Serial.printf("[MQTT ALINDI] %s: %s\n", topic, message.c_str());

  if (message == "START") isSystemActive = true;
  else if (message == "STOP") isSystemActive = false;
  else if (message == "RESET_ALARM") isEmergencyTriggered = false;
  else if (message.startsWith("SET_TEMP:")) {
    targetTemperature = message.substring(9).toFloat();
  }
}\n