#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <HardwareSerial.h>

// ==========================
// Sensor Addresses
// ==========================
#define AHT20_ADDR 0x38
#define BH1750_ADDR 0x5C

// ==========================
// Hardware Pins
// ==========================
#define TFT_CS    5
#define TFT_DC    25
#define TFT_RST   -1
#define TFT_SCLK  18
#define TFT_MOSI  23

// ==========================
// GSM Pins
// ==========================
#define GSM_RX 17
#define GSM_TX 16
#define PWRKEY 4

HardwareSerial gsm(1); // UART1 for GSM

// ==========================
// Objects
// ==========================
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// ==========================
// Timing Variables
// ==========================
unsigned long previousPowerMillis = 0;
unsigned long previousPingMillis = 0;
const unsigned long GSM_POWER_DELAY = 1200;
const unsigned long GSM_PING_INTERVAL = 60000; // 60s ping

// ==========================
// Thresholds
// ==========================
float TEMP_THRESHOLD = 40.0;
float HUM_THRESHOLD = 70.0;
float LIGHT_THRESHOLD = 800.0;

// ==========================
// Sensor Values
// ==========================
float tempValue = 0;
float humValue = 0;
float luxValue = 0;

// ==========================
// GSM numbers
// ==========================
String phoneNumbers[5] = {
  "+639928900314",
  "+639916425321",
  "",
  "",
  ""
};

bool alertSent = false;

// ==========================
// Helper Functions
// ==========================
void powerOnSIM800() {
  pinMode(PWRKEY, OUTPUT);
  digitalWrite(PWRKEY, HIGH);  
  delay(100);
  digitalWrite(PWRKEY, LOW);    // Hold LOW for 1.2s to power on
  delay(GSM_POWER_DELAY);
  digitalWrite(PWRKEY, HIGH);   // Release HIGH
  delay(5000);                   // Wait module fully boots
}

void updatePowerKey() {
  // not used in this version; PWRKEY only in setup
}

void sendAT(String command) {
  gsm.println(command);
  delay(200);
  while (gsm.available()) Serial.write(gsm.read());
}

void initGSM() {
  sendAT("AT");          // Test communication
  sendAT("AT+CFUN=1");   // Full functionality
  sendAT("AT+CSCLK=0");  // Disable sleep
  Serial.println("GSM Initialized");
}

bool waitForNetwork(int timeout=30000){
  unsigned long start = millis();
  Serial.println("Waiting for network registration...");
  while(millis() - start < timeout){
    sendAT("AT+CREG?");
    if(gsm.available()){
      String resp = gsm.readString();
      if(resp.indexOf(",1")!=-1 || resp.indexOf(",5")!=-1){ // Registered
        Serial.println("Network registered!");
        return true;
      }
    }
    delay(500);
  }
  Serial.println("Network registration timeout!");
  return false;
}

void sendSMS(String number, String message, int index) {
  sendAT("AT+CMGF=1"); // Text mode
  gsm.print("AT+CMGS=\""); gsm.print(number); gsm.println("\"");
  delay(500);
  gsm.print(message);
  delay(500);
  gsm.write(26); // CTRL+Z
  delay(3000); // Wait for send completion
  Serial.printf("SMS sent to number %d: %s\n", index+1, number);
}

void sendSMSAll(String message) {
  for(int i=0;i<5;i++){
    if(phoneNumbers[i] != "") sendSMS(phoneNumbers[i], message, i);
  }
}

// ==========================
// Sensor Reading
// ==========================
float readBH1750() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10);
  Wire.endTransmission();
  delay(180);
  Wire.requestFrom(BH1750_ADDR,2);
  if(Wire.available()==2){
    uint16_t raw=Wire.read()<<8 | Wire.read();
    return raw/1.2;
  }
  return -1;
}

bool readAHT20(float* tempC,float* humidity) {
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xAC); Wire.write(0x33); Wire.write(0x00);
  Wire.endTransmission();
  delay(80);

  Wire.requestFrom(AHT20_ADDR,6);
  if(Wire.available()!=6) return false;
  uint8_t data[6]; for(int i=0;i<6;i++) data[i]=Wire.read();

  uint32_t hum_raw=((uint32_t)data[1]<<12)|((uint32_t)data[2]<<4)|((uint32_t)(data[3]>>4));
  uint32_t temp_raw=(((uint32_t)data[3]&0x0F)<<16)|((uint32_t)data[4]<<8)|((uint32_t)data[5]);

  *humidity=((float)hum_raw/1048576.0)*100.0;
  *tempC=((float)temp_raw/1048576.0)*200.0-50.0;

  return true;
}

// ==========================
// TFT Helper
// ==========================
void printCentered(const char* text,int y) {
  tft.setTextSize(2); tft.setTextColor(GC9A01A_WHITE);
  int16_t x1,y1; uint16_t w,h;
  tft.getTextBounds(text,0,0,&x1,&y1,&w,&h);
  int centerX=(240-w)/2;
  tft.setCursor(centerX,y);
  tft.println(text);
}

// ==========================
// Setup
// ==========================
void setup() {
  Serial.begin(115200);
  Wire.begin(21,22);

  SPI.begin(TFT_SCLK,-1,TFT_MOSI,TFT_CS);
  tft.begin(); tft.fillScreen(GC9A01A_BLACK);
  printCentered("Initializing...",80);

  gsm.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
  powerOnSIM800();
  initGSM();

  // Wait for network before allowing alerts
  waitForNetwork();
}

// ==========================
// Loop
// ==========================
void loop() {
  unsigned long currentMillis = millis();

  // Keep-alive ping
  static unsigned long previousPingMillis = 0;
  if(currentMillis - previousPingMillis >= GSM_PING_INTERVAL){
    previousPingMillis = currentMillis;
    sendAT("AT");
    Serial.println("GSM Keep-alive Ping Sent");
  }

  // --- SENSOR READING ---
  if(readAHT20(&tempValue,&humValue)){
    float lux = readBH1750(); if(lux>=0) luxValue = lux;

    // Check threshold
    bool thresholdExceeded = (tempValue>=TEMP_THRESHOLD || humValue>=HUM_THRESHOLD || luxValue>=LIGHT_THRESHOLD);

    // --- PRIORITY SMS ---
    if(thresholdExceeded && !alertSent){
      tft.fillScreen(GC9A01A_BLACK);
      printCentered("Threshold Exceeded!",90);

      String msg = "ALERT! Threshold exceeded!\n";
      msg += "T: " + String(tempValue,1) + "C\n";
      msg += "H: " + String(humValue,1) + "%\n";
      msg += "L: " + String(luxValue,1) + " lux";

      Serial.println("Threshold exceeded! Sending SMS to all numbers...");
      // Send SMS to all numbers with index printing
      for(int i=0;i<5;i++){
        if(phoneNumbers[i] != "") sendSMS(phoneNumbers[i], msg, i);
      }

      alertSent = true;
      Serial.println("All SMS sent. Resuming normal operation.");
    }

    if(!thresholdExceeded) alertSent = false; // reset flag when normal

    // Update TFT AFTER SMS sent or if no alert
    if(!thresholdExceeded || alertSent){
      tft.fillScreen(GC9A01A_BLACK);
      printCentered(("T: "+String(tempValue,1)+"C").c_str(),60);
      printCentered(("H: "+String(humValue,1)+"%").c_str(),90);
      printCentered(("L: "+String(luxValue,1)+"lux").c_str(),120);
    }
  }
}
