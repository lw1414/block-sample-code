/*************************************************
 * ESP32 BLE CLIENT + NeoPixel + Touch Sequences
 * Receives: COLOR,BRIGHTNESS
 * Drives WS2812 LED strip
 * Touch pin (active HIGH) cycles sequences
 * Automatic BLE reconnection if server disconnects
 *************************************************/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>

/* ==============================
   LED STRIP CONFIG
============================== */
#define LED_PIN   14
#define LED_COUNT 10
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

/* ==============================
   TOUCH PIN CONFIG
============================== */
#define TOUCH_PIN   34                // AT42QT1010 output (active HIGH)
int sequenceMode = 0;                // Tracks which sequence is active
int numSequences = 4;                // Total number of sequences
unsigned long lastTouchTime = 0;     // Last touch time for debounce
unsigned long touchDebounce = 300;   // Minimum ms between taps

/* ==============================
   BLE CONFIGURATION
============================== */
#define BLE_SERVER_NAME "ESP32_RGB_SERVER"
#define SERVICE_UUID       "24b1940e-f18e-4d9e-bceb-3f4c0a6476bb"
#define CHARACTERISTIC_UUID "5059fdae-ac9e-41eb-977a-a6287a7de423"

static boolean doConnect = false;
static boolean connected = false;

static BLEAddress* pServerAddress;                 
static BLERemoteCharacteristic* pRemoteCharacteristic; 

BLEClient* pClient = nullptr;  // Global BLE client object

// Store current color/brightness from BLE
uint32_t currentColor = strip.Color(0, 0, 0);
int currentBrightness = 0;

/* ==============================
   BLE NOTIFICATION CALLBACK
============================== */
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify
) {
  String payload = "";
  for (size_t i = 0; i < length; i++) payload += (char)pData[i];

  Serial.print("BLE Received: ");
  Serial.println(payload);

  char colorStr[16];
  sscanf(payload.c_str(), "%15[^,],%d", colorStr, &currentBrightness);

  if (strcmp(colorStr, "RED") == 0) currentColor = strip.Color(currentBrightness, 0, 0);
  else if (strcmp(colorStr, "GREEN") == 0) currentColor = strip.Color(0, currentBrightness, 0);
  else if (strcmp(colorStr, "BLUE") == 0) currentColor = strip.Color(0, 0, currentBrightness);
  else if (strcmp(colorStr, "YELLOW") == 0) currentColor = strip.Color(currentBrightness, currentBrightness, 0);
  else if (strcmp(colorStr, "MAGENTA") == 0) currentColor = strip.Color(currentBrightness, 0, currentBrightness);
  else if (strcmp(colorStr, "CYAN") == 0) currentColor = strip.Color(0, currentBrightness, currentBrightness);
  else if (strcmp(colorStr, "WHITE") == 0) currentColor = strip.Color(currentBrightness, currentBrightness, currentBrightness);
}

/* ==============================
   BLE CLIENT CALLBACKS
   Detect disconnection and trigger reconnection
============================== */
class MyClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pC) {
    Serial.println("BLE Client Connected");
  }

  void onDisconnect(BLEClient* pC) {
    Serial.println("BLE Client Disconnected! Will try to reconnect...");
    connected = false;
    doConnect = true;  // Flag to reconnect in loop
  }
};

/* ==============================
   CONNECT TO SERVER
============================== */
bool connectToServer(BLEAddress address) {
  if (!pClient) {
    pClient = BLEDevice::createClient();       // Create client if null
    pClient->setClientCallbacks(new MyClientCallbacks());
  }

  Serial.println("Connecting to BLE server...");
  if (!pClient->connect(address)) {
    Serial.println("Connection failed!");
    connected = false;
    return false;
  }

  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("Service not found.");
    connected = false;
    return false;
  }

  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("Characteristic not found.");
    connected = false;
    return false;
  }

  if (pRemoteCharacteristic->canNotify())
    pRemoteCharacteristic->registerForNotify(notifyCallback);

  connected = true;
  Serial.println("Connected & Notifications enabled.");
  return true;
}

/* ==============================
   SCAN CALLBACK
============================== */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == BLE_SERVER_NAME) {
      Serial.println("BLE Server Found!");
      advertisedDevice.getScan()->stop();
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
    }
  }
};

/* ==============================
   NON-BLOCKING LED SEQUENCES
============================== */
unsigned long lastUpdate = 0; 
int chaseIndex = 0;

void ledSequences() {
  unsigned long currentMillis = millis();

  switch (sequenceMode) {
    case 0: // Solid
      for (int i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, currentColor);
      strip.show();
      break;

    case 1: // Blink every 500ms
      if (currentMillis - lastUpdate >= 500) {
        lastUpdate = currentMillis;
        static bool on = true;
        for (int i = 0; i < LED_COUNT; i++)
          strip.setPixelColor(i, on ? currentColor : 0);
        strip.show();
        on = !on;
      }
      break;

    case 2: // Chase
      if (currentMillis - lastUpdate >= 150) {
        lastUpdate = currentMillis;
        strip.clear();
        strip.setPixelColor(chaseIndex, currentColor);
        strip.show();
        chaseIndex = (chaseIndex + 1) % LED_COUNT;
      }
      break;

    case 3: // Rainbow
      if (currentMillis - lastUpdate >= 50) {
        lastUpdate = currentMillis;
        for (int i = 0; i < LED_COUNT; i++) {
          strip.setPixelColor(i, strip.Color(
            (i*30 + currentMillis/10) % 256,
            (i*60 + currentMillis/10) % 256,
            (i*90 + currentMillis/10) % 256
          ));
        }
        strip.show();
      }
      break;
  }
}

/* ==============================
   SETUP
============================== */
void setup() {
  Serial.begin(115200);

  // NeoPixel
  strip.begin();
  strip.show(); // all off

  // BLE
  BLEDevice::init("");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pScan->setActiveScan(true);
  pScan->start(30, false);

  // Touch pin
  pinMode(TOUCH_PIN, INPUT); // Digital input
}

/* ==============================
   LOOP
============================== */
void loop() {
  // --- BLE reconnection ---
  if (doConnect && pServerAddress != nullptr) {
    if (connectToServer(*pServerAddress)) {
      Serial.println("Successfully connected.");
    } else {
      Serial.println("Failed to connect.");
    }
    doConnect = false;
  }

  if (!connected) {
    BLEDevice::getScan()->start(10); // Keep scanning if disconnected
  }

  // --- Touch detection (active HIGH) ---
  if (digitalRead(TOUCH_PIN) == HIGH) {
    if (millis() - lastTouchTime > touchDebounce) {
      sequenceMode = (sequenceMode + 1) % numSequences;
      Serial.print("Sequence changed to: ");
      Serial.println(sequenceMode);
      lastTouchTime = millis();
    }
  }

  // --- Run only current LED sequence ---
  ledSequences();
}
