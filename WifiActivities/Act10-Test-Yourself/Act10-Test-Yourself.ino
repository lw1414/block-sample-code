/*
  ESP32 RGB Controller with Joystick, MQTT, TFT Display
  ----------------------------------------------------
  Teaching Version: Every section has step-by-step explanation.
  
  Features:
  - Joystick X-axis controls hue (0-360°) for RGB NeoPixel strip
  - Subscribe "dimmer" topic to adjust brightness (0-100%)
  - Subscribe "announce" topic to display text on TFT centered
  - Announce text disappears when joystick moves → crosshair reappears
  - WiFi + MQTT connection flashes via RGB strip (brief)
  - RGB status always reflects current hue/brightness
  - All millis-based (non-blocking) for smooth operation
*/

#include <WiFi.h>              // For WiFi connectivity
#include <PubSubClient.h>      // For MQTT communication
#include <Adafruit_NeoPixel.h> // For RGB NeoPixel strip
#include <SPI.h>               // SPI for TFT
#include <Adafruit_GFX.h>      // Graphics library for TFT
#include <Adafruit_GC9A01A.h>  // TFT driver library
#include <ArduinoJson.h>       // For JSON formatting of RGB values

// ------------------------
// Pins Configuration
// ------------------------
#define JOY_X_PIN 32        // Joystick X-axis
#define JOY_Y_PIN 33        // Joystick Y-axis
#define LED_PIN   14        // NeoPixel data pin
#define NUM_LEDS  10        // Number of LEDs in the strip
#define BUZZER_PIN 13       // Passive buzzer pin

// TFT pins
#define TFT_CS    5
#define TFT_DC    25
#define TFT_RST   14
#define TFT_SCLK  18
#define TFT_MOSI  23

// Create TFT object
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// Create NeoPixel object
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ------------------------
// WiFi / MQTT Configuration
// ------------------------
const char* ssid = "WIFI";         // WiFi SSID
const char* password = "PASS1234";   // WiFi Password

const char* mqtt_server = "innovph.com"; // MQTT Broker
const int   mqtt_port   = 1883;          // MQTT Port
const char* mqtt_user   = "mqtt";        // MQTT Username
const char* mqtt_pass   = "ICPHmqtt!";   // MQTT Password
const char* deviceId    = "esp01";       // Device ID for topics

// MQTT topics
char topicRGB[64];       // RGB publish topic
char topicDimmer[64];    // Dimmer subscription topic
char topicAnnounce[64];  // Announce subscription topic

// ------------------------
// MQTT + WiFi clients
// ------------------------
WiFiClient espClient;
PubSubClient mqtt(espClient);

// ------------------------
// State Variables
// ------------------------
int currentHue = 0;             // 0–360°, controlled by joystick X
int currentBrightness = 128;    // 0–255 (NeoPixel brightness)
bool announceActive = false;    // True if announce text displayed

// Last joystick positions for detecting movement
int lastJoyX = 0;
int lastJoyY = 0;
const int JOY_THRESHOLD = 20;   // Minimum movement to consider joystick moved

// Flash timing for WiFi/MQTT brief flashes
unsigned long previousFlashMillis = 0;
const int flashDuration = 300;  // RGB flash duration in ms
bool flashActive = false;

// Throttle RGB publishing to MQTT
unsigned long lastPublishMillis = 0;
const unsigned long PUBLISH_INTERVAL = 200; // publish every 200ms

// ------------------------
// Function Prototypes
// ------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();
void setStripHueBrightness(int hue, int brightness255);
uint32_t HSVtoRGBpacked(int h,int s_percent,int v);
void drawCrosshair(int joyX,int joyY);
void handleDimmerMessage(int value);
void handleAnnounceMessage(String text);
void publishRGB();

// ------------------------
// Setup function
// ------------------------
void setup() {
  Serial.begin(115200); // Serial monitor for debugging

  // ------------------------
  // TFT Initialization
  // ------------------------
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS); // Begin SPI
  tft.begin();                // Initialize TFT
  tft.setRotation(0);         // Default rotation
  tft.fillScreen(GC9A01A_BLACK); // Clear screen to black

  // ------------------------
  // NeoPixel Initialization
  // ------------------------
  strip.begin();  // Initialize NeoPixel strip
  strip.show();   // Turn all LEDs off initially

  // ------------------------
  // Buzzer Initialization
  // ------------------------
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN); // Ensure buzzer off initially

  // ------------------------
  // WiFi Connection
  // ------------------------
  WiFi.begin(ssid, password); // Start connecting to WiFi

  // ------------------------
  // MQTT Setup
  // ------------------------
  mqtt.setServer(mqtt_server, mqtt_port); // Configure broker
  mqtt.setCallback(mqttCallback);         // Set callback for incoming messages

  // ------------------------
  // Prepare MQTT topic strings
  // ------------------------
  snprintf(topicRGB, sizeof(topicRGB), "WiFi/act10/%s/rgb", deviceId);
  snprintf(topicDimmer, sizeof(topicDimmer), "WiFi/act10/%s/dimmer", deviceId);
  snprintf(topicAnnounce, sizeof(topicAnnounce), "WiFi/act10/%s/announce", deviceId);
}

// ------------------------
// Main loop
// ------------------------
void loop() {
  unsigned long now = millis(); // Current time in milliseconds

  // ------------------------
  // WiFi / MQTT brief flashes
  // This visually indicates connecting status on NeoPixel
  // ------------------------
  if(flashActive && now - previousFlashMillis >= flashDuration){
    flashActive = false;                     // Turn off flash after duration
    setStripHueBrightness(currentHue,currentBrightness); // Restore normal color
  }

  if(!flashActive){
    if(WiFi.status() != WL_CONNECTED){
      // WiFi is connecting → flash blue
      for(int i=0;i<NUM_LEDS;i++) strip.setPixelColor(i, strip.Color(0,0,255));
      strip.show();
      flashActive = true;
      previousFlashMillis = now;
    } else if(WiFi.status() == WL_CONNECTED && !mqtt.connected()){
      // MQTT is connecting → flash cyan
      for(int i=0;i<NUM_LEDS;i++) strip.setPixelColor(i, strip.Color(0,150,150));
      strip.show();
      flashActive = true;
      previousFlashMillis = now;
    }
  }

  // ------------------------
  // Ensure MQTT connection
  // ------------------------
  if(WiFi.status() == WL_CONNECTED && !mqtt.connected()) reconnectMQTT();
  mqtt.loop(); // Keep MQTT alive

  // ------------------------
  // Read Joystick
  // ------------------------
  int joyX = analogRead(JOY_X_PIN); // 0-4095
  int joyY = analogRead(JOY_Y_PIN); // 0-4095

  // ------------------------
  // Detect joystick movement → restore crosshair if announce is active
  // ------------------------
  if(announceActive){
    if(abs(joyX - lastJoyX) > JOY_THRESHOLD || abs(joyY - lastJoyY) > JOY_THRESHOLD){
      announceActive = false;               // Remove announce
      tft.fillScreen(GC9A01A_BLACK);       // Clear screen
    }
  }

  // Update last joystick positions
  lastJoyX = joyX;
  lastJoyY = joyY;

  // ------------------------
  // Map joystick X-axis to hue (0-360°)
  // ------------------------
  currentHue = map(joyX,0,4095,0,360);
  setStripHueBrightness(currentHue,currentBrightness); // Update NeoPixel

  // ------------------------
  // Draw crosshair if announce not active
  // ------------------------
  if(!announceActive) drawCrosshair(joyX,joyY);

  // ------------------------
  // Publish RGB JSON periodically
  // ------------------------
  if(now - lastPublishMillis >= PUBLISH_INTERVAL){
    lastPublishMillis = now;
    if(mqtt.connected()) publishRGB();
  }

  delay(60); // Small UI refresh delay
}

// ------------------------
// MQTT Callback
// ------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length){
  // Convert payload to string
  payload[length] = '\0';
  String msg = String((char*)payload);
  msg.trim();
  Serial.print("MQTT RX: "); Serial.print(topic); Serial.println(msg);

  if(String(topic) == topicDimmer){
    handleDimmerMessage(msg.toInt());  // Update brightness
  } else if(String(topic) == topicAnnounce){
    handleAnnounceMessage(msg);        // Show announce text
  }
}

// ------------------------
// MQTT Reconnect Function
// ------------------------
void reconnectMQTT(){
  if(mqtt.connected()) return;         // Already connected, nothing to do
  Serial.println("Connecting MQTT...");
  if(mqtt.connect(deviceId,mqtt_user,mqtt_pass)){
    Serial.println("MQTT connected!");
    mqtt.subscribe(topicDimmer);       // Subscribe to dimmer
    mqtt.subscribe(topicAnnounce);     // Subscribe to announce

    // Flash cyan briefly to indicate connection
    for(int i=0;i<NUM_LEDS;i++) strip.setPixelColor(i,strip.Color(0,150,150));
    strip.show();
    delay(150);
    setStripHueBrightness(currentHue,currentBrightness);
  } else {
    Serial.print("MQTT failed rc=");
    Serial.println(mqtt.state());
  }
}

// ------------------------
// Publish RGB JSON to MQTT
// ------------------------
void publishRGB(){
  StaticJsonDocument<64> doc;
  uint32_t rgb = HSVtoRGBpacked(currentHue,100,currentBrightness);
  doc["r"] = (rgb>>16)&0xFF;
  doc["g"] = (rgb>>8)&0xFF;
  doc["b"] = rgb&0xFF;
  char buffer[64];
  serializeJson(doc,buffer);
  mqtt.publish(topicRGB,buffer);
}

// ------------------------
// Handle dimmer message (0-100) → adjust brightness
// ------------------------
void handleDimmerMessage(int value){
  if(value<0)value=0;
  if(value>100)value=100;
  currentBrightness = map(value,0,100,0,255);
  setStripHueBrightness(currentHue,currentBrightness);
}

// ------------------------
// Handle announce message → centered text
// ------------------------
void handleAnnounceMessage(String text){
  announceActive = true;                 // Flag announce active
  tft.fillScreen(GC9A01A_BLACK);        // Clear screen

  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(2);

  // Calculate centered position
  int16_t x1,y1;
  uint16_t w,h;
  tft.getTextBounds(text,0,0,&x1,&y1,&w,&h);
  int cx = (240 - w)/2;                  // Center X
  int cy = (240 - h)/2;                  // Center Y
  tft.setCursor(cx,cy);
  tft.print(text);                       // Display text
}

// ------------------------
// Draw joystick crosshair
// ------------------------
void drawCrosshair(int joyX,int joyY){
  int cx=120, cy=120;                     // TFT center
  int dx = map(joyY,4095,0,-40,40);       // Map Y-axis
  int dy = map(joyX,0,4095,-40,40);       // Map X-axis

  tft.fillCircle(cx,cy,60,GC9A01A_BLACK); // Clear previous crosshair area
  tft.drawLine(cx-60,cy,cx+60,cy,GC9A01A_DARKGREY); // Horizontal line
  tft.drawLine(cx,cy-60,cx,cy+60,GC9A01A_DARKGREY); // Vertical line
  tft.fillCircle(cx+dx,cy-dy,6,GC9A01A_RED);        // Red dot at joystick
}

// ------------------------
// Convert HSV to packed RGB
// ------------------------
uint32_t HSVtoRGBpacked(int h,int s_percent,int v){
  if(h<0)h=0;if(h>360)h=360;
  if(s_percent<0)s_percent=0;if(s_percent>100)s_percent=100;
  int s=(s_percent*255)/100;
  if(s==0) return strip.Color(v,v,v); // Gray
  float hf=(float)h/60.0f;
  int i=floor(hf);
  float f=hf-i;
  int p=(int)((v*(255-s))/255.0f);
  int q=(int)((v*(255-(s*f)))/255.0f);
  int t=(int)((v*(255-(s*(1.0f-f))))/255.0f);
  uint8_t r,g,b;
  switch(i%6){
    case 0:r=v;g=t;b=p;break;
    case 1:r=q;g=v;b=p;break;
    case 2:r=p;g=v;b=t;break;
    case 3:r=p;g=q;b=v;break;
    case 4:r=t;g=p;b=v;break;
    case 5:r=v;g=p;b=q;break;
    default:r=v;g=v;b=v;break;
  }
  return strip.Color(r,g,b);
}

// ------------------------
// Set NeoPixel color by Hue/Brightness
// ------------------------
void setStripHueBrightness(int hue,int brightness255){
  uint32_t packed=HSVtoRGBpacked(hue,100,brightness255);
  for(int i=0;i<NUM_LEDS;i++) strip.setPixelColor(i,packed);
  strip.show();
}
