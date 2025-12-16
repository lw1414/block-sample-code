/*********
  ESP32 BLE SERIAL CLIENT (TEACHING VERSION)

  - This code turns the ESP32 into a BLE Client
  - It scans for a specific BLE Server by name
  - Connects to the server when found
  - Subscribes to NOTIFY updates
  - Prints received data to the Serial Monitor

  UUIDs are generated using:
  👉 https://www.uuidgenerator.net/version4
*********/

#include <BLEDevice.h>   // Core BLE client functionality

// =====================================================
// BLE SERVER NAME
// =====================================================
// This MUST exactly match the BLE server name
// set in BLEDevice::init() on the server side
// =====================================================
#define bleServerName "ESP32_SERIAL_BLE"

// =====================================================
// BLE UUIDs
// =====================================================
// These UUIDs MUST match the server
// Otherwise, the client will NOT find the service
//
// Generate UUIDs here:
// 👉 https://www.uuidgenerator.net/version4
// =====================================================
static BLEUUID serviceUUID("ce459eda-898d-4477-8d14-64589f0fcc5e");
static BLEUUID charUUID("d2255f56-46a9-40ce-8524-da8ae5f1a92e");

// =====================================================
// CONNECTION FLAGS
// =====================================================
// doConnect → set to true after the server is found
// connected → true after successful connection
// =====================================================
static boolean doConnect = false;
static boolean connected = false;

// =====================================================
// BLE SERVER ADDRESS
// =====================================================
// Stores the MAC address of the BLE server once found
// =====================================================
static BLEAddress* pServerAddress;

// =====================================================
// REMOTE CHARACTERISTIC
// =====================================================
// This represents the characteristic on the SERVER
// that we will subscribe to for notifications
// =====================================================
static BLERemoteCharacteristic* remoteCharacteristic;

// =====================================================
// NOTIFICATION ENABLE VALUE
// =====================================================
// 0x2902 descriptor value to enable notifications
// =====================================================
const uint8_t notificationOn[] = {0x01, 0x00};

// =====================================================
// NOTIFY CALLBACK FUNCTION
// =====================================================
// This function is automatically called every time
// the BLE server sends new data using notify()
// =====================================================
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify
) {
  // Print the received BLE data to Serial Monitor
  Serial.print("BLE Received: ");
  Serial.write(pData, length);  // write raw bytes
  Serial.println();
}

// =====================================================
// CONNECT TO BLE SERVER FUNCTION
// =====================================================
// 1. Create BLE client
// 2. Connect to server using its MAC address
// 3. Find the service UUID
// 4. Find the characteristic UUID
// 5. Enable notifications
// =====================================================
bool connectToServer(BLEAddress pAddress) {

  // Create BLE client instance
  BLEClient* pClient = BLEDevice::createClient();

  Serial.println("Connecting to BLE server...");

  // Connect to the BLE server
  pClient->connect(pAddress);

  // Get reference to the desired BLE service
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("Failed to find service UUID");
    return false;
  }

  // Get reference to the characteristic we want
  remoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (remoteCharacteristic == nullptr) {
    Serial.println("Failed to find characteristic UUID");
    return false;
  }

  // Enable notifications if supported
  if (remoteCharacteristic->canNotify()) {

    // Register callback function
    remoteCharacteristic->registerForNotify(notifyCallback);

    // Write to CCCD (0x2902) to enable notify
    remoteCharacteristic
      ->getDescriptor(BLEUUID((uint16_t)0x2902))
      ->writeValue((uint8_t*)notificationOn, 2, true);
  }

  return true;
}

// =====================================================
// BLE SCAN CALLBACK CLASS
// =====================================================
// This class runs every time a BLE device is found
// during scanning
// =====================================================
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {

  void onResult(BLEAdvertisedDevice advertisedDevice) {

    // Check if this device matches our server name
    if (advertisedDevice.getName() == bleServerName) {

      Serial.println("BLE Server found!");

      // Stop scanning once server is found
      advertisedDevice.getScan()->stop();

      // Save the server MAC address
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());

      // Set flag to start connection
      doConnect = true;
    }
  }
};

void setup() {
  // ---------------------------------------------------
  // Start Serial Communication
  // ---------------------------------------------------
  Serial.begin(115200);
  Serial.println("ESP32 BLE Client Started");

  // ---------------------------------------------------
  // Initialize BLE (client does not need a name)
  // ---------------------------------------------------
  BLEDevice::init("");

  // ---------------------------------------------------
  // Configure BLE Scanner
  // ---------------------------------------------------
  BLEScan* pBLEScan = BLEDevice::getScan();

  // Attach scan result callback
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());

  // Active scan gives more data but uses more power
  pBLEScan->setActiveScan(true);

  // Start scanning for 30 seconds
  pBLEScan->start(30);
}

void loop() {

  // ===================================================
  // CONNECT WHEN SERVER IS FOUND
  // ===================================================
  if (doConnect) {

    if (connectToServer(*pServerAddress)) {
      Serial.println("Connected to BLE Server");
      connected = true;
    } else {
      Serial.println("Failed to connect");
    }

    // Reset flag after attempting connection
    doConnect = false;
  }

  // Client mostly waits for notifications
  delay(1000);
}
