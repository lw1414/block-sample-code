#include <Wire.h>

#define AHT20_ADDR 0x38
#define BH1750_ADDR 0x5C   // your detected BH1750 address
#define MQ136_PIN 32       // analog input for MQ136 gas sensor

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // --- AHT20 INIT ---
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xBE);
  Wire.write(0x08);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(50);

  // Set MQ136 analog pin
  analogReadResolution(12);      // ESP32 default (0-4095)
  pinMode(MQ136_PIN, INPUT);

  Serial.println("AHT20 + BH1750 + MQ136 Ready (NO LIBRARIES)");
}

float readBH1750() {
  // Start continuous H-res mode
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10);
  Wire.endTransmission();
  delay(180);

  Wire.requestFrom(BH1750_ADDR, 2);
  if (Wire.available() == 2) {
    uint16_t raw = Wire.read() << 8 | Wire.read();
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

  uint32_t hum_raw =
      ((uint32_t)data[1] << 12) |
      ((uint32_t)data[2] << 4) |
      ((uint32_t)(data[3] >> 4));

  uint32_t temp_raw =
      (((uint32_t)data[3] & 0x0F) << 16) |
      ((uint32_t)data[4] << 8) |
      ((uint32_t)data[5]);

  *humidity = ((float)hum_raw / 1048576.0) * 100.0;
  *tempC   = ((float)temp_raw / 1048576.0) * 200.0 - 50.0;

  return true;
}

void loop() {
  // --- AHT20 Read ---
  float t, h;
  if (readAHT20(&t, &h)) {
    Serial.print("Temp: ");
    Serial.print(t);
    Serial.print(" °C,  Humidity: ");
    Serial.print(h);
    Serial.println(" %");
  } else {
    Serial.println("AHT20 read error");
  }

  // --- BH1750 Read ---
  float lux = readBH1750();
  if (lux >= 0) {
    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lux");
  } else {
    Serial.println("BH1750 read error");
  }

  // --- MQ136 Analog Read ---
  int mq_value = analogRead(MQ136_PIN);
  Serial.print("MQ136 Analog Value: ");
  Serial.println(mq_value);

  Serial.println("--------------------------");
  delay(1000);
}
