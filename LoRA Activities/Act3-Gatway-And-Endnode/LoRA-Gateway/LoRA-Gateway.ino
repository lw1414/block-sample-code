#include <SPI.h>
#include <LoRa.h>

// =====================================================
// GATEWAY SECURITY
// =====================================================
#define APP_KEY "APPKEY123"// change to avoid traffic on others

// =====================================================
// LORA PINS
// =====================================================
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   4
#define LORA_DIO0  26

#define LORA_BAND  433E6

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

  Serial.println("🟢 Gateway Started");
}

// =====================================================
void loop() {
  receiveLoRa();
}

// =====================================================
void receiveLoRa() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  Serial.print("📩 RX: ");
  Serial.println(msg);

  // JOIN REQUEST
  if (msg.startsWith("JOIN_REQ")) {
    handleJoin(msg);
  }

  // DATA
  if (msg.startsWith("DATA")) {
    handleData(msg);
  }
}

// =====================================================
// HANDLE JOIN
// =====================================================
void handleJoin(String msg) {
  int p1 = msg.indexOf('|');
  int p2 = msg.indexOf('|', p1 + 1);

  String devEUI = msg.substring(p1 + 1, p2);
  String key = msg.substring(p2 + 1);

  if (key != APP_KEY) {
    Serial.println("❌ Invalid APP KEY");
    return;
  }

  String devAddr = "A1B2";          // Example
  String sessionKey = "SESS123";    // Example

  LoRa.beginPacket();
  LoRa.print("JOIN_ACCEPT|");
  LoRa.print(devAddr);
  LoRa.print("|");
  LoRa.print(sessionKey);
  LoRa.endPacket();

  Serial.println("✅ JOIN ACCEPT SENT");
}

// =====================================================
// HANDLE DATA
// =====================================================
void handleData(String msg) {
  int p1 = msg.indexOf('|');
  int p2 = msg.indexOf('|', p1 + 1);
  int p3 = msg.indexOf('|', p2 + 1);
  int p4 = msg.indexOf('|', p3 + 1);

  String devAddr = msg.substring(p1 + 1, p2);
  String seq = msg.substring(p2 + 1, p3);
  String payload = msg.substring(p3 + 1, p4);
  String sessKey = msg.substring(p4 + 1);

  Serial.print("📦 Data from ");
  Serial.print(devAddr);
  Serial.print(": ");
  Serial.println(payload);

  // ACK
  LoRa.beginPacket();
  LoRa.print("ACK|");
  LoRa.print(devAddr);
  LoRa.print("|");
  LoRa.print(seq);
  LoRa.endPacket();

  Serial.println("✅ ACK SENT");
}
