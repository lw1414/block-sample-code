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
// RELAY PINS
// ================================
#define RELAY1_PIN 14
#define RELAY2_PIN 25

// ================================
// NODE CONFIGURATION
// ================================
String nodeAddr = "NODE_001";   // Must match the gateway
bool relay1State = true;  // Initial state HIGH (swapped)
bool relay2State = true;  // Initial state HIGH (swapped)

// ================================
// SETUP
// ================================
void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Initialize Relays
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  
  // Initial state (swapped logic: HIGH = OFF, LOW = ON)
  digitalWrite(RELAY1_PIN, relay1State ? HIGH : LOW);
  digitalWrite(RELAY2_PIN, relay2State ? HIGH : LOW);

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
  Serial.printf("[%lu ms] ✅ LoRa End Node Started\n", millis());
}

// ================================
// MAIN LOOP
// ================================
void loop() {
  receiveLoRa(); // Continuously check for commands
}

// ================================
// RECEIVE LORA COMMANDS
// ================================
void receiveLoRa() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  Serial.printf("[%lu ms] LoRa RX -> %s\n", millis(), msg.c_str());

  // Expecting format: CMD|NODE_001|RELAY1|ON or CMD|NODE_001|RELAY2|OFF
  if (!msg.startsWith("CMD|")) return;

  int idx1 = msg.indexOf('|');
  int idx2 = msg.indexOf('|', idx1 + 1);
  int idx3 = msg.indexOf('|', idx2 + 1);
  int idx4 = msg.indexOf('|', idx3 + 1);

  String targetNode = msg.substring(idx1 + 1, idx2);
  if (targetNode != nodeAddr) return; // Not for this node

  String relayCmd = msg.substring(idx2 + 1, idx3);
  String action   = msg.substring(idx3 + 1);

  // Swapped logic: ON -> LOW, OFF -> HIGH
  if (relayCmd == "RELAY1") {
    relay1State = (action == "OFF"); // OFF = HIGH, ON = LOW
    digitalWrite(RELAY1_PIN, relay1State ? HIGH : LOW);
    Serial.printf("[%lu ms] RELAY1 -> %s\n", millis(), relay1State ? "ON" : "OFF");
  } else if (relayCmd == "RELAY2") {
    relay2State = (action == "OFF");
    digitalWrite(RELAY2_PIN, relay2State ? HIGH : LOW);
    Serial.printf("[%lu ms] RELAY2 -> %s\n", millis(), relay2State ? "ON" : "OFF");
  }

  // Send ACK back to Gateway
  String ackMsg = "ACK|" + nodeAddr + "|" + relayCmd + "|" + action;
  LoRa.beginPacket();
  LoRa.print(ackMsg);
  LoRa.endPacket();
  Serial.printf("[%lu ms] LoRa TX -> %s\n", millis(), ackMsg.c_str());
}
