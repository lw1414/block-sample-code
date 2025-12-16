/*************************************************
 * ESP32 BLE CLIENT + GC9A01 LCD
 * Receives: T,H,L,G
 * Displays values on round TFT
 *************************************************/

#include <Arduino.h>                // Core Arduino library
#include <BLEDevice.h>              // BLE core functions
#include <BLEScan.h>                // BLE scanning functions
#include <BLEAdvertisedDevice.h>    // BLE advertising device class

#include <SPI.h>                    // SPI communication for TFT
#include <Adafruit_GFX.h>           // Core graphics library
#include <Adafruit_GC9A01A.h>       // Driver for GC9A01A TFT

/* ==============================
   LCD PIN CONFIGURATION
   ============================== */
#define TFT_CS    5   // Chip select pin
#define TFT_DC    25  // Data/command pin
#define TFT_RST   14  // Reset pin
#define TFT_SCLK  18  // SPI Clock
#define TFT_MOSI  23  // SPI MOSI line

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST); // Create TFT object

/* ==============================
   BLE CONFIGURATION
   ============================== */
#define BLE_SERVER_NAME "ESP32_SERIAL_BLE" // Name of BLE server to connect

// UUIDs must match server's service & characteristic
static BLEUUID serviceUUID("91bad492-b950-4226-aa2b-4ede9fa42f59");
static BLEUUID charUUID("cba1d466-344c-4be3-ab3f-189f80dd7518");

/* ==============================
   BLE STATE FLAGS
   ============================== */
static boolean doConnect = false;  // Flag to start connection
static boolean connected = false;  // Flag indicating if connected

static BLEAddress* pServerAddress;              // Stores server address
static BLERemoteCharacteristic* pRemoteCharacteristic; // Remote characteristic pointer

/* ==============================
   SENSOR VALUES
   ============================== */
float tempC = 0;
float humidity = 0;
float lux = 0;
float gasPPM = 0;

/* ==============================
   NOTIFY CALLBACK
   ============================== */
// This function is called whenever the BLE server sends a notification
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify
) {
  // Convert received byte array to string
  String payload = "";
  for (size_t i = 0; i < length; i++) {
    payload += (char)pData[i];
  }

  Serial.print("BLE Received: ");
  Serial.println(payload);

  // Parse the payload string "T,H,L,G" into float variables
  sscanf(payload.c_str(), "%f,%f,%f,%f", &tempC, &humidity, &lux, &gasPPM);

  // --- Update the LCD with new sensor values ---
  tft.fillScreen(GC9A01A_BLACK);       // Clear screen

  tft.setTextColor(GC9A01A_WHITE);     // White text
  tft.setTextSize(2);                   // Font size 2

  // Display temperature
  tft.setCursor(40, 40);
  tft.print("TEMP:");
  tft.print(tempC, 1);
  tft.print(" C");

  // Display humidity
  tft.setCursor(40, 80);
  tft.print("HUM :");
  tft.print(humidity, 1);
  tft.print(" %");

  // Display light in lux
  tft.setCursor(40, 120);
  tft.print("LUX :");
  tft.print(lux, 0);

  // Display gas concentration
  tft.setCursor(40, 160);
  tft.print("GAS :");
  tft.print(gasPPM, 1);
  tft.print(" ppm");
}

/* ==============================
   CONNECT TO SERVER FUNCTION
   ============================== */
// Tries to connect to BLE server and set up notifications
bool connectToServer(BLEAddress address) {
  BLEClient* pClient = BLEDevice::createClient(); // Create a BLE client object

  Serial.println("Connecting to BLE server...");
  pClient->connect(address); // Connect to server

  // Get the remote service by UUID
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("Service UUID not found");
    return false;
  }

  // Get the characteristic by UUID
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("Characteristic UUID not found");
    return false;
  }

  // Register notification callback if characteristic supports notify
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  }

  connected = true;
  Serial.println("BLE Connected & Notifications Enabled");
  return true;
}

/* ==============================
   SCAN CALLBACK
   ============================== */
// Called when a BLE advertisement is detected during scanning
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == BLE_SERVER_NAME) { // Check server name
      Serial.println("BLE Server Found!");
      advertisedDevice.getScan()->stop();  // Stop scanning
      pServerAddress = new BLEAddress(advertisedDevice.getAddress()); // Save server address
      doConnect = true; // Flag to initiate connection in loop
    }
  }
};

/* ==============================
   SETUP FUNCTION
   ============================== */
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 BLE LCD Client");

  // --- LCD INITIALIZATION ---
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS); // Initialize SPI bus
  tft.begin();
  tft.setRotation(4);                        // Adjust rotation for round display
  tft.fillScreen(GC9A01A_BLACK);

  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.println("Scanning BLE...");            // Inform user scanning is starting

  // --- BLE INITIALIZATION ---
  BLEDevice::init("");                        // Initialize BLE

  BLEScan* pScan = BLEDevice::getScan();      // Create BLE scan object
  pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); // Set callback
  pScan->setActiveScan(true);                 // Active scan for more info
  pScan->start(30, false);                    // Scan for 30 seconds
}

/* ==============================
   LOOP FUNCTION
   ============================== */
void loop() {
  // Attempt to connect if server found
  if (doConnect) {
    if (connectToServer(*pServerAddress)) {
      Serial.println("Connected to server");
    } else {
      Serial.println("Failed to connect");
    }
    doConnect = false;
  }

  // If not connected, keep scanning every 10 seconds
  if (!connected) {
    BLEDevice::getScan()->start(10);
  }

  delay(1000); // Main loop delay
}
