#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>

// ==================== LORA CONFIG ====================
#define NODE_ID 1   // Sensor Node
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   4
#define LORA_DIO0  26
#define LORA_BAND  433E6  // RA-02 typical

// ==================== SENSOR ADDRESSES ====================
#define AHT20_ADDR 0x38
#define BH1750_ADDR 0x5C

// ==================== TIMERS ====================
unsigned long lastSensorSend = 0;
const unsigned long SENSOR_INTERVAL = 5000; // Send every 5s

// ==================== SENSOR FUNCTIONS ====================
float readBH1750() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10); // High-res mode
  Wire.endTransmission();
  delay(180);
  Wire.requestFrom(BH1750_ADDR, 2);
  if (Wire.available() == 2) {
    uint16_t raw = (Wire.read() << 8) | Wire.read();
    return raw / 1.2;
  }
  return -1;
}

bool readAHT20(float *tempC, float *humidity) {
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(80);

  Wire.requestFrom(AHT20_ADDR, 6);
  if (Wire.available() != 6) return false;

  uint8_t data[6];
  for (int i = 0; i < 6; i++) data[i] = Wire.read();

  uint32_t hum_raw = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((uint32_t)(data[3] >> 4));
  uint32_t temp_raw = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | ((uint32_t)data[5]);

  *humidity = (hum_raw / 1048576.0) * 100.0;
  *tempC   = (temp_raw / 1048576.0) * 200.0 - 50.0;

  return true;
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  while (!Serial);

  // LoRa setup
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("LoRa init failed!");
    while (1);
  }
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setTxPower(17);
  Serial.println("LoRa init OK");

  // Sensor setup
  Wire.begin(21, 22);
  Serial.println("Sensors initialized");
}

// ==================== LOOP ====================
void loop() {
  if (millis() - lastSensorSend > SENSOR_INTERVAL) {
    lastSensorSend = millis();

    float temp=0, hum=0;
    float lux = readBH1750();
    readAHT20(&temp, &hum);

    // Format message: temp|hum|lux
    String msg = String(temp, 1) + "|" + String(hum, 1) + "|" + String(lux, 1);

    LoRa.beginPacket();
    LoRa.print(msg);
    LoRa.endPacket();

    Serial.println("📤 Sent: " + msg);
  }
}
