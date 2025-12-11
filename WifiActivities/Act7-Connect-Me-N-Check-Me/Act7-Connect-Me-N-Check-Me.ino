#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <WebServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// ==========================
// 1. WiFi Configuration
// ==========================
const char* ssid = "RiveraWIFI";
const char* password = "@Rivera20214";

// ==========================
// 2. Sensor Addresses
// ==========================
#define AHT20_ADDR 0x38
#define BH1750_ADDR 0x5C

// ==========================
// 3. Hardware Pins
// ==========================
#define LED_PIN 14
#define NUM_LEDS 10
#define BUZZER_PIN 13

#define TFT_CS    5
#define TFT_DC    25
#define TFT_RST   -1  // Software reset
#define TFT_SCLK  18
#define TFT_MOSI  23

// ==========================
// 4. Objects
// ==========================
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);
WebServer server(80);

// ==========================
// 5. Timing Variables
// ==========================
unsigned long lastLedUpdate = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastBuzzerToggle = 0;
int spinningIndex = 0;

const unsigned long SENSOR_INTERVAL = 5000;  // 5s
const unsigned long BUZZER_INTERVAL = 500;  // 0.5s blink

// ==========================
// 6. Thresholds
// ==========================
float TEMP_THRESHOLD = 40.0;    // °C
float HUM_THRESHOLD = 70.0;     // %
float LIGHT_THRESHOLD = 800.0;  // lux

bool thresholdExceeded = false;  // Flag if any threshold exceeded
const int BUZZER_FREQ = 1000;    // 1 kHz buzzer

// ==========================
// 7. Sensor Values
// ==========================
float tempValue = 0;
float humValue = 0;
float luxValue = 0;

// ==========================
// 8. LED Colors
// ==========================
uint32_t colorWiFi = strip.Color(0, 0, 60);   // Blue: WiFi connecting
uint32_t colorReady = strip.Color(0, 60, 0);  // Green: normal ready
uint32_t colorAlert = strip.Color(60, 0, 0);  // Red: threshold exceeded

// ==========================
// 9. Helper Functions
// ==========================

// Fill all LEDs with a color
void setAll(uint32_t color){
  for(int i=0;i<NUM_LEDS;i++){
    strip.setPixelColor(i,color);
  }
  strip.show();
}

// Spinning LED effect
void spinningRing(uint32_t color,int speed=100){
  unsigned long now=millis();
  if(now-lastLedUpdate>speed){
    lastLedUpdate=now;
    strip.clear();
    strip.setPixelColor(spinningIndex,color);
    strip.show();
    spinningIndex=(spinningIndex+1)%NUM_LEDS;
  }
}

// Read BH1750 light sensor
float readBH1750(){
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

// Read AHT20 temperature & humidity sensor
bool readAHT20(float* tempC,float* humidity){
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(80);

  Wire.requestFrom(AHT20_ADDR,6);
  if(Wire.available()!=6) return false;

  uint8_t data[6];
  for(int i=0;i<6;i++) data[i]=Wire.read();

  uint32_t hum_raw=((uint32_t)data[1]<<12)|((uint32_t)data[2]<<4)|((uint32_t)(data[3]>>4));
  uint32_t temp_raw=(((uint32_t)data[3]&0x0F)<<16)|((uint32_t)data[4]<<8)|((uint32_t)data[5]);

  *humidity=((float)hum_raw/1048576.0)*100.0;
  *tempC=((float)temp_raw/1048576.0)*200.0-50.0;

  return true;
}

// Print text centered on TFT
void printCentered(const char* text,int y){
  tft.setTextSize(2);
  tft.setTextColor(GC9A01A_WHITE);
  int16_t x1,y1;
  uint16_t w,h;
  tft.getTextBounds(text,0,0,&x1,&y1,&w,&h);
  int centerX=(240-w)/2;
  tft.setCursor(centerX,y);
  tft.println(text);
}

// Webpage with gauge bars
void handleRoot(){
  String html="<!DOCTYPE html><html><head><meta charset='UTF-8'><title>ESP32 Sensor Dashboard</title>";
  html+="<meta http-equiv='refresh' content='5'>";
  html+="<style>body{font-family:Arial;text-align:center;background:#111;color:#eee;} ";
  html+=".gauge{width:200px;height:20px;background:#333;border-radius:10px;margin:5px auto;} ";
  html+=".fill{height:100%;border-radius:10px;background:#0f0;}</style></head><body>";
  html+="<h1>ESP32 Sensor Dashboard</h1>";

  // Temperature gauge
  int tempPercent = min(int((tempValue/TEMP_THRESHOLD)*100),100);
  html+="<p>Temperature: "+String(tempValue,1)+" °C</p>";
  html+="<div class='gauge'><div class='fill' style='width:"+String(tempPercent)+"%;background:"+(tempPercent>=100?"#f00":"#0f0")+";'></div></div>";

  // Humidity gauge
  int humPercent = min(int((humValue/HUM_THRESHOLD)*100),100);
  html+="<p>Humidity: "+String(humValue,1)+" %</p>";
  html+="<div class='gauge'><div class='fill' style='width:"+String(humPercent)+"%;background:"+(humPercent>=100?"#f00":"#00f")+";'></div></div>";

  // Light gauge
  int luxPercent = min(int((luxValue/LIGHT_THRESHOLD)*100),100);
  html+="<p>Light: "+String(luxValue,1)+" lux</p>";
  html+="<div class='gauge'><div class='fill' style='width:"+String(luxPercent)+"%;background:"+(luxPercent>=100?"#f00":"#ff0")+";'></div></div>";

  html+="<p>ESP32 IP: "+WiFi.localIP().toString()+"</p>";
  html+="</body></html>";
  server.send(200,"text/html",html);
}

// ==========================
// 10. Setup
// ==========================
void setup(){
  Serial.begin(115200);

  // Initialize LED strip
  strip.begin();
  strip.show();
  pinMode(BUZZER_PIN,OUTPUT);
  digitalWrite(BUZZER_PIN,LOW);

  Wire.begin(21,22); // I2C

  // TFT init
  SPI.begin(TFT_SCLK,-1,TFT_MOSI,TFT_CS);
  tft.begin();
  tft.fillScreen(GC9A01A_BLACK);

  // Display "Connecting to WiFi"
  tft.fillScreen(GC9A01A_BLACK);
  printCentered("Connecting",60);
  printCentered("to",90);
  printCentered("WiFi",120);

  // Connect WiFi
  WiFi.begin(ssid,password);

  while(WiFi.status()!=WL_CONNECTED){
    spinningRing(colorWiFi,150); // LED animation while connecting
    delay(100);
  }

  // WiFi connected tone
  tone(BUZZER_PIN, 1000, 200);

  // Show IP on TFT
  tft.fillScreen(GC9A01A_BLACK);
  printCentered("WiFi Connected!",60);
  printCentered(WiFi.localIP().toString().c_str(),90);

  // Start web server
  server.on("/",handleRoot);
  server.begin();
}

// ==========================
// 11. Loop
// ==========================
void loop(){
  unsigned long now=millis();

  // Handle WiFi disconnect
  if(WiFi.status()!=WL_CONNECTED){
    spinningRing(colorWiFi,150);
    tft.fillScreen(GC9A01A_BLACK);
    printCentered("Reconnecting...",80);
    thresholdExceeded=false;
    noTone(BUZZER_PIN);
    return;
  } else {
    setAll(colorReady);
  }

  // Handle web server
  server.handleClient();

  // Sensor reading every 5s
  if(now-lastSensorUpdate>SENSOR_INTERVAL){
    lastSensorUpdate=now;
    if(readAHT20(&tempValue,&humValue)) Serial.printf("Temp: %.1fC Hum: %.1f%%\n",tempValue,humValue);
    float lux=readBH1750();
    if(lux >= 0) {
        luxValue = lux;
        Serial.printf("Light: %.1f lux\n", luxValue);  // <-- Add this line
    }
    // Print values on TFT
    tft.fillScreen(GC9A01A_BLACK);
    printCentered(("T: "+String(tempValue,1)+"C").c_str(),60);
    printCentered(("H: "+String(humValue,1)+"%").c_str(),90);
    printCentered(("L: "+String(luxValue,1)+"lux").c_str(),120);

    // Check thresholds
    thresholdExceeded=(tempValue>=TEMP_THRESHOLD || humValue>=HUM_THRESHOLD || luxValue>=LIGHT_THRESHOLD);
  }

  // Blink LEDs + buzzer when threshold exceeded
  if(thresholdExceeded){
    if(now-lastBuzzerToggle>BUZZER_INTERVAL){
      lastBuzzerToggle=now;
      static bool alertOn=false;
      alertOn=!alertOn;
      if(alertOn){
        setAll(colorAlert);
        tone(BUZZER_PIN,BUZZER_FREQ);
      } else {
        setAll(0);
        noTone(BUZZER_PIN);
      }
    }
  } else {
    setAll(colorReady);
    noTone(BUZZER_PIN);
  }
}
