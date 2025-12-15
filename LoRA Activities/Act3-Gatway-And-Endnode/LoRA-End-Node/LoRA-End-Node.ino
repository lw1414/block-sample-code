#include <SPI.h>
#include <LoRa.h>

// =====================================================
// DEVICE INFO (CHANGE PER NODE)
// =====================================================
#define DEV_EUI   "NODE_001"
#define APP_KEY   "APPKEY123" // change to avoid traffic to otheers

// =====================================================
// LORA PINS (RA-02)
// =====================================================
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   4
#define LORA_DIO0  26

#define LORA_BAND  433E6

// =====================================================
// SESSION VARIABLES (AFTER JOIN)
// =====================================================
bool joined = false;
String devAddr = "";
String sessionKey = "";
uint16_t seq = 0;

// =====================================================
void setup() {
  Serial.begin(115200);
  while (!Serial);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("LoRa init failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setTxPower(17);

  Serial.println("📡 End Device Started");
  sendJoinRequest();
}

// =====================================================
void loop() {
  receiveLoRa();

  if (joined && Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    if (msg.length()) {
      sendData(msg);
    }
  }
}

// =====================================================
// JOIN REQUEST
// =====================================================
void sendJoinRequest() {
  Serial.println("➡️ Sending JOIN REQUEST");

  LoRa.beginPacket();
  LoRa.print("JOIN_REQ|");
  LoRa.print(DEV_EUI);
  LoRa.print("|");
  LoRa.print(APP_KEY);
  LoRa.endPacket();
}

// =====================================================
// SEND DATA
// =====================================================
void sendData(String payload) {
  seq++;

  LoRa.beginPacket();
  LoRa.print("DATA|");
  LoRa.print(devAddr);
  LoRa.print("|");
  LoRa.print(seq);
  LoRa.print("|");
  LoRa.print(payload);
  LoRa.print("|");
  LoRa.print(sessionKey);
  LoRa.endPacket();

  Serial.print("📤 Sent: ");
  Serial.println(payload);
}

// =====================================================
// RECEIVE
// =====================================================
void receiveLoRa() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  Serial.print("📩 RX: ");
  Serial.println(msg);

  // JOIN ACCEPT
  if (msg.startsWith("JOIN_ACCEPT")) {
    int p1 = msg.indexOf('|');
    int p2 = msg.indexOf('|', p1 + 1);

    devAddr = msg.substring(p1 + 1, p2);
    sessionKey = msg.substring(p2 + 1);

    joined = true;
    Serial.println("✅ JOIN SUCCESS");
    Serial.print("DevAddr: ");
    Serial.println(devAddr);
  }

  // ACK
  if (msg.startsWith("ACK")) {
    Serial.println("✅ ACK RECEIVED");
  }
}
