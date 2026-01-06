#include <Wire.h>                 // I2C communication library (used for AHT20, BH1750)
#include <SPI.h>                  // SPI communication library (used for TFT)
#include <Adafruit_GFX.h>         // Core graphics library for TFT displays
#include <Adafruit_GC9A01A.h>    // Specific driver for GC9A01A TFT
#include <HardwareSerial.h>       // Allows use of additional serial ports on ESP32

// =====================================================
// PINS
// =====================================================
#define TFT_CS    5               // TFT Chip Select pin
#define TFT_DC    25              // TFT Data/Command pin
#define TFT_RST   -1              // TFT Reset pin (not used)
#define TFT_SCLK  18              // TFT SPI Clock
#define TFT_MOSI  23              // TFT SPI MOSI

#define GSM_RX 17                 // RX pin for SIM800C
#define GSM_TX 16                 // TX pin for SIM800C
#define PWRKEY 4                  // Pin to power on/off GSM module

#define MQ136_PIN 32              // Analog input pin for MQ136 gas sensor

// =====================================================
// CONSTANTS
// =====================================================
#define SMS_QUEUE_SIZE   10       // Max number of SMS messages in queue
#define SMS_COOLDOWN_MS  4000     // Cooldown period between SMS sending

// =====================================================
// OBJECTS
// =====================================================
HardwareSerial gsm(1);            // Use Serial1 for GSM communication
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);  // TFT display object

// =====================================================
// REGISTERED NUMBERS//text STATUS to used number//
// =====================================================
String phoneNumbers[5] = {       // Array to store phone numbers allowed to receive alerts
  "+639928900314",
  "+639916425321",
  "",
  "",
  ""
};

// =====================================================
// GSM VARIABLES
// =====================================================
bool gsmReady = false;           // Flag to check if GSM is ready
String gsmBuffer = "";           // Buffer to store incoming GSM data

// =====================================================
// GSM STATE MACHINE
// =====================================================
enum GSMState {
  GSM_POWER_OFF,                 // GSM module is off
  GSM_POWER_ON,                  // Powering on GSM
  GSM_INIT,                      // Sending initialization commands
  GSM_WAIT_NETWORK,              // Waiting for network registration
  GSM_READY                      // GSM ready to send/receive SMS
};
GSMState gsmState = GSM_POWER_OFF;

unsigned long gsmMillis = 0;      // Timer for GSM command intervals
uint8_t gsmInitIndex = 0;         // Index for GSM initialization commands

// Array of GSM initialization commands
const char* gsmInitCmds[] = {
  "AT",             // Basic attention command
  "AT+CFUN=1",      // Set full functionality
  "AT+CSCLK=0",     // Disable sleep mode
  "AT+CMGF=1",      // Set SMS mode to text
  "AT+CNMI=1,2,0,0,0"  // Configure SMS notifications
};
const uint8_t GSM_INIT_COUNT =
  sizeof(gsmInitCmds) / sizeof(gsmInitCmds[0]);  // Count number of init commands

// =====================================================
// SENSOR DATA VARIABLES
// =====================================================
float tempValue = 0, humValue = 0, luxValue = 0; // Store temperature, humidity, light
float gasPPM = 0;                               // Store gas concentration in ppm

// Thresholds for alerts
float TEMP_THRESHOLD  = 40;
float HUM_THRESHOLD   = 70;
float LIGHT_THRESHOLD = 800;
float GAS_THRESHOLD   = 1;   // ppm

bool alertSent = false;      // Flag to prevent multiple alerts

// =====================================================
// MQ136 CALIBRATION CONSTANTS
// =====================================================
#define ADC_MAX     4095.0     // Max ADC value for ESP32 12-bit
#define VREF        3.3        // Reference voltage
#define RL_VALUE    10.0       // Load resistor value (kΩ)
#define RO_CLEAN    10.0       // Baseline resistance in clean air (kΩ)

// =====================================================
// SMS QUEUE STRUCTURE
// =====================================================
struct SMSItem {               // Structure to store SMS info
  String number;               // Recipient number
  String message;              // Message body
};

SMSItem smsQueue[SMS_QUEUE_SIZE];  // Circular queue for outgoing SMS
uint8_t smsHead = 0;               // Queue head
uint8_t smsTail = 0;               // Queue tail

// =====================================================
// SMS SEND STATE MACHINE
// =====================================================
enum SMSSendState {
  SMS_IDLE,           // No SMS to send
  SMS_SEND_BODY,      // Sending SMS content
  SMS_WAIT_RESULT,    // Waiting for GSM confirmation
  SMS_COOLDOWN        // Cooldown before next SMS
};

SMSSendState smsState = SMS_IDLE;
SMSItem currentSMS;              // Currently sending SMS
unsigned long smsMillis = 0;     // Timer for SMS state transitions

// =====================================================
// TFT HELPER FUNCTIONS
// =====================================================
unsigned long lcdMillis = 0;     // Timer for LCD message duration
bool lcdActive = false;          // True if showing SMS notification
unsigned long lcdSensorMillis = 0; // Timer for live sensor updates

// Print text centered horizontally on TFT
void printCentered(const char* txt, int y) {
  tft.setTextSize(2);
  tft.setTextColor(GC9A01A_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h); // Measure text width
  tft.setCursor((240 - w) / 2, y);               // Center horizontally
  tft.println(txt);
}

// Show a message on TFT for SMS notifications
void showLCDMessage(const char* title, String line1, String line2) {
  tft.fillScreen(GC9A01A_BLACK);  // Clear screen
  tft.setTextSize(2);
  tft.setTextColor(GC9A01A_WHITE);

  printCentered(title, 40);        // Show title

  tft.setCursor(20, 90);           // Line 1
  tft.println(line1);

  tft.setCursor(20, 130);          // Line 2
  tft.println(line2);

  lcdMillis = millis();             // Reset timer
  lcdActive = true;                 // Set flag
}

// =====================================================
// DISPLAY LIVE SENSOR READINGS
// =====================================================
void displayLiveSensors() {
  tft.fillScreen(GC9A01A_BLACK);   // Clear TFT
  tft.setTextSize(2);
  tft.setTextColor(GC9A01A_WHITE);

  printCentered("SENSOR DATA", 40);

  tft.setCursor(50, 70);
  tft.println("T: " + String(tempValue,1) + " C");

  tft.setCursor(50, 100);
  tft.println("H: " + String(humValue,1) + " %");

  tft.setCursor(50, 130);
  tft.println("L: " + String(luxValue,0) + " lux");

  tft.setCursor(50, 160);
  tft.println("G: " + String(gasPPM,2) + " ppm");
}

// =====================================================
// QUEUE FUNCTIONS
// =====================================================
bool enqueueSMS(String num, String msg) {
  // Calculate next tail index in circular buffer
  uint8_t next = (smsTail + 1) % SMS_QUEUE_SIZE;
  if (next == smsHead) return false; // Queue full
  smsQueue[smsTail] = { num, msg }; // Add SMS
  smsTail = next;
  return true;
}

bool dequeueSMS(SMSItem &item) {
  if (smsHead == smsTail) return false; // Queue empty
  item = smsQueue[smsHead];
  smsHead = (smsHead + 1) % SMS_QUEUE_SIZE;
  return true;
}

// Send SMS to all registered numbers
void sendToAll(String msg) {
  for (int i = 0; i < 5; i++) {
    if (phoneNumbers[i] != "") {
      enqueueSMS(phoneNumbers[i], msg);
    }
  }
}

// =====================================================
// BUILD STATUS MESSAGE
// =====================================================
String buildStatusMessage() {
  // Create a formatted status string with sensor values
  String msg = "STATUS:\n";
  msg += "T: " + String(tempValue, 1) + "C\n";
  msg += "H: " + String(humValue, 1) + "%\n";
  msg += "L: " + String(luxValue, 1) + " lux\n";
  msg += "G: " + String(gasPPM, 2) + " ppm";
  return msg;
}

// =====================================================
// SMS HANDLER
// =====================================================
void handleSMSQueue() {
  unsigned long now = millis();

  switch (smsState) {
    case SMS_IDLE:
      if (dequeueSMS(currentSMS)) {        // If SMS in queue
        gsm.print("AT+CMGS=\"");           // Prepare to send
        gsm.print(currentSMS.number);
        gsm.println("\"");
        smsMillis = now;
        smsState = SMS_SEND_BODY;
      }
      break;

    case SMS_SEND_BODY:
      if (now - smsMillis >= 200) {       // Wait small delay
        gsm.print(currentSMS.message);     // Send message
        gsm.write(26);                     // Ctrl+Z to finish SMS
        smsMillis = now;
        smsState = SMS_WAIT_RESULT;
      }
      break;

    case SMS_WAIT_RESULT:
      if (gsmBuffer.indexOf("+CMGS:") != -1 ||
          gsmBuffer.indexOf("OK") != -1 ||
          now - smsMillis > 8000) {

        showLCDMessage(                    // Show SMS sent on TFT
          "SMS SENT",
          "TO:",
          currentSMS.number
        );

        gsmBuffer = "";
        smsMillis = now;
        smsState = SMS_COOLDOWN;
      }
      break;

    case SMS_COOLDOWN:
      if (now - smsMillis >= SMS_COOLDOWN_MS) {
        smsState = SMS_IDLE;              // Ready for next SMS
      }
      break;
  }
}

// =====================================================
// SENSOR READ FUNCTIONS
// =====================================================
bool readAHT20(float* t, float* h) {
  // Communicate with AHT20 over I2C to read temp & humidity
  Wire.beginTransmission(0x38);
  Wire.write(0xAC); Wire.write(0x33); Wire.write(0x00);
  Wire.endTransmission();
  delay(80);
  Wire.requestFrom(0x38, 6);
  if (Wire.available() != 6) return false;

  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();

  uint32_t hum_raw =
    ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
  uint32_t temp_raw =
    ((uint32_t)(d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];

  *h = hum_raw * 100.0 / 1048576.0;
  *t = temp_raw * 200.0 / 1048576.0 - 50.0;
  return true;
}

float readBH1750() {
  // Read light intensity from BH1750 sensor
  Wire.beginTransmission(0x5C);
  Wire.write(0x10);
  Wire.endTransmission();
  Wire.requestFrom(0x5C, 2);
  if (Wire.available() == 2)
    return ((Wire.read() << 8) | Wire.read()) / 1.2;
  return -1;
}

// =====================================================
// MQ136 → PPM CONVERSION
// =====================================================
float readMQ136PPM() {
  // Convert analog voltage to gas ppm using sensor curve
  int adc = analogRead(MQ136_PIN);
  if (adc <= 0) return 0;

  float voltage = adc * (VREF / ADC_MAX);
  float Rs = ((VREF - voltage) / voltage) * RL_VALUE;
  float ratio = Rs / RO_CLEAN;

  float ppm = pow(10, ((log10(ratio) - 0.38) / -0.45)); // Empirical formula
  return ppm;
}

// =====================================================
// GSM HANDLER
// =====================================================
void handleGSM() {
  while (gsm.available()) {              // Read any incoming GSM characters
    gsmBuffer += (char)gsm.read();
  }

  unsigned long now = millis();

  switch (gsmState) {

    case GSM_POWER_OFF:
      gsmState = GSM_POWER_ON;          // Move to power on state
      break;

    case GSM_POWER_ON:
      digitalWrite(PWRKEY, HIGH);       // Pulse PWRKEY to turn on GSM
      delay(100);
      digitalWrite(PWRKEY, LOW);
      gsmState = GSM_INIT;
      gsmInitIndex = 0;
      gsmMillis = now;
      break;

    case GSM_INIT:
      if (now - gsmMillis >= 500 && gsmInitIndex < GSM_INIT_COUNT) {
        gsm.println(gsmInitCmds[gsmInitIndex++]); // Send next init command
        gsmMillis = now;
      }
      if (gsmInitIndex >= GSM_INIT_COUNT) {
        gsmState = GSM_WAIT_NETWORK;    // All init commands sent
      }
      break;

    case GSM_WAIT_NETWORK:
      if (now - gsmMillis >= 1000) {
        gsm.println("AT+CREG?");        // Check network registration
        gsmMillis = now;
      }
      if (gsmBuffer.indexOf(",1") != -1 ||
          gsmBuffer.indexOf(",5") != -1) { // Registered
        gsmReady = true;
        gsmState = GSM_READY;
        gsmBuffer = "";
        sendToAll("ONLINE: Device started");  // Notify all registered numbers
        tft.fillScreen(GC9A01A_BLACK);
        printCentered("GSM READY", 90);
      }
      break;

    case GSM_READY:
      // GSM is ready, handled elsewhere
      break;
  }
}

// =====================================================
// SMS RECEIVE PARSER
// =====================================================
void handleIncomingSMS() {
  int cmtIndex = gsmBuffer.indexOf("+CMT:");
  if (cmtIndex == -1) return;   // No SMS received

  int firstNL = gsmBuffer.indexOf('\n', cmtIndex);
  int secondNL = gsmBuffer.indexOf('\n', firstNL + 1);
  if (firstNL == -1 || secondNL == -1) return;

  String header = gsmBuffer.substring(cmtIndex, firstNL);
  String body   = gsmBuffer.substring(firstNL + 1, secondNL);
  gsmBuffer = gsmBuffer.substring(secondNL + 1);

  int q1 = header.indexOf("\"") + 1;
  int q2 = header.indexOf("\"", q1);
  String sender = header.substring(q1, q2);

  body.trim();
  body.toUpperCase();

  // LCD + enqueue SMS
  if (body == "STATUS") {
    showLCDMessage(
      "SMS RECEIVED",
      "FROM:",
      sender
    );
    enqueueSMS(sender, buildStatusMessage()); // Reply with sensor status
  }
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);           // Initialize serial monitor
  gsm.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX); // Initialize GSM serial
  Wire.begin(21, 22);             // Initialize I2C for sensors
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS); // Initialize SPI for TFT

  pinMode(PWRKEY, OUTPUT);        // GSM power pin
  pinMode(MQ136_PIN, INPUT);      // Gas sensor pin

  tft.begin();                    // Start TFT
  tft.fillScreen(GC9A01A_BLACK);  // Clear screen
  printCentered("Initializing...", 90); // Show startup message
}

// =====================================================
// LOOP
// =====================================================
void loop() {

  handleGSM();                    // Continuously manage GSM states
  if (!gsmReady) return;          // Do not proceed until GSM ready

  handleIncomingSMS();            // Check for new SMS
  handleSMSQueue();               // Send queued SMS

  static unsigned long sensorMillis = 0;
  if (millis() - sensorMillis >= 5000) { // Read sensors every 5s
    sensorMillis = millis();

    if (readAHT20(&tempValue, &humValue)) { // Read temp/humidity
      luxValue = readBH1750();             // Read light
      gasPPM = readMQ136PPM();             // Read gas
    }
  }

  // Check if any threshold exceeded
  bool exceeded =
    tempValue >= TEMP_THRESHOLD ||
    humValue >= HUM_THRESHOLD ||
    luxValue >= LIGHT_THRESHOLD ||
    gasPPM >= GAS_THRESHOLD;

  if (exceeded && !alertSent) {    // Send alert if not already sent
    String msg = "ALERT!\n";
    msg += "T:" + String(tempValue, 1) + "C\n";
    msg += "H:" + String(humValue, 1) + "%\n";
    msg += "L:" + String(luxValue, 1) + " lux\n";
    msg += "G:" + String(gasPPM, 2) + " ppm";
    sendToAll(msg);
    alertSent = true;
  }

  if (!exceeded) alertSent = false; // Reset alert flag if normal

  // Auto-clear SMS LCD after 3s
  if (lcdActive && millis() - lcdMillis > 3000) {
    lcdActive = false;
  }

  // Update live sensor display every 2s if no SMS notification
  if (!lcdActive && millis() - lcdSensorMillis > 2000) {
    lcdSensorMillis = millis();
    displayLiveSensors();
  }
}
