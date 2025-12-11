#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

/* ===================================================
  1. WiFi Hotspot (ESP32 AP Mode)
===================================================
The ESP32 acts as its own WiFi network. Any device can
connect directly to it and access the web page.
*/
const char* ssid     = "ESP32_Hotspot";
const char* password = "12345678";

/* ===================================================
  2. Pin Definitions
===================================================
- LED_PIN: NeoPixel RGB strip data pin
- LED_COUNT: Number of LEDs
- BUZZER_PIN: Passive buzzer
- TFT SPI pins
- TFT_RST = -1 means we'll use software reset instead of hardware
*/
#define LED_PIN     14
#define LED_COUNT   10
#define BUZZER_PIN  13
#define TFT_CS    5
#define TFT_DC    25
#define TFT_RST   -1
#define TFT_SCLK  18
#define TFT_MOSI  23

/* ===================================================
  3. Objects
===================================================
- strip: controls NeoPixel LEDs
- tft: controls the 1.28" round TFT display
- server: web server running on port 80
*/
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);
WebServer server(80);

/* ===================================================
  4. State Variables
===================================================
Tracks LED color, sequence, buzzer tune, brightness,
TFT text, and non-blocking timers.
*/
uint32_t currentColor = 0x003C00; // default green
int brightness = 128;              // default brightness (0-255)

enum SequenceType {CIRCLING, ALL_ON, FADE};
SequenceType currentSeq = CIRCLING;

enum BuzzerTune {NONE, NOKIA, USB, DARTH, MARIO, ALERT};
BuzzerTune currentTune = NONE;

String lcdText = "";

// Non-blocking timers
unsigned long lastLedUpdate   = 0;
unsigned long lastFadeUpdate  = 0;
int spinningIndex             = 0;
int fadeValue                 = 0;
int fadeDirection             = 5;

unsigned long lastToneTime    = 0;
int toneIndex                 = 0;

/* ===================================================
  5. LED Control Functions
===================================================
- setAll: sets all LEDs to the same color with brightness
- spinningRing: rotates a single LED around the strip
- fadeLED: fades LEDs in/out
*/
void setAll(uint32_t color) {
  for (int i = 0; i < LED_COUNT; i++) {
    uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
    uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
    uint8_t b = (color & 0xFF) * brightness / 255;
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

void spinningRing(uint32_t color, int speed = 100) {
  if (millis() - lastLedUpdate > speed) {
    lastLedUpdate = millis();
    strip.clear();
    uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
    uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
    uint8_t b = (color & 0xFF) * brightness / 255;
    strip.setPixelColor(spinningIndex, strip.Color(r,g,b));
    strip.show();
    spinningIndex = (spinningIndex + 1) % LED_COUNT;
  }
}

void fadeLED(uint32_t color) {
  if (millis() - lastFadeUpdate > 50) {
    lastFadeUpdate = millis();
    int r = ((color >> 16) & 0xFF) * fadeValue / 255 * brightness / 255;
    int g = ((color >> 8) & 0xFF) * fadeValue / 255 * brightness / 255;
    int b = (color & 0xFF) * fadeValue / 255 * brightness / 255;
    setAll(strip.Color(r,g,b));
    fadeValue += fadeDirection;
    if (fadeValue >= 255 || fadeValue <= 0) fadeDirection *= -1;
  }
}

/* ===================================================
  6. TFT Display Functions
===================================================
- printCentered: prints text horizontally centered
- tftSoftwareReset: resets TFT using software command
*/
void printCentered(const char* text, int y) {
  tft.setTextSize(2);
  tft.setTextColor(GC9A01A_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text,0,0,&x1,&y1,&w,&h);
  int centerX = (240 - w)/2;
  tft.setCursor(centerX, y);
  tft.println(text);
}

void tftSoftwareReset() {
  tft.writeCommand(GC9A01A_SWRESET);
  delay(150);
}

/* ===================================================
  7. Buzzer Tunes
===================================================
Define multiple tunes using frequency (Hz) and duration (ms)
*/
struct ToneNote { int freq; int dur; };

ToneNote nokiaTone[] = {{659,100},{659,100},{523,100},{659,100},{784,200}};
ToneNote usbTone[]   = {{1000,100},{1500,100},{2000,200}};
ToneNote darthTone[] = {{98,500},{117,500},{147,500},{98,500}};
ToneNote marioTone[] = {{659,100},{659,100},{0,50},{659,100},{0,50},{523,100},{659,100},{784,200}};
ToneNote alertTone[] = {{1000,150},{0,50},{1000,150}};

void playTune(BuzzerTune tune) {
  ToneNote* notes;
  int len;
  switch(tune) {
    case NOKIA:  notes = nokiaTone; len = sizeof(nokiaTone)/sizeof(ToneNote); break;
    case USB:    notes = usbTone;   len = sizeof(usbTone)/sizeof(ToneNote);   break;
    case DARTH:  notes = darthTone; len = sizeof(darthTone)/sizeof(ToneNote); break;
    case MARIO:  notes = marioTone; len = sizeof(marioTone)/sizeof(ToneNote); break;
    case ALERT:  notes = alertTone; len = sizeof(alertTone)/sizeof(ToneNote); break;
    default: return;
  }
  if (millis() - lastToneTime > notes[toneIndex].dur + 50) {
    if (toneIndex < len) {
      tone(BUZZER_PIN, notes[toneIndex].freq, notes[toneIndex].dur);
      lastToneTime = millis();
      toneIndex++;
    } else {
      noTone(BUZZER_PIN);
      toneIndex = 0;
      currentTune = NONE;
    }
  }
}

/* ===================================================
  8. Web Server Handler with Dynamic Page
===================================================
Dropdowns for color, sequence, buzzer tune, brightness,
and TFT text. Selected/entered values are retained.
*/
void handleRoot() {
  // ----- Update state from form submission -----
  if (server.hasArg("color")) {
    String col = server.arg("color");
    if (col.length()==6) currentColor = strtol(col.c_str(),NULL,16);
  }

  if (server.hasArg("seq")) {
    String s = server.arg("seq");
    if(s=="circling") currentSeq=CIRCLING;
    else if(s=="all") currentSeq=ALL_ON;
    else if(s=="fade") currentSeq=FADE;
  }

  if (server.hasArg("tune")) {
    String t = server.arg("tune");
    if(t=="nokia") {currentTune=NOKIA; toneIndex=0;}
    else if(t=="usb") {currentTune=USB; toneIndex=0;}
    else if(t=="darth") {currentTune=DARTH; toneIndex=0;}
    else if(t=="mario") {currentTune=MARIO; toneIndex=0;}
    else if(t=="alert") {currentTune=ALERT; toneIndex=0;}
    else currentTune=NONE;
  }

  if (server.hasArg("brightness")) brightness = server.arg("brightness").toInt();
  if (server.hasArg("lcdtext")) {
    lcdText = server.arg("lcdtext");
    tft.fillScreen(GC9A01A_BLACK);
    printCentered(lcdText.c_str(),100);
  }

  // ----- Generate Dynamic HTML -----
  String html = "<!DOCTYPE html><html><head><title>ESP32 RGB Dashboard</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'></head><body>";
  html += "<h2>ESP32 RGB Control</h2><form method='GET'>";

  // Color dropdown
  html += "<label>Color:</label><select name='color'>";
  String colors[8] = {"FF0000","00FF00","0000FF","FFFF00","FF00FF","00FFFF","FFFFFF","FFA500"};
  String names[8]  = {"Red","Green","Blue","Yellow","Magenta","Cyan","White","Orange"};
  for(int i=0;i<8;i++){
    html += "<option value='"+colors[i]+"'";
    if(currentColor == strtol(colors[i].c_str(),NULL,16)) html += " selected";
    html += ">"+names[i]+"</option>";
  }
  html += "</select><br><br>";

  // Sequence dropdown
  html += "<label>Sequence:</label><select name='seq'>";
  html += "<option value='circling'" + String(currentSeq==CIRCLING?" selected":"") + ">Circling</option>";
  html += "<option value='all'"      + String(currentSeq==ALL_ON?" selected":"") + ">All On</option>";
  html += "<option value='fade'"     + String(currentSeq==FADE?" selected":"") + ">Fade</option>";
  html += "</select><br><br>";

  // Buzzer dropdown
  html += "<label>Buzzer Tune:</label><select name='tune'>";
  html += "<option value='none'"  + String(currentTune==NONE?" selected":"") + ">None</option>";
  html += "<option value='nokia'" + String(currentTune==NOKIA?" selected":"") + ">Nokia</option>";
  html += "<option value='usb'"   + String(currentTune==USB?" selected":"") + ">USB Connected</option>";
  html += "<option value='darth'" + String(currentTune==DARTH?" selected":"") + ">Darth Vader</option>";
  html += "<option value='mario'" + String(currentTune==MARIO?" selected":"") + ">Mario</option>";
  html += "<option value='alert'" + String(currentTune==ALERT?" selected":"") + ">Alert</option>";
  html += "</select><br><br>";

  // Brightness dropdown
  html += "<label>Brightness:</label><select name='brightness'>";
  int levels[5] = {50,100,128,180,255};
  for(int i=0;i<5;i++){
    html += "<option value='"+String(levels[i])+"'";
    if(brightness==levels[i]) html += " selected";
    html += ">"+String(levels[i])+"</option>";
  }
  html += "</select><br><br>";

  // TFT text input
  html += "<label>Text for TFT:</label>";
  html += "<input type='text' name='lcdtext' value='"+lcdText+"'><br><br>";

  html += "<input type='submit' value='Update'></form></body></html>";
  server.send(200,"text/html",html);
}

/* ===================================================
  9. Setup
===================================================
Initialize LEDs, TFT, WiFi AP, and web server
*/
void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.show();

  SPI.begin(TFT_SCLK,-1,TFT_MOSI,TFT_CS);
  tft.begin();
  tftSoftwareReset();
  tft.setRotation(4);
  tft.fillScreen(GC9A01A_BLACK);

  // Start WiFi Hotspot
  WiFi.softAP(ssid,password);
  IPAddress IP = WiFi.softAPIP();
  char ipStr[25];
  sprintf(ipStr,"IP: %d.%d.%d.%d",IP[0],IP[1],IP[2],IP[3]);
  printCentered("Hotspot Active!", 80);
  printCentered(ipStr,110);

  // Start web server
  server.on("/",handleRoot);
  server.begin();
  Serial.println("Webserver running...");
}

/* ===================================================
  10. Loop
===================================================
- Handle web requests
- Update LED animations non-blocking
- Play buzzer tunes non-blocking
*/
void loop() {
  server.handleClient();

  switch(currentSeq){
    case CIRCLING: spinningRing(currentColor,100); break;
    case ALL_ON:   setAll(currentColor); break;
    case FADE:     fadeLED(currentColor); break;
  }

  if(currentTune != NONE) playTune(currentTune);
}
