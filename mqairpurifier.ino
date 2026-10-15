#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// --- KREDENSIAL WIFI ---
const char* ssid     = "airpurifier";
const char* password = "password123";

// --- PIN DEFINITIONS (ESP32-C3 Super Mini) ---
#define PIN_MQ135      0
#define PIN_DHT        2
#define PIN_RELAY      3
#define PIN_LED_GREEN  4
#define PIN_LED_YELLOW 5
#define PIN_LED_RED    6

// TIPE LOGIKA RELAY (Active-LOW: LOW = ON, HIGH = OFF)
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);

// --- IDENTITAS ALAT & TOPIK DINAMIS ---
const char* device_id = "purifier-01"; 

// --- HIVEMQ PUBLIC BROKER (PORT 1883 TCP, TANPA USER/PASS) ---
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port   = 1883;

String topic_status  = String("smart-purifier/") + device_id + "/status";
String topic_control = String("smart-purifier/") + device_id + "/control";
String topic_config  = String("smart-purifier/") + device_id + "/config";

WiFiClient espClient; // Menggunakan TCP biasa (ringan & stabil)
PubSubClient client(espClient);
Preferences prefs;

// --- PARAMETER SISTEM & TIMER ---
unsigned long lastTelemetry = 0;
unsigned long lastMqttRetry = 0;
unsigned long lastWiFiRetry = 0;

bool fanState = false;
bool autoMode = true;

int airOffset = 0;
// Ambang batas disesuaikan untuk ADC 12-bit MQ-135
int thresholdModerate = 600;
int thresholdBad = 1200;

void setLEDs(bool g, bool y, bool r) {
  digitalWrite(PIN_LED_GREEN, g ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW, y ? HIGH : LOW);
  digitalWrite(PIN_LED_RED, r ? HIGH : LOW);
}

// Menerima perintah kontrol atau kalibrasi dari web
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;

  String strTopic = String(topic);

  if (strTopic == topic_control) {
    if (doc.containsKey("mode")) {
      autoMode = (doc["mode"] == "auto");
    }
    if (doc.containsKey("fan")) {
      fanState = doc["fan"];
      digitalWrite(PIN_RELAY, fanState ? RELAY_ON : RELAY_OFF);
    }
  } 
  else if (strTopic == topic_config) {
    if (doc.containsKey("offset")) airOffset = doc["offset"];
    if (doc.containsKey("th_mod")) thresholdModerate = doc["th_mod"];
    if (doc.containsKey("th_bad")) thresholdBad = doc["th_bad"];

    // Simpan permanen ke Flash Memory NVS
    prefs.begin("calib", false);
    prefs.putInt("offset", airOffset);
    prefs.putInt("th_mod", thresholdModerate);
    prefs.putInt("th_bad", thresholdBad);
    prefs.end();
    Serial.println("[Config] Kalibrasi baru disimpan ke Flash Memory.");
  }
}

void checkWiFiConnection() {
  if (WiFi.status() == WL_CONNECTED) return;

  unsigned long now = millis();
  if (now - lastWiFiRetry > 8000) {
    lastWiFiRetry = now;
    Serial.println("[WiFi] Menyambung ke SSID 'airpurifier'...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
  }
}

void checkMqttConnection() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (client.connected()) return;

  unsigned long now = millis();
  if (now - lastMqttRetry > 4000) {
    lastMqttRetry = now;
    Serial.println("[MQTT] Menghubungkan ke broker.hivemq.com:1883...");
    
    // Client ID acak untuk broker publik
    String clientId = "ESP32C3-Pub-" + String(device_id) + "-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("[MQTT] Berhasil Terhubung ke HiveMQ Public!");
      client.subscribe(topic_control.c_str());
      client.subscribe(topic_config.c_str());
    } else {
      Serial.print("[MQTT] Gagal konek, error: ");
      Serial.println(client.state());
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- Starting Smart Air Purifier (HiveMQ Public) ---");

  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  digitalWrite(PIN_RELAY, RELAY_OFF);

  // Self-Test LED saat pertama menyala
  setLEDs(true, false, false); delay(200);
  setLEDs(false, true, false); delay(200);
  setLEDs(false, false, true); delay(200);
  setLEDs(false, false, false);

  analogReadResolution(12);
  dht.begin();

  // Load konfigurasi kalibrasi jika ada
  prefs.begin("calib", true);
  airOffset = prefs.getInt("offset", 0);
  thresholdModerate = prefs.getInt("th_mod", 600);
  thresholdBad = prefs.getInt("th_bad", 1200);
  prefs.end();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // HiveMQ Public Port 1883 (Standard TCP)
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  client.setKeepAlive(15);
}

void loop() {
  checkWiFiConnection();
  checkMqttConnection();
  
  if (client.connected()) {
    client.loop();
  }

  // Rutinitas Sensor & Kipas (Offline & Online)
  unsigned long currentMillis = millis();
  if (currentMillis - lastTelemetry >= 2000) {
    lastTelemetry = currentMillis;

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    // Sampling ADC MQ-135 agar tidak berfluktuasi liar
    long adcSum = 0;
    for (int i = 0; i < 10; i++) {
      adcSum += analogRead(PIN_MQ135);
      delay(3);
    }
    int rawAnalog = adcSum / 10;
    int airQuality = rawAnalog + airOffset;
    if (airQuality < 0) airQuality = 0;

    // Logika Indikator LED & Kipas Otomatis
    if (airQuality < thresholdModerate) {
      setLEDs(true, false, false); // Hijau (Bersih)
      if (autoMode) fanState = false;
    } else if (airQuality < thresholdBad) {
      setLEDs(false, true, false); // Kuning (Sedang)
      if (autoMode) fanState = true;
    } else {
      setLEDs(false, false, true); // Merah (Buruk/Bahaya)
      if (autoMode) fanState = true;
    }

    digitalWrite(PIN_RELAY, fanState ? RELAY_ON : RELAY_OFF);

    // Cetak ke Serial Monitor untuk memantau nilai
    Serial.printf("[Sensor] Udara: %d | Suhu: %.1f C | Hum: %.1f %% | Kipas: %s | WiFi: %s\n",
                  airQuality, 
                  isnan(t) ? 0.0 : t, 
                  isnan(h) ? 0.0 : h, 
                  fanState ? "ON" : "OFF",
                  (WiFi.status() == WL_CONNECTED) ? "Connected" : "Disconnected");

    // Kirim telemetri jika MQTT tersambung
    if (client.connected()) {
      StaticJsonDocument<256> doc;
      doc["temp"] = isnan(t) ? 0.0 : ((int)(t * 10)) / 10.0;
      doc["hum"] = isnan(h) ? 0.0 : ((int)(h * 10)) / 10.0;
      doc["air_quality"] = airQuality;
      doc["fan_state"] = fanState;
      doc["mode"] = autoMode ? "auto" : "manual";

      char jsonBuffer[256];
      serializeJson(doc, jsonBuffer);
      client.publish(topic_status.c_str(), jsonBuffer);
      Serial.println("[MQTT] Data telemetri terkirim!");
    }
  }
}
