#include <Arduino.h>                 // Core Arduino functions
#include <BLEDevice.h>               // BLE device creation
#include <BLEScan.h>                 // BLE scanning functions
#include <BLEAdvertisedDevice.h>     // BLE device object

#include <SPI.h>                     // SPI communication for TFT
#include <Adafruit_GFX.h>            // Core graphics library for displays
#include <Adafruit_GC9A01A.h>       // Driver for GC9A01 TFT display
#include <Adafruit_NeoPixel.h>       // Driver for WS2812/NeoPixel LEDs

/* ----------------------
   TFT CONFIGURATION
---------------------- */
#define TFT_CS    5   // TFT chip select pin
#define TFT_DC    25  // TFT data/command pin
#define TFT_SCLK  18  // SPI clock pin
#define TFT_MOSI  23  // SPI MOSI pin
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, -1); // TFT object; -1 = use software reset

/* ----------------------
   NEOPIXEL LED CONFIG
---------------------- */
#define LED_PIN     14  // Data pin for NeoPixels
#define LED_COUNT   10  // Number of LEDs in the strip
#define TOUCH_PIN   34  // Touch input pin to cycle LED patterns
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

/* ----------------------
   BLE CONFIGURATION
---------------------- */
#define BLE_SERVER_NAME "ESP32_SERIAL_BLE"  // Name of the BLE server to connect to
static BLEUUID serviceUUID("91bad492-b950-4226-aa2b-4ede9fa42f59");  // Service UUID
static BLEUUID charUUID("cba1d466-344c-4be3-ab3f-189f80dd7518");     // Characteristic UUID

/* ----------------------
   BLE STATE VARIABLES
---------------------- */
static boolean doConnect = false;     // Flag to attempt connection
static boolean connected = false;     // Connection state
static BLEAddress* pServerAddress;    // Address of BLE server
static BLERemoteCharacteristic* pRemoteCharacteristic; // Characteristic for notifications

/* ----------------------
   SENSOR DATA VARIABLES
---------------------- */
float tempC = 0, humidity = 0, lux = 0, gasPPM = 0; // Sensor readings
bool alarmState = false;  // Whether alarm is active

/* ----------------------
   TOUCH COLORS + SEQUENCES
---------------------- */
enum SequenceType { BLINK, CHASE, ALTERNATE, PULSE }; // Types of LED patterns

// Each pattern stores a color and a sequence type
struct AlarmPattern {
  uint32_t color;
  SequenceType seq;
};

// Define multiple LED patterns
AlarmPattern patterns[] = {
  { strip.Color(255, 0, 0), BLINK },
  { strip.Color(0, 255, 0), CHASE },
  { strip.Color(0, 0, 255), ALTERNATE },
  { strip.Color(255, 255, 0), PULSE },
  { strip.Color(255, 0, 255), CHASE }
};
const int patternCount = sizeof(patterns)/sizeof(patterns[0]); // Count of patterns
int currentPattern = 0;   // Index of current pattern
bool touchPressed = false; // Track touch button state to avoid multiple triggers

/* ----------------------
   HELPER FUNCTIONS FOR LED STRIP
---------------------- */

// Set all LEDs to a specific color
void setStripColor(uint32_t color) {
  for (int i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, color);
  strip.show();
}

// BLINK pattern
void blinkSequence(uint32_t color) {
  setStripColor(color);
  delay(200);
  setStripColor(0);
  delay(200);
}

// CHASE pattern: lights move along the strip
void chaseSequence(uint32_t color) {
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
    if (i > 0) strip.setPixelColor(i - 1, 0);
    strip.show();
    delay(100);
  }
  strip.clear();
  strip.show();
}

// ALTERNATE pattern: even and odd LEDs toggle
void alternateSequence(uint32_t color) {
  for (int j = 0; j < 4; j++) { // repeat 4 times
    for (int i = 0; i < LED_COUNT; i++) {
      if (i % 2 == 0) strip.setPixelColor(i, color);
      else strip.setPixelColor(i, 0);
    }
    strip.show();
    delay(200);
    for (int i = 0; i < LED_COUNT; i++) {
      if (i % 2 != 0) strip.setPixelColor(i, color);
      else strip.setPixelColor(i, 0);
    }
    strip.show();
    delay(200);
  }
  strip.clear();
  strip.show();
}

// PULSE pattern: gradually increase and decrease brightness
void pulseSequence(uint32_t color) {
  for (int b = 0; b <= 255; b += 5) {
    uint32_t c = strip.Color(((color >> 16 & 0xFF) * b) / 255,
                             ((color >> 8 & 0xFF) * b) / 255,
                             ((color & 0xFF) * b) / 255);
    setStripColor(c);
    delay(20);
  }
  for (int b = 255; b >= 0; b -= 5) {
    uint32_t c = strip.Color(((color >> 16 & 0xFF) * b) / 255,
                             ((color >> 8 & 0xFF) * b) / 255,
                             ((color & 0xFF) * b) / 255);
    setStripColor(c);
    delay(20);
  }
}

/* ----------------------
   RUN SELECTED ALARM SEQUENCE
---------------------- */
void runAlarm() {
  AlarmPattern p = patterns[currentPattern];
  switch (p.seq) {
    case BLINK: setStripColor(0); blinkSequence(p.color); break;
    case CHASE: chaseSequence(p.color); break;
    case ALTERNATE: alternateSequence(p.color); break;
    case PULSE: pulseSequence(p.color); break;
  }
}

/* ----------------------
   BLE NOTIFY CALLBACK
   Called whenever the BLE server sends a notification
---------------------- */
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify) {
  if (length < 9) return; // Ensure enough data received

  // Parse sensor values from bytes
  tempC    = ((uint16_t)pData[0] << 8 | pData[1]) / 100.0;
  humidity = ((uint16_t)pData[2] << 8 | pData[3]) / 100.0;
  lux      = ((uint16_t)pData[4] << 8 | pData[5]);
  gasPPM   = ((uint16_t)pData[6] << 8 | pData[7]);
  alarmState = pData[8];

  // Update TFT display with sensor readings
  tft.fillScreen(GC9A01A_BLACK);
  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(2);
  tft.setCursor(40, 80); tft.print("TEMP:"); tft.print(tempC, 1); tft.print(" C");
  tft.setCursor(40, 100); tft.print("HUM :"); tft.print(humidity, 1); tft.print(" %");
  tft.setCursor(40, 120); tft.print("LUX :"); tft.print(lux, 0);
  tft.setCursor(40, 140); tft.print("GAS :"); tft.print(gasPPM, 1); tft.print(" ppm");
  tft.setCursor(40, 160); tft.print("ALARM:"); tft.print(alarmState ? "ON" : "OFF");
}

/* ----------------------
   BLE CONNECTION FUNCTION
---------------------- */
bool connectToServer(BLEAddress address) {
  BLEClient* pClient = BLEDevice::createClient();
  Serial.println("Connecting...");
  pClient->connect(address);  // Connect to server

  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (!pRemoteService) return false;  // Service not found

  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (!pRemoteCharacteristic) return false; // Characteristic not found

  // Subscribe to notifications if supported
  if (pRemoteCharacteristic->canNotify())
    pRemoteCharacteristic->registerForNotify(notifyCallback);

  connected = true;
  Serial.println("Connected & Notifications Enabled");
  return true;
}

/* ----------------------
   BLE SCAN CALLBACK
   Called when a BLE device is found during scanning
---------------------- */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == BLE_SERVER_NAME) {
      Serial.println("BLE Server Found!");
      advertisedDevice.getScan()->stop(); // Stop scanning
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true; // Flag to connect in main loop
    }
  }
};

/* ----------------------
   SETUP FUNCTION
   Runs once when ESP32 starts
---------------------- */
void setup() {
  Serial.begin(115200);

  // Initialize SPI for TFT
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.setRotation(4);
  tft.fillScreen(GC9A01A_BLACK);
  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 100); tft.println("Scanning BLE...");

  // Initialize LED strip
  strip.begin(); strip.show();
  pinMode(TOUCH_PIN, INPUT);

  // Initialize BLE
  BLEDevice::init("");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pScan->setActiveScan(true);
  pScan->start(30, false); // Scan for 30 seconds
}

/* ----------------------
   LOOP FUNCTION
   Runs repeatedly
---------------------- */
void loop() {
  // Handle touch input to cycle LED patterns
  if (digitalRead(TOUCH_PIN) == HIGH && !touchPressed) {
    touchPressed = true;
    currentPattern++;
    if (currentPattern >= patternCount) currentPattern = 0;
    Serial.print("Selected Pattern: "); Serial.println(currentPattern);
    delay(300); // Debounce touch
  } else if (digitalRead(TOUCH_PIN) == LOW) touchPressed = false;

  // Attempt BLE connection if flagged
  if (doConnect) {
    connectToServer(*pServerAddress);
    doConnect = false;
  }

  // Run alarm sequences if alarm is active
  if (alarmState) runAlarm();
  else { // Show normal status colors
    if (!connected) setStripColor(strip.Color(255, 0, 0));   // Red = disconnected
    else setStripColor(strip.Color(0, 255, 0));              // Green = connected
  }

  // Keep scanning if disconnected
  if (!connected) BLEDevice::getScan()->start(10);
}
