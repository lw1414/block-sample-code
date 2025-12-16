/*********
  ESP32 BLE SERIAL SERVER (TEACHING VERSION)

  - This code turns the ESP32 into a BLE Server
  - Data is typed from the Serial Monitor
  - Data is sent to a BLE Client using NOTIFY
  - millis() is used instead of delay()

  UUIDs are generated using:
  👉 https://www.uuidgenerator.net/version4
*********/

#include <BLEDevice.h>     // Core BLE functionality
#include <BLEServer.h>     // BLE Server support
#include <BLEUtils.h>      // Helper utilities
#include <BLE2902.h>       // Descriptor required for notifications

// =====================================================
// BLE SERVER NAME
// This is the name that BLE clients will see when scanning
// =====================================================
#define bleServerName "ESP32_SERIAL_BLE"

// =====================================================
// TIMER VARIABLES (millis-based timing)
// =====================================================
// lastTime    → stores the last time data was sent
// timerDelay → how often data is sent (in milliseconds)
unsigned long lastTime = 0;
unsigned long timerDelay = 2000; // send every 2 seconds

// This flag tells us whether a BLE client is connected
bool deviceConnected = false;

// =====================================================
// BLE UUIDs
// =====================================================
// UUID = Universally Unique Identifier
// Used to uniquely identify BLE services and characteristics
//
// IMPORTANT RULES:
// 1. UUIDs MUST match between server and client
// 2. Generate them once and KEEP them fixed
// 3. Use 128-bit UUIDs to avoid conflicts
//
// Generate UUIDs here:
// 👉 https://www.uuidgenerator.net/version4
// =====================================================
#define SERVICE_UUID     "ce459eda-898d-4477-8d14-64589f0fcc5e"
#define CHAR_SERIAL_UUID "d2255f56-46a9-40ce-8524-da8ae5f1a92e"

// =====================================================
// BLE CHARACTERISTIC
// =====================================================
// A characteristic is where the actual data is sent
//
// PROPERTY_NOTIFY means:
// - The server pushes data automatically
// - The client does NOT need to request it
// =====================================================
BLECharacteristic serialCharacteristic(
  CHAR_SERIAL_UUID,
  BLECharacteristic::PROPERTY_NOTIFY
);

// =====================================================
// BLE SERVER CALLBACK CLASS
// =====================================================
// This class lets us detect when a client connects
// or disconnects from the BLE server
// =====================================================
class MyServerCallbacks : public BLEServerCallbacks {

  // Called automatically when a client connects
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("BLE Client Connected");
  };

  // Called automatically when a client disconnects
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("BLE Client Disconnected");
  }
};

// =====================================================
// GLOBAL SERIAL BUFFER
// =====================================================
// Stores the last line typed in the Serial Monitor
// =====================================================
String serialData = "";

void setup() {
  // ---------------------------------------------------
  // Start Serial Communication
  // ---------------------------------------------------
  Serial.begin(115200);
  Serial.println("ESP32 BLE Serial Server");

  // ---------------------------------------------------
  // Initialize BLE and set device name
  // ---------------------------------------------------
  BLEDevice::init(bleServerName);

  // ---------------------------------------------------
  // Create BLE Server
  // ---------------------------------------------------
  BLEServer *pServer = BLEDevice::createServer();

  // Attach connection callbacks
  pServer->setCallbacks(new MyServerCallbacks());

  // ---------------------------------------------------
  // Create BLE Service
  // ---------------------------------------------------
  BLEService *service = pServer->createService(SERVICE_UUID);

  // ---------------------------------------------------
  // Add Characteristic to the Service
  // ---------------------------------------------------
  service->addCharacteristic(&serialCharacteristic);

  // BLE2902 descriptor is REQUIRED for notifications
  serialCharacteristic.addDescriptor(new BLE2902());

  // ---------------------------------------------------
  // Start the Service
  // ---------------------------------------------------
  service->start();

  // ---------------------------------------------------
  // Start BLE Advertising
  // ---------------------------------------------------
  // Advertising makes the ESP32 visible to BLE clients
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  Serial.println("Waiting for BLE client...");
}

void loop() {

  // ===================================================
  // READ DATA FROM SERIAL MONITOR
  // ===================================================
  // If the user types something and presses ENTER,
  // it will be stored in serialData
  // ===================================================
  if (Serial.available()) {
    serialData = Serial.readStringUntil('\n'); // read one line
    serialData.trim();                         // remove \r and \n

    Serial.print("Typed: ");
    Serial.println(serialData);
  }

  // ===================================================
  // SEND DATA OVER BLE USING millis()
  // ===================================================
  // Conditions:
  // 1. A BLE client must be connected
  // 2. timerDelay must have passed
  // 3. serialData must not be empty
  // ===================================================
  if (deviceConnected && (millis() - lastTime > timerDelay)) {

    if (serialData.length() > 0) {

      // Set the characteristic value
      serialCharacteristic.setValue(serialData.c_str());

      // Notify the connected client
      serialCharacteristic.notify();

      Serial.print("BLE Sent: ");
      Serial.println(serialData);
    }

    // Update timer reference
    lastTime = millis();
  }
}
