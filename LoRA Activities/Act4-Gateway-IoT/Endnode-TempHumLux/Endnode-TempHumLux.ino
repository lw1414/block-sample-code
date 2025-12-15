#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>

/* =====================================================
   DEVICE INFO
   ===================================================== */
#define DEV_EUI   "NODE_001"
#define APP_KEY   "APPKEY123"

/* =====================================================
   LORA PINS (RA-02)
   ===================================================== */
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   4
#define LORA_DIO0  26
#define LORA_BAND  433E6

/* =====================================================
   I2C SENSOR ADDRESSES
   ===================================================== */
#define AHT20_ADDR   0x38
#define BH1750_ADDR  0x5C

/* =====================================================
   SESSION VARIABLES
   ===================================================== */
bool joined = false;
String devAddr = "";
String sessionKey = "";
uint16_t seq = 0;

/* =====================================================
   SEND INTERVAL CONFIG
   ===================================================== */
const unsigned long SEND_INTERVAL = 5000; // <-- CHANGE HERE (ms)
unsigned long lastSend = 0;

/* =====================================================
   SETUP
   ===================================================== */
void setup() {
  Serial.begin(115200);

  // --- I2C ---
  Wire.begin(21, 22);

  // AHT20 init
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xBE);
  Wire.write(0x08);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(50);

  // --- LoRa ---
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("❌ LoRa init failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setTxPower(17);

  Serial.println("📡 End Device Started");
  sendJoinRequest();
}

/* =====================================================
   LOOP
   ===================================================== */
void loop() {

  receiveLoRa();

  if (!joined) return;

  unsigned long now = millis();

  if (now - lastSend >= SEND_INTERVAL) {
    lastSend = now;
    sendSensorData();
  }
}

/* =====================================================
   JOIN REQUEST
   ===================================================== */
void sendJoinRequest() {
  Serial.println("➡️ Sending JOIN REQUEST");

  LoRa.beginPacket();
  LoRa.print("JOIN_REQ|");
  LoRa.print(DEV_EUI);
  LoRa.print("|");
  LoRa.print(APP_KEY);
  LoRa.endPacket();
}

/* =====================================================
   SEND SENSOR DATA
   ===================================================== */
void sendSensorData() {

  float temp, hum, lux;

  if (!readAHT20(&temp, &hum)) {
    Serial.println("❌ AHT20 read failed");
    return;
  }

  lux = readBH1750();

  seq++;

  char payload[64];
  snprintf(payload, sizeof(payload),
           "T=%.1f,H=%.1f,L=%.1f", temp, hum, lux);

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

  Serial.print("📤 Sent @ ");
  Serial.print(millis() / 1000);
  Serial.print("s → ");
  Serial.println(payload);
}

/* =====================================================
   RECEIVE LORA
   ===================================================== */
void receiveLoRa() {

  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  Serial.print("📩 RX: ");
  Serial.println(msg);

  if (msg.startsWith("JOIN_ACCEPT")) {
    int p1 = msg.indexOf('|');
    int p2 = msg.indexOf('|', p1 + 1);

    devAddr = msg.substring(p1 + 1, p2);
    sessionKey = msg.substring(p2 + 1);

    joined = true;
    lastSend = millis();  // reset interval on join

    Serial.println("✅ JOIN SUCCESS");
  }

  if (msg.startsWith("ACK")) {
    Serial.println("✅ ACK RECEIVED");
  }
}

/* =====================================================
   SENSOR FUNCTIONS
   ===================================================== */
bool readAHT20(float *tempC, float *humidity) {

  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(80);

  Wire.requestFrom(AHT20_ADDR, 6);
  if (Wire.available() != 6) return false;

  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();

  uint32_t hum_raw =
    ((uint32_t)d[1] << 12) |
    ((uint32_t)d[2] << 4) |
    (d[3] >> 4);

  uint32_t temp_raw =
    (((uint32_t)d[3] & 0x0F) << 16) |
    ((uint32_t)d[4] << 8) |
    d[5];

  *humidity = (hum_raw / 1048576.0) * 100.0;
  *tempC    = (temp_raw / 1048576.0) * 200.0 - 50.0;

  return true;
}

float readBH1750() {

  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10);
  Wire.endTransmission();
  delay(180);

  Wire.requestFrom(BH1750_ADDR, 2);
  if (Wire.available() == 2) {
    uint16_t raw = (Wire.read() << 8) | Wire.read();
    return raw / 1.2;
  }
  return -1;
}
