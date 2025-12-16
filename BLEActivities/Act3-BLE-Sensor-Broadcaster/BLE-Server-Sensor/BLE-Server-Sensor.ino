/*************************************************
 * ESP32 BLE SERVER
 * Multi-client + Sensor Broadcast
 * Payload: T,H,L,G
 *************************************************/

#include <Arduino.h>           // Core Arduino library
#include <Wire.h>              // I2C communication library
#include <BLEDevice.h>         // BLE core functions
#include <BLEServer.h>         // BLE server functions
#include <BLEUtils.h>          // Utility functions for BLE
#include <BLE2902.h>           // Descriptor for notifications

/* ==============================
   BLE CONFIGURATION
   ============================== */
#define BLE_DEVICE_NAME "ESP32_SERIAL_BLE"  // Name of the BLE device when scanned

// UUIDs must match on client side to connect correctly
#define SERVICE_UUID        "91bad492-b950-4226-aa2b-4ede9fa42f59"  // Unique Service ID
#define CHARACTERISTIC_UUID "cba1d466-344c-4be3-ab3f-189f80dd7518"  // Unique Characteristic ID

/* ==============================
   SENSOR CONFIGURATION
   ============================== */
#define AHT20_ADDR   0x38   // I2C address of temperature & humidity sensor
#define BH1750_ADDR  0x5C   // I2C address of light sensor
#define MQ136_PIN    32     // Analog pin connected to air quality sensor

/* ==============================
   MQ136 CALIBRATION (ADJUST!)
   ============================== */
#define ADC_MAX   4095.0     // 12-bit ADC max value for ESP32
#define VREF      3.3        // Reference voltage
#define RL_KOHM   10.0       // Load resistor value in kilo-ohms
#define R0_KOHM   20.0       // Sensor baseline resistance (clean air)  

/* ==============================
   GLOBAL VARIABLES
   ============================== */
BLEServer* pServer = nullptr;           // Pointer to BLE server object
BLECharacteristic* pCharacteristic = nullptr; // Pointer to BLE characteristic

uint32_t connectedClients = 0;          // Count of connected BLE clients
uint32_t lastSendTime = 0;              // Timestamp of last notification
const uint32_t sendInterval = 1000;     // Send interval in milliseconds (1 sec)

/* ==============================
   BLE SERVER CALLBACKS
   ============================== */
class MyServerCallbacks : public BLEServerCallbacks {
  // Triggered when a client connects
  void onConnect(BLEServer* pServer) override {
    connectedClients++;  // Increase count of connected clients
    Serial.print("Client connected. Total clients: ");
    Serial.println(connectedClients);

    // Keep advertising so other clients can connect too
    BLEDevice::startAdvertising();
  }

  // Triggered when a client disconnects
  void onDisconnect(BLEServer* pServer) override {
    if (connectedClients > 0) connectedClients--;  // Decrease count

    Serial.print("Client disconnected. Total clients: ");
    Serial.println(connectedClients);

    delay(200);                   // Small delay to stabilize
    BLEDevice::startAdvertising(); // Restart advertising
  }
};

/* ==============================
   SENSOR FUNCTIONS
   ============================== */

// Function to read AHT20 temperature & humidity sensor
bool readAHT20(float* tempC, float* humidity) {
  Wire.beginTransmission(AHT20_ADDR);  // Start I2C communication
  Wire.write(0xAC);                     // Command to trigger measurement
  Wire.write(0x33);
  Wire.write(0x00);
  Wire.endTransmission();

  delay(80); // Wait for sensor to process data

  Wire.requestFrom(AHT20_ADDR, 6);      // Request 6 bytes from sensor
  if (Wire.available() != 6) return false;

  uint8_t data[6];
  for (int i = 0; i < 6; i++) data[i] = Wire.read();

  // Extract humidity from raw data
  uint32_t hum_raw =
    ((uint32_t)data[1] << 12) |
    ((uint32_t)data[2] << 4) |
    ((uint32_t)(data[3] >> 4));

  // Extract temperature from raw data
  uint32_t temp_raw =
    (((uint32_t)data[3] & 0x0F) << 16) |
    ((uint32_t)data[4] << 8) |
    ((uint32_t)data[5]);

  // Convert to human-readable values
  *humidity = ((float)hum_raw / 1048576.0) * 100.0;
  *tempC   = ((float)temp_raw / 1048576.0) * 200.0 - 50.0;

  return true; // Return true if reading successful
}

// Function to read BH1750 light sensor
float readBH1750() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10); // Continuous High-Res mode
  Wire.endTransmission();
  delay(180);       // Wait for measurement

  Wire.requestFrom(BH1750_ADDR, 2);
  if (Wire.available() == 2) {
    uint16_t raw = Wire.read() << 8 | Wire.read();
    return raw / 1.2; // Convert to lux
  }
  return -1; // Error value if reading fails
}

// Function to read MQ136 air quality sensor in ppm
float mq136ReadPPM(int adcValue) {
  float voltage = (adcValue / ADC_MAX) * VREF; // Convert ADC value to voltage
  if (voltage <= 0.01) return 0;               // Avoid division by zero

  float rs = ((VREF - voltage) / voltage) * RL_KOHM; // Sensor resistance
  float ratio = rs / R0_KOHM;                        // Ratio to clean air

  // Approximate curve formula to get ppm
  float ppm = 100.0 * pow(ratio, -1.5);
  return ppm;
}

/* ==============================
   SETUP FUNCTION
   ============================== */
void setup() {
  Serial.begin(115200);     // Initialize serial monitor
  Wire.begin(21, 22);       // Initialize I2C (SDA=21, SCL=22)

  analogReadResolution(12); // Set ADC resolution to 12-bit
  pinMode(MQ136_PIN, INPUT); // Set analog pin as input

  Serial.println("Initializing BLE Server...");

  BLEDevice::init(BLE_DEVICE_NAME); // Initialize BLE device

  pServer = BLEDevice::createServer();          // Create BLE server
  pServer->setCallbacks(new MyServerCallbacks()); // Set server callbacks

  BLEService* pService = pServer->createService(SERVICE_UUID); // Create service

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );

  pCharacteristic->addDescriptor(new BLE2902()); // Needed for notifications

  pService->start(); // Start the service

  // Configure advertising so clients can discover this server
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x00);

  BLEDevice::startAdvertising(); // Start BLE advertising

  Serial.println("BLE Server started, waiting for clients...");
}

/* ==============================
   LOOP FUNCTION
   ============================== */
void loop() {
  // Only send data if at least one client is connected and interval elapsed
  if (connectedClients > 0 && millis() - lastSendTime >= sendInterval) {

    float t = 0, h = 0, lux = 0;
    bool ahtOk = readAHT20(&t, &h);  // Read temperature & humidity
    lux = readBH1750();               // Read light in lux

    int mq_adc = analogRead(MQ136_PIN); // Read air quality sensor
    float mq_ppm = mq136ReadPPM(mq_adc); // Convert to ppm

    // Prepare payload in format T,H,L,G
    String payload = "";

    if (ahtOk) {
      payload += String(t, 2); // Temperature with 2 decimals
      payload += ",";
      payload += String(h, 2); // Humidity with 2 decimals
    } else {
      payload += "0,0";         // Fallback if sensor fails
    }

    payload += ",";
    payload += String(lux, 1);   // Light value
    payload += ",";
    payload += String(mq_ppm, 2); // Gas ppm value

    // Send the payload to all connected clients
    pCharacteristic->setValue(payload.c_str());
    pCharacteristic->notify();

    Serial.print("BLE Notify -> ");
    Serial.println(payload);

    lastSendTime = millis(); // Update last send timestamp
  }
}
