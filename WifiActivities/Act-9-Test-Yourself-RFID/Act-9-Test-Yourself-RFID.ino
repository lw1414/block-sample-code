/************************************************************
  ESP32 RFID + MQTT (Teaching Version)

  WHAT THIS CODE DOES:
  1. Connects ESP32 to WiFi
  2. Connects to an MQTT broker
  3. Reads an RFID card UID using MFRC522
  4. Sounds a buzzer when a card is detected
  5. Publishes the UID to an MQTT topic
  6. Uses millis() instead of delay() for timing
************************************************************/

// ==================== LIBRARIES ====================
// SPI library is needed for RFID communication
#include <SPI.h>

// MFRC522 library for RFID reader
#include <MFRC522.h>

// WiFi library for ESP32 internet connection
#include <WiFi.h>

// MQTT client library
#include <PubSubClient.h>

// ==================== PIN DEFINITIONS ====================
// These pins define how the RFID and buzzer are wired
#define SS_PIN     5     // RFID SDA / SS pin
#define RST_PIN    27    // RFID Reset pin
#define BUZZER_PIN 13    // Active buzzer pin

// ==================== WIFI CREDENTIALS ====================
// Change these to match your WiFi network
const char* ssid     = "WIFI";
const char* password = "PASS1234";

// ==================== MQTT SETTINGS ====================
// MQTT broker information
const char* mqtt_server   = "innovph.com";
const int   mqtt_port     = 1883;
const char* mqtt_user     = "mqtt";
const char* mqtt_password = "ICPHmqtt!";

// 🔹 DEVICE ID
// This identifies this ESP32 on the MQTT broker
// Changing this changes the MQTT topic automatically
const char* deviceId = "esp02";

// Buffer to store the final MQTT topic string
char mqtt_topic[64];

// ==================== OBJECT CREATION ====================
// Create RFID reader object
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Create WiFi client (used by MQTT)
WiFiClient espClient;

// Create MQTT client
PubSubClient client(espClient);

// ==================== MILLIS TIMERS ====================
// These variables allow non-blocking timing
unsigned long lastScanTime = 0;   // last RFID scan time
unsigned long buzzerStart  = 0;   // when buzzer was turned on
bool buzzerActive          = false;

// Timing constants
const unsigned long SCAN_INTERVAL   = 1000; // 1 second between scans
const unsigned long BUZZER_DURATION = 100;  // buzzer ON time (ms)

// ==================== MQTT CONNECT FUNCTION ====================
// This function keeps trying until MQTT is connected
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");

    // Attempt to connect using deviceId as client name
    if (client.connect(deviceId, mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      // If failed, print error code
      Serial.print("failed (rc=");
      Serial.print(client.state());
      Serial.println("), retrying...");
      delay(2000); // Delay is OK here (only reconnect logic)
    }
  }
}

// ==================== SETUP FUNCTION ====================
// Runs ONCE when ESP32 boots
void setup() {
  Serial.begin(115200);  // Start Serial Monitor

  // Setup buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // buzzer OFF

  // Build MQTT topic dynamically
  // Example result: WiFi/act9/esp02/rfid
  snprintf(mqtt_topic, sizeof(mqtt_topic),
           "WiFi/act9/%s/rfid", deviceId);

  Serial.print("MQTT Topic: ");
  Serial.println(mqtt_topic);

  // Initialize SPI bus for RFID
  SPI.begin(18, 19, 23, SS_PIN);

  // Initialize RFID reader
  mfrc522.PCD_Init();

  // ==================== WIFI CONNECTION ====================
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);

  // Wait until WiFi is connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); // OK in setup
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // ==================== MQTT SETUP ====================
  client.setServer(mqtt_server, mqtt_port);
  connectMQTT();

  Serial.println("RFID Ready. Tap card...");
}

// ==================== LOOP FUNCTION ====================
// Runs repeatedly forever
void loop() {
  // Get current system time
  unsigned long currentMillis = millis();

  // Ensure MQTT connection stays alive
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop(); // Required for MQTT background tasks

  // ==================== BUZZER CONTROL ====================
  // Turn buzzer OFF after duration expires
  if (buzzerActive && (currentMillis - buzzerStart >= BUZZER_DURATION)) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }

  // ==================== RFID SCAN COOLDOWN ====================
  // Prevents multiple scans from the same card
  if (currentMillis - lastScanTime < SCAN_INTERVAL) {
    return;
  }

  // ==================== RFID DETECTION ====================
  // Check if a new card is present
  if (!mfrc522.PICC_IsNewCardPresent()) return;

  // Read the card UID
  if (!mfrc522.PICC_ReadCardSerial()) return;

  // Update last scan time
  lastScanTime = currentMillis;

  // ==================== BUZZER FEEDBACK ====================
  digitalWrite(BUZZER_PIN, HIGH); // buzzer ON
  buzzerStart  = currentMillis;
  buzzerActive = true;

  // ==================== UID PROCESSING ====================
  // Convert UID bytes to HEX string
  String uidStr = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uidStr += String(mfrc522.uid.uidByte[i], HEX);
  }
  uidStr.toUpperCase(); // make UID readable and consistent

  Serial.print("RFID UID: ");
  Serial.println(uidStr);

  // ==================== MQTT PUBLISH ====================
  client.publish(mqtt_topic, uidStr.c_str());
  Serial.println("UID sent to MQTT");

  // Stop RFID communication properly
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
