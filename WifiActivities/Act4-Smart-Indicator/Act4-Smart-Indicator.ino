#include <Wire.h>               // Library for I2C communication (AHT20 & BH1750 sensors)
#include <Adafruit_GFX.h>       // Graphics library (text, shapes)
#include <Adafruit_GC9A01A.h>   // Driver for the round GC9A01A LCD
#include <Adafruit_NeoPixel.h>  // Library for controlling RGB LED strip
#include <SPI.h>                // SPI communication for LCD

// -----------------------------------------------------------
//  SECTION 1 — LCD PIN DEFINITIONS
//  These are SPI pins used to communicate with the round LCD.
// -----------------------------------------------------------
#define TFT_CS    5
#define TFT_DC    25
#define TFT_SCLK  18
#define TFT_MOSI  23

// Create the LCD object using CS + DC pins
Adafruit_GC9A01A tft(TFT_CS, TFT_DC);

// -----------------------------------------------------------
//  SECTION 2 — RGB LED STRIP SETTINGS
// -----------------------------------------------------------
#define LED_PIN   14
#define LED_COUNT 10

// Create NeoPixel object controlling 10 LEDs on pin 14
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// -----------------------------------------------------------
//  SECTION 3 — PASSIVE BUZZER
// -----------------------------------------------------------
#define BUZZER_PIN 13

// -----------------------------------------------------------
//  SECTION 4 — SENSOR ADDRESSES (I2C)
// -----------------------------------------------------------
#define AHT20_ADDR 0x38   // Temp/Humidity
#define BH1750_ADDR 0x5C  // Light intensity

// -----------------------------------------------------------
//  SECTION 5 — ALERT THRESHOLDS
//  When values exceed these, buzzer + red flashing will activate
// -----------------------------------------------------------
float TEMP_HIGH  = 30.0f;      // °C
float HUM_HIGH   = 70.0f;      // %
float LIGHT_HIGH = 1000.0f;    // lux

// -----------------------------------------------------------
//  SECTION 6 — GAUGE DISPLAY SETTINGS
//  The gauges are small circular indicators.
// -----------------------------------------------------------
#define GAUGE_RADIUS 15
#define GAUGE_THICKNESS 3
#define X_GAUGE 70        // All gauges centered at X=70
#define TEXT_X 100        // Text aligned to the right of gauge

// Vertical spacing of gauges
#define Y_TEMP  70
#define Y_HUM  120
#define Y_LUX  170

// Last drawn angles (used to ERASE old gauge before drawing new)
int lastTempAngle = 0;
int lastHumAngle  = 0;
int lastLuxAngle  = 0;

// -----------------------------------------------------------
//  SECTION 7 — ALERT LED FADING VARIABLES
// -----------------------------------------------------------
int fadeBrightness = 0;
bool fadingUp = true;      // Controls fade direction

// -----------------------------------------------------------
//  SECTION 8 — READ TEMPERATURE & HUMIDITY (AHT20)
//  Step-by-step process to teach a student how sensor reading works
// -----------------------------------------------------------
bool readAHT20(float *tempC, float *humidity) {

  // 1. Send measurement command to AHT20 sensor
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xAC);   // Start measurement
  Wire.write(0x33);   // Measurement parameters
  Wire.write(0x00);   // More parameters
  Wire.endTransmission();

  delay(80); // AHT20 needs time to measure (~80ms)

  // 2. Request 6 bytes of data from AHT20
  Wire.requestFrom(AHT20_ADDR, 6);
  if (Wire.available() != 6) return false; // Sensor error

  // 3. Read raw bytes into array
  uint8_t data[6];
  for (int i = 0; i < 6; i++) data[i] = Wire.read();

  // 4. Convert raw humidity bits into actual % humidity
  uint32_t hum_raw  = ((uint32_t)data[1] << 12) |
                      ((uint32_t)data[2] << 4)  |
                      ((uint32_t)(data[3] >> 4));

  // 5. Convert raw temperature bits into actual °C temp
  uint32_t temp_raw = (((uint32_t)data[3] & 0x0F) << 16) |
                      ((uint32_t)data[4] << 8) |
                      ((uint32_t)data[5]);

  // 6. Apply sensor formula (from datasheet)
  *humidity = ((float)hum_raw / 1048576.0f) * 100.0f;
  *tempC    = ((float)temp_raw / 1048576.0f) * 200.0f - 50.0f;

  return true;
}

// -----------------------------------------------------------
//  SECTION 9 — READ LIGHT SENSOR (BH1750)
// -----------------------------------------------------------
float readBH1750() {

  // Tell BH1750 to perform a high-resolution light measurement
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10);  // 1 lx resolution
  Wire.endTransmission();

  delay(180); // Sensor needs time to do the measurement

  // Request 2 bytes (light value)
  Wire.requestFrom(BH1750_ADDR, 2);
  if (Wire.available() == 2) {
    uint16_t raw = (Wire.read() << 8) | Wire.read();
    return raw / 1.2f;   // Convert raw value to lux
  }

  return -1.0f; // Error
}

// -----------------------------------------------------------
//  SECTION 10 — SIMPLE LED UTILITY FUNCTION
// -----------------------------------------------------------
void setAllLEDs(uint32_t color) {
  for (int i = 0; i < LED_COUNT; i++)
    strip.setPixelColor(i, color);
  strip.show();
}

// -----------------------------------------------------------
//  SECTION 11 — LCD SOFTWARE RESET
// -----------------------------------------------------------
void lcdSoftwareReset() {
  tft.writeCommand(0x01);  // Hardware reset command
  delay(150);
}

// -----------------------------------------------------------
//  SECTION 12 — DRAWING CIRCULAR PROGRESS GAUGES
//  This creates a thin ring around a point that fills up based on value.
// -----------------------------------------------------------
void drawCircleGauge(int cx, int cy, int baseRadius, int &lastAngle,
                     float value, float maxValue, uint16_t color)
{
  // Calculate the new angle: (value / max) × 360°
  int angle = (int)((value / maxValue) * 360.0f);

  // ---- Step 1: Erase previous gauge ring ----
  if (lastAngle != 0) {
    for (int a = 0; a <= lastAngle; a++) {
      float rad = a * 0.0174533f; // Convert degrees → radians
      for (int r = baseRadius - GAUGE_THICKNESS; r <= baseRadius; r++)
        tft.drawPixel(cx + r * cos(rad), cy + r * sin(rad), GC9A01A_BLACK);
    }
  }

  // ---- Step 2: Draw new gauge ring ----
  for (int a = 0; a <= angle; a++) {
    float rad = a * 0.0174533f;
    for (int r = baseRadius - GAUGE_THICKNESS; r <= baseRadius; r++)
      tft.drawPixel(cx + r * cos(rad), cy + r * sin(rad), color);
  }

  // Update last angle
  lastAngle = angle;

  // Clean inner circle so the center looks neat
  int innerRadius = baseRadius - GAUGE_THICKNESS - 2;
  tft.fillCircle(cx, cy, innerRadius, GC9A01A_BLACK);
}

// -----------------------------------------------------------
//  SECTION 13 — PRINT SENSOR TEXT (T / H / L)
// -----------------------------------------------------------
void drawGaugeText(float temp, float hum, float lux) {
  tft.setTextSize(2);

  tft.setTextColor(tft.color565(255,165,0));
  tft.setCursor(TEXT_X, Y_TEMP);
  tft.print("T:"); tft.print(temp,1); tft.println("C");

  tft.setTextColor(tft.color565(0,255,255));
  tft.setCursor(TEXT_X, Y_HUM);
  tft.print("H:"); tft.print((int)hum); tft.println("%");

  tft.setTextColor(tft.color565(0,255,0));
  tft.setCursor(TEXT_X, Y_LUX);
  tft.print("L:"); tft.print((int)lux);
}

// -----------------------------------------------------------
//  SECTION 14 — LED COLOR CHANGES BASED ON SENSOR VALUES
// -----------------------------------------------------------
void updateRGBfromSensors(float temp, float lux) {

  uint8_t r=0,g=0,b=0;

  // Temperature → Color logic
  if (temp >= TEMP_HIGH) { r=255; g=0; b=0; }   // Hot → Red
  else if (temp <= 28.99){ r=0; g=0; b=255; }   // Cool → Blue
  else                  { r=0; g=255; b=0; }    // Normal → Green

  // Light affects brightness (inverse mapping)
  int brightness = map((int)lux, 0, 1000, 255, 10);
  brightness = constrain(brightness, 10, 255);

  // Apply brightness scaling
  uint32_t finalColor = strip.Color(
    (r * brightness) / 255,
    (g * brightness) / 255,
    (b * brightness) / 255
  );

  // Update LEDs
  for (int i = 0; i < LED_COUNT; i++)
    strip.setPixelColor(i, finalColor);

  strip.show();
}

// -----------------------------------------------------------
//  SECTION 15 — SETUP FUNCTION (RUN ONCE)
// -----------------------------------------------------------
void setup() {

  Serial.begin(115200);

  Wire.begin(21,22);  // SDA = 21, SCL = 22

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  lcdSoftwareReset();
  tft.setRotation(0);
  tft.fillScreen(GC9A01A_BLACK);

  strip.begin();
  strip.show();

  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println("Dashboard Ready!");
}

// -----------------------------------------------------------
//  SECTION 16 — MAIN LOOP
//  This repeats forever. Every cycle updates the whole dashboard.
// -----------------------------------------------------------
void loop() {

  // ---- 1. Read sensors ----
  float temp=0, hum=0;
  if(!readAHT20(&temp,&hum))
    Serial.println("AHT20 error");

  float lux = readBH1750();
  if(lux < 0)
    Serial.println("BH1750 error");

  // ---- 2. Clear screen each frame ----
  tft.fillScreen(GC9A01A_BLACK);

  // ---- 3. Draw gauges (small circular meters) ----
  drawCircleGauge(X_GAUGE, Y_TEMP, GAUGE_RADIUS, lastTempAngle, temp, 50.0f, tft.color565(255,165,0));
  drawCircleGauge(X_GAUGE, Y_HUM,  GAUGE_RADIUS, lastHumAngle, hum, 100.0f, tft.color565(0,255,255));
  drawCircleGauge(X_GAUGE, Y_LUX,  GAUGE_RADIUS, lastLuxAngle, lux, LIGHT_HIGH, tft.color565(0,255,0));

  // ---- 4. Display text ----
  drawGaugeText(temp, hum, lux);

  // ---- 5. ALERT MODE (if sensor exceeds threshold) ----
  if (temp >= TEMP_HIGH || hum >= HUM_HIGH || lux >= LIGHT_HIGH) {

    // LED fade between dim ↔ bright red
    if(fadingUp){
      fadeBrightness += 15;
      if(fadeBrightness >= 255) fadingUp = false;
    } else {
      fadeBrightness -= 15;
      if(fadeBrightness <= 0) fadingUp = true;
    }

    setAllLEDs(strip.Color(fadeBrightness,0,0));

    // Buzzer alternating tone (like firetruck siren)
    static unsigned long lastTone = 0;
    static bool toggleTone = false;

    if (millis() - lastTone > 150) {
      toggleTone = !toggleTone;
      if(toggleTone) tone(BUZZER_PIN,1200);
      else           tone(BUZZER_PIN,1000);

      lastTone = millis();
    }
  }

  // ---- 6. NORMAL MODE ----
  else {
    noTone(BUZZER_PIN);               // Stop siren
    updateRGBfromSensors(temp, lux);  // Adjust LEDs normally
  }

  // Small delay to reduce flicker
  delay(100);
}
