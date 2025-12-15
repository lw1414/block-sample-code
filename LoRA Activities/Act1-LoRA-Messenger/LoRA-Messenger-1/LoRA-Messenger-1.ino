#include <SPI.h>
#include <LoRa.h>

// =====================================================
// CHANGE THIS PER DEVICE
// =====================================================
#define NODE_ID 1   // <-- SET TO 1 ON FIRST ESP32, 2 ON SECOND

// =====================================================
// LORA PIN CONFIG (RA-02)
// =====================================================
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   4
#define LORA_DIO0  26

// =====================================================
// LORA SETTINGS
// =====================================================
#define LORA_BAND  433E6   // RA-02 is usually 433 MHz

// =====================================================
void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("\nLoRa Chat Starting...");
  Serial.print("Node ID: ");
  Serial.println(NODE_ID);

  // SPI setup
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);

  // LoRa pin mapping
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // Initialize LoRa
  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("LoRa init failed!");
    while (1);
  }

  // Optional tuning (recommended)
  LoRa.setSpreadingFactor(7);     // SF7–SF12
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setTxPower(17);            // Max for RA-02

  Serial.println("LoRa init OK");
  Serial.println("Type message and press ENTER to send\n");
}

// =====================================================
void loop() {
  receiveLoRa();
  sendFromSerial();
}

// =====================================================
// RECEIVE MESSAGE
// =====================================================
void receiveLoRa() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";

    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    Serial.print("📩 Received: ");
    Serial.println(incoming);

    Serial.print("RSSI: ");
    Serial.println(LoRa.packetRssi());
    Serial.println();
  }
}

// =====================================================
// SEND SERIAL MESSAGE
// =====================================================
void sendFromSerial() {
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    if (msg.length() == 0) return;

    LoRa.beginPacket();
    LoRa.print("Node ");
    LoRa.print(NODE_ID);
    LoRa.print(": ");
    LoRa.print(msg);
    LoRa.endPacket();

    Serial.print("📤 Sent: ");
    Serial.println(msg);
  }
}
