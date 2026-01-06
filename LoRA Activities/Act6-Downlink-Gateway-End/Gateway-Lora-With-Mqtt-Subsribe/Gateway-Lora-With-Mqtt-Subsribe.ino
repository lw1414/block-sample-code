#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <LoRa.h>

// ================================
// LORA CONFIGURATION
// ================================
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   4
#define LORA_DIO0  26
#define LORA_BAND  433E6

// ================================
// WIFI + MQTT CONFIGURATION
// ================================
const char* ssid     = "PLDTinnov";
const char* password = "Password12345!";

const char* mqtt_server = "innovph.com";
const int   mqtt_port   = 1883;
const char* mqtt_user   = "mqtt";
const char* mqtt_pass   = "ICPHmqtt!";

// ================================
// DEVICE CONFIGURATION
// ================================
String deviceId = "esp01";       // Dynamic device ID
String nodeAddr = "NODE_001";    // LoRa end node address

char button1Topic[64];
char button2Topic[64];

// ================================
// CLIENTS
// ================================
WiFiClient espClient;
PubSubClient mqtt(espClient);

// ================================
// TIMERS
// ================================
unsigned long previousMillis = 0;

// ================================
// SETUP
// ================================
void setup() {
  Serial.begin(115200);
  while(!Serial);

  // Initialize LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("❌ LoRa init failed!");
    while (1);
  }
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setTxPower(17);
  Serial.printf("[%lu ms] ✅ LoRa Gateway Started\n", millis());

  // Initialize WiFi
  WiFi.begin(ssid, password);
  mqtt.setServer(mqtt_server, mqtt_port);

  // Prepare dynamic MQTT topics using deviceId
  snprintf(button1Topic, sizeof(button1Topic), "LoRa/act6/%s/button1", deviceId.c_str());
  snprintf(button2Topic, sizeof(button2Topic), "LoRa/act6/%s/button2", deviceId.c_str());
  Serial.printf("[%lu ms] Button1 Topic: %s\n", millis(), button1Topic);
  Serial.printf("[%lu ms] Button2 Topic: %s\n", millis(), button2Topic);

  // Set MQTT callback
  mqtt.setCallback(mqttCallback);
}

// ================================
// MAIN LOOP
// ================================
void loop() {
  unsigned long currentMillis = millis();

  // WiFi reconnect
  if (WiFi.status() != WL_CONNECTED) {
    if (currentMillis - previousMillis >= 2000) {
      Serial.printf("[%lu ms] Connecting to WiFi...\n", millis());
      WiFi.begin(ssid, password);
      previousMillis = currentMillis;
    }
    return;
  }

  // MQTT reconnect
  if (!mqtt.connected()) {
    if (currentMillis - previousMillis >= 2000) {
      connectMQTT();
      previousMillis = currentMillis;
    }
    return;
  }

  mqtt.loop();
  receiveLoRa(); // Monitor LoRa data from nodes
}

// ================================
// MQTT CALLBACK
// ================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String msg = String((char*)payload);
  msg.trim();

  Serial.printf("[%lu ms] MQTT RX -> Topic: %s | Payload: %s\n", millis(), topic, msg.c_str());

  // Forward to LoRa end node
  String cmd = "CMD|" + nodeAddr + "|";
  if (String(topic) == button1Topic) cmd += "RELAY1|" + msg;
  else if (String(topic) == button2Topic) cmd += "RELAY2|" + msg;
  else return;

  LoRa.beginPacket();
  LoRa.print(cmd);
  LoRa.endPacket();
  Serial.printf("[%lu ms] LoRa TX -> %s\n", millis(), cmd.c_str());
}

// ================================
// MQTT CONNECT
// ================================
void connectMQTT() {
  Serial.printf("[%lu ms] Connecting to MQTT...\n", millis());
  if (mqtt.connect(deviceId.c_str(), mqtt_user, mqtt_pass)) {
    Serial.printf("[%lu ms] MQTT Connected!\n", millis());
    mqtt.subscribe(button1Topic);
    mqtt.subscribe(button2Topic);
    Serial.printf("[%lu ms] Subscribed to: %s\n", millis(), button1Topic);
    Serial.printf("[%lu ms] Subscribed to: %s\n", millis(), button2Topic);
  } else {
    Serial.printf("[%lu ms] MQTT Failed, rc=%d\n", millis(), mqtt.state());
  }
}

// ================================
// RECEIVE LORA
// ================================
void receiveLoRa() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  Serial.printf("[%lu ms] LoRa RX -> %s\n", millis(), msg.c_str());
}
