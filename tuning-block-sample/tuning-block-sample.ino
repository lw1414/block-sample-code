// ESP32 Analog Read on GPIO 32 every 3 seconds

const int analogPin = 33;   // GPIO32 = ADC1_CH4 for rotary

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting Analog Read...");
}

void loop() {
  int rawValue = analogRead(analogPin);   // 0–4095 (12-bit)
  float voltage = (rawValue * 3.3) / 4095.0;  // Convert to voltage (approx)

  Serial.print("Raw ADC: ");
  Serial.print(rawValue);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 3);
  Serial.println(" V");

  delay(3000);  // 3 seconds
}
