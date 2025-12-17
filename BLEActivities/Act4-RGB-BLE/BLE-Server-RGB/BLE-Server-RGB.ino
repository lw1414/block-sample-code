/*************************************************
 * ESP32 BLE SERVER
 * Multi-client + Rotary & Slider Broadcast
 * Payload: COLOR,BRIGHTNESS
 *************************************************/

#include <Arduino.h>           // Core Arduino functions
#include <BLEDevice.h>         // BLE device initialization
#include <BLEServer.h>         // BLE server functions
#include <BLEUtils.h>          // Utility functions for BLE
#include <BLE2902.h>           // Descriptor for enabling notifications

/* =======================
   Analog Inputs
======================= */
#define SLIDER_PIN 33     // Pin connected to brightness slider (analog input)
#define ROTARY_PIN 32     // Pin connected to rotary potentiometer (analog input)

/* =======================
   BLE Config
======================= */
#define BLE_DEVICE_NAME    "ESP32_RGB_SERVER"  // Name that clients will see
#define SERVICE_UUID       "24b1940e-f18e-4d9e-bceb-3f4c0a6476bb" // Unique BLE service ID
#define CHARACTERISTIC_UUID "5059fdae-ac9e-41eb-977a-a6287a7de423" // Unique BLE characteristic ID

// Pointers to BLE objects
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
BLEAdvertising* pAdvertising = nullptr;

// Track if a BLE client is connected
volatile bool bleClientConnected = false;

/* =======================
   BLE Server Callbacks
======================= */
// This class handles events when a client connects or disconnects
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    bleClientConnected = true; // mark client as connected
    Serial.println("BLE Client Connected"); // print to serial monitor
  }

  void onDisconnect(BLEServer* pServer) override {
    bleClientConnected = false; // mark client as disconnected
    Serial.println("BLE Client Disconnected → Restart advertising");

    // Start advertising again so other clients can connect
    BLEDevice::startAdvertising();
  }
};

/* =======================
   Rotary → Color Name
======================= */
// Convert analog value from rotary potentiometer to a color string
String getRotaryColor() {
  int raw = analogRead(ROTARY_PIN); // read analog value (0-4095 for 12-bit ADC)

  // Map ranges of values to specific colors
  if (raw < 2800) return "RED";
  if (raw < 3000) return "GREEN";
  if (raw < 3200) return "BLUE";
  if (raw < 3400) return "YELLOW";
  if (raw < 3500) return "MAGENTA";
  if (raw < 3650) return "CYAN";
  return "WHITE"; // default if above 3650
}

/* =======================
   Slider → Brightness
======================= */
// Convert slider analog value (0-4095) to brightness (0-255)
int getSliderBrightness() {
  return map(analogRead(SLIDER_PIN), 0, 4095, 0, 255); 
  // map() scales one range to another
}

/* =======================
   Setup BLE
======================= */
void setupBLE() {
  BLEDevice::init(BLE_DEVICE_NAME); // initialize BLE with device name

  pServer = BLEDevice::createServer(); // create BLE server
  pServer->setCallbacks(new MyServerCallbacks()); // assign callbacks

  // Create a BLE service
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Create a characteristic with read and notify properties
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );

  // Add a descriptor so client can enable notifications
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Setup advertising so clients can see the server
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x00);

  BLEDevice::startAdvertising(); // start advertising
  Serial.println("BLE Server started and advertising...");
}

/* =======================
   Setup
======================= */
void setup() {
  Serial.begin(115200); // Start serial monitor for debugging

  analogReadResolution(12); // 12-bit ADC resolution (0-4095)
  pinMode(SLIDER_PIN, INPUT); // set slider pin as input
  pinMode(ROTARY_PIN, INPUT); // set rotary pin as input

  setupBLE(); // call function to setup BLE
}

/* =======================
   Loop
======================= */
// Variables to store last transmitted values
String lastColor = "";
int lastBrightness = -1;

void loop() {
  // Read current values
  String color = getRotaryColor();
  int brightness = getSliderBrightness();

  // Only send BLE notification if there is a change
  if (color != lastColor || brightness != lastBrightness) {
    String payload = color + "," + String(brightness); // prepare payload
    pCharacteristic->setValue(payload.c_str());       // set characteristic value
    pCharacteristic->notify();                        // send notification to client

    Serial.print("BLE TX → "); // print to serial monitor
    Serial.println(payload);

    // Update last values
    lastColor = color;
    lastBrightness = brightness;
  }

  delay(50); // small delay to reduce BLE traffic and CPU usage
}
