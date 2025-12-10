#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <WebServer.h>

// ---------------------------
// HARD-CODED WIFI CONFIG
// ---------------------------
const char* ssid = "TECNO POVA 7";
const char* password = "nes141414";

// ---------------------------
// Sensor Addresses
// ---------------------------
#define AHT20_ADDR 0x38
#define BH1750_ADDR 0x5C

// ---------------------------
// Hardware Pins
// ---------------------------
#define LED_PIN 14
#define NUM_LEDS 10
#define BUZZER_PIN 13

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------------------------
// Timing Variables
// ---------------------------
unsigned long lastLedUpdate = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastBuzzerToggle = 0;
int spinningIndex = 0;

const unsigned long SENSOR_INTERVAL = 5000;  // 5 seconds
const unsigned long BUZZER_INTERVAL = 500;

// ---------------------------
// Thresholds for buzzer alarm
// ---------------------------
float TEMP_THRESHOLD = 30.0;    // °C
float HUM_THRESHOLD = 70.0;     // %
float LIGHT_THRESHOLD = 800.0;  // lux

bool buzzerActive = false;
const int BUZZER_FREQ = 1000;  // 1 kHz

// LED Colors
uint32_t colorWiFi = strip.Color(0, 0, 60);   // Blue
uint32_t colorReady = strip.Color(0, 60, 0);  // Green
uint32_t colorError = strip.Color(60, 0, 0);  // Red

// ---------------------------
// Web server
// ---------------------------
WebServer server(80);

// Latest sensor values
float tempValue = 0;
float humValue = 0;
float luxValue = 0;

// ---------------------------
// Helper: Fill all LEDs
// ---------------------------
void setAll(uint32_t color) {
  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, color);
  strip.show();
}

// ---------------------------
// Spinning LED effect (millis-based)
// ---------------------------
void spinningRing(uint32_t color, int speed = 100) {
  unsigned long now = millis();
  if (now - lastLedUpdate > speed) {
    lastLedUpdate = now;
    strip.clear();
    strip.setPixelColor(spinningIndex, color);
    strip.show();
    spinningIndex = (spinningIndex + 1) % NUM_LEDS;
  }
}

// ---------------------------
// Sensor Reads
// ---------------------------
float readBH1750() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10);  // Continuous H-Resolution mode
  Wire.endTransmission();
  delay(180);
  Wire.requestFrom(BH1750_ADDR, 2);
  if (Wire.available() == 2) {
    uint16_t raw = Wire.read() << 8 | Wire.read();
    return raw / 1.2;
  }
  return -1;
}

bool readAHT20(float* tempC, float* humidity) {
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
    ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((uint32_t)(data[3] >> 4));

  uint32_t temp_raw =
    (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | ((uint32_t)data[5]);

  *humidity = ((float)hum_raw / 1048576.0) * 100.0;
  *tempC = ((float)temp_raw / 1048576.0) * 200.0 - 50.0;

  return true;
}

// ---------------------------
// Webpage handler
// ---------------------------
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>ESP32 Sensor Dashboard</title>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<style>";
  html += "body{font-family:Arial, sans-serif; background:#111; color:#eee; text-align:center;}";
  html += "h1{color:#0f0;}";
  html += ".card{display:inline-block; margin:20px; padding:20px; border-radius:10px; width:180px; font-size:1.2em;}";
  html += ".temp{background:" + String(tempValue >= TEMP_THRESHOLD ? "#ff4d4d" : "#33cc33") + ";}";
  html += ".hum{background:" + String(humValue >= HUM_THRESHOLD ? "#ff4d4d" : "#3399ff") + ";}";
  html += ".light{background:" + String(luxValue >= LIGHT_THRESHOLD ? "#ff4d4d" : "#ffcc33") + ";}";
  html += "</style></head><body>";
  html += "<h1>ESP32 Sensor Dashboard</h1>";
  html += "<div class='card temp'>Temperature<br>" + String(tempValue,1) + " °C</div>";
  html += "<div class='card hum'>Humidity<br>" + String(humValue,1) + " %</div>";
  html += "<div class='card light'>Light<br>" + String(luxValue,1) + " lux</div>";
  html += "<p>ESP32 IP: " + WiFi.localIP().toString() + "</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}


// ---------------------------
// Setup
// ---------------------------
void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.show();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(21, 22);  // I2C SDA, SCL

  // Init AHT20
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xBE);
  Wire.write(0x08);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(50);

  Serial.println("Sensors Initialized");

  // Connect WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  // Wait for WiFi to connect and print IP
  while (WiFi.status() != WL_CONNECTED) {
    spinningRing(colorWiFi, 150);
    delay(100);
  }
  Serial.println();
  Serial.print("WiFi connected! ESP32 IP address: ");
  Serial.println(WiFi.localIP());  // <-- This prints your ESP32 local IP


  // Start web server
  server.on("/", handleRoot);
  server.begin();
}

// ---------------------------
// Loop
// ---------------------------
void loop() {
  unsigned long now = millis();

  // Handle WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    spinningRing(colorWiFi, 150);
    return;
  } else {
    setAll(colorReady);
  }

  // Handle web server requests
  server.handleClient();

  // --- Sensor read every SENSOR_INTERVAL ---
  if (now - lastSensorUpdate > SENSOR_INTERVAL) {
    lastSensorUpdate = now;

    if (readAHT20(&tempValue, &humValue)) {
      Serial.printf("Temp: %.1f °C, Hum: %.1f %%\n", tempValue, humValue);
    }

    float lux = readBH1750();
    if (lux >= 0) {
      luxValue = lux;
      Serial.printf("Light: %.1f lux\n", luxValue);
    }

    // --- Check thresholds for buzzer ---
    buzzerActive = false;
    if (tempValue >= TEMP_THRESHOLD || humValue >= HUM_THRESHOLD || luxValue >= LIGHT_THRESHOLD) {
      buzzerActive = true;
    }
  }

  // --- Passive buzzer alarm using tone() (millis-based) ---
  if (buzzerActive) {
    if (now - lastBuzzerToggle > BUZZER_INTERVAL) {
      lastBuzzerToggle = now;
      static bool buzzerOn = false;
      buzzerOn = !buzzerOn;
      if (buzzerOn) {
        tone(BUZZER_PIN, BUZZER_FREQ);
      } else {
        noTone(BUZZER_PIN);
      }
    }
  } else {
    noTone(BUZZER_PIN);
  }
}
