#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ==================== PIN DEFINITIONS ====================
#define SS_PIN     5
#define RST_PIN    27
#define BUZZER_PIN 13

// ==================== WIFI CREDENTIALS ====================
const char* ssid     = "WIFI";
const char* password = "PASS1234";

// ==================== MQTT SETTINGS ====================
const char* mqtt_server   = "innovph.com";
const int   mqtt_port     = 1883;
const char* mqtt_user     = "mqtt";
const char* mqtt_password = "ICPHmqtt!";

// 🔹 CHANGE THIS ONLY
const char* deviceId = "esp02";

// MQTT topic buffer
char mqtt_topic[64];

// ==================== OBJECTS ====================
MFRC522 mfrc522(SS_PIN, RST_PIN);
WiFiClient espClient;
PubSubClient client(espClient);

// ==================== MILLIS TIMERS ====================
unsigned long lastScanTime   = 0;
unsigned long buzzerStart    = 0;
bool buzzerActive            = false;

const unsigned long SCAN_INTERVAL   = 1000; // 1 second
const unsigned long BUZZER_DURATION = 100;  // 100 ms

// ==================== FUNCTION ====================
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect(deviceId, mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed (rc=");
      Serial.print(client.state());
      Serial.println("), retrying...");
      delay(2000); // OK here: reconnect logic only
    }
  }
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Build MQTT topic
  snprintf(mqtt_topic, sizeof(mqtt_topic),
           "WiFi/act9/%s/rfid", deviceId);

  Serial.print("MQTT Topic: ");
  Serial.println(mqtt_topic);

  // RFID SPI init
  SPI.begin(18, 19, 23, SS_PIN);
  mfrc522.PCD_Init();

  // ==================== WIFI CONNECT ====================
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); // acceptable in setup
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // ==================== MQTT ====================
  client.setServer(mqtt_server, mqtt_port);
  connectMQTT();

  Serial.println("RFID Ready. Tap card...");
}

// ==================== LOOP ====================
void loop() {
  unsigned long currentMillis = millis();

  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();

  // ==================== BUZZER CONTROL ====================
  if (buzzerActive && (currentMillis - buzzerStart >= BUZZER_DURATION)) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }

  // ==================== RFID SCAN COOLDOWN ====================
  if (currentMillis - lastScanTime < SCAN_INTERVAL) {
    return;
  }

  // ==================== RFID CHECK ====================
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  lastScanTime = currentMillis;

  // Start buzzer
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerStart  = currentMillis;
  buzzerActive = true;

  // Convert UID to string
  String uidStr = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uidStr += String(mfrc522.uid.uidByte[i], HEX);
  }
  uidStr.toUpperCase();

  Serial.print("RFID UID: ");
  Serial.println(uidStr);

  // Publish UID
  client.publish(mqtt_topic, uidStr.c_str());
  Serial.println("UID sent to MQTT");

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
