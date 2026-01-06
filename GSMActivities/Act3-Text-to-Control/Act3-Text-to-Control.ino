#include <HardwareSerial.h>
#include <Adafruit_NeoPixel.h>

#define GSM_RX 17
#define GSM_TX 16
#define PWRKEY 4
HardwareSerial gsm(1);

#define LED_PIN 14
#define NUM_LEDS 10
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ----------------------------------------------------
// Color structure for storing RGB values
// ----------------------------------------------------
struct RGB { uint8_t r, g, b; };

// Convert text → actual RGB color
RGB getColor(String color) {
  color.trim();
  color.toUpperCase();

  if (color == "RED")     return {255, 0, 0};
  if (color == "GREEN")   return {0, 255, 0};
  if (color == "BLUE")    return {0, 0, 255};
  if (color == "YELLOW")  return {255, 255, 0};
  if (color == "CYAN")    return {0, 255, 255};
  if (color == "MAGENTA") return {255, 0, 255};
  if (color == "WHITE")   return {255, 255, 255};
  if (color == "OFF")     return {0, 0, 0};

  return {0, 255, 0};  // default = GREEN
}
//message format- BRIGHTNESS:100 COLOR:RED
// ----------------------------------------------------
// Apply color + brightness to NeoPixel strip
// ----------------------------------------------------
void setNeoPixelColor(RGB col, int brightness = 255) {
  brightness = constrain(brightness, 0, 255);

  uint8_t r = (col.r * brightness) / 255;
  uint8_t g = (col.g * brightness) / 255;
  uint8_t b = (col.b * brightness) / 255;

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

// ----------------------------------------------------
// GLOBAL STATE
// ----------------------------------------------------
String smsBuffer = "";

RGB lastColor = {0, 255, 0}; // Default GREEN
int currentBrightness = 200;

unsigned long previousMillis = 0;
unsigned long blinkTimer = 0;
bool blinkState = false;

bool noSignal = false;               // updated via +CSQ:
bool waitingForSMSBody = false;      // true when next line is actual SMS text

const long interval = 50;

// ----------------------------------------------------
// Request for GSM signal quality (CSQ)
// ----------------------------------------------------
void checkGSMSignal() {
  gsm.println("AT+CSQ");
}

// ----------------------------------------------------
// SETUP
// ----------------------------------------------------
void setup() {
  Serial.begin(115200);
  gsm.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
  pinMode(PWRKEY, OUTPUT);

  // Proper SIM800C power-on pulse
  digitalWrite(PWRKEY, HIGH);
  delay(1000);
  digitalWrite(PWRKEY, LOW);
  delay(5000);

  Serial.println("Booting SIM800C...");

  // Text mode SMS
  gsm.println("AT+CMGF=1");
  delay(500);

  // Auto print received SMS to serial
  gsm.println("AT+CNMI=2,2,0,0,0");
  delay(500);

  strip.begin();
  setNeoPixelColor(lastColor, currentBrightness);

  Serial.println("GSM Ready… Waiting for signal.");
}

// ----------------------------------------------------
// MAIN LOOP
// ----------------------------------------------------
void loop() {

  unsigned long currentMillis = millis();

  // ----------- NON-BLOCKING GSM CHECK -----------
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Collect all characters from GSM
    while (gsm.available()) {
      char c = gsm.read();
      smsBuffer += c;
    }

    // Process complete lines only
    int newLineIndex = smsBuffer.indexOf('\n');
    if (newLineIndex != -1) {

      String line = smsBuffer.substring(0, newLineIndex + 1);
      smsBuffer = smsBuffer.substring(newLineIndex + 1);

      line.trim();

      // ---------- 1. Check for signal quality response ----------
      if (line.startsWith("+CSQ:")) {
        int val = line.substring(line.indexOf(":") + 1).toInt();
        noSignal = (val <= 5);
        return;  // done
      }

      // ---------- 2. Check for SMS header ----------
      if (line.startsWith("+CMT:")) {
        waitingForSMSBody = true; // next line is actual message text
        return;
      }

      // ---------- 3. The next line after +CMT: is the REAL SMS body ----------
      if (waitingForSMSBody) {
        waitingForSMSBody = false;

        Serial.println("SMS RECEIVED!");
        parseSMS(line);
        return;
      }
    }
  }

  // ----------- BLINK LED WHEN GSM HAS NO SIGNAL -----------
  if (noSignal) {
    if (millis() - blinkTimer >= 500) {
      blinkTimer = millis();
      blinkState = !blinkState;

      if (blinkState)
        setNeoPixelColor({255, 0, 0}, 80); // dim red blink
      else
        setNeoPixelColor({0, 0, 0}, 0);    // off
    }
  }

  // Request signal quality every 3 seconds
  static unsigned long signalCheckTimer = 0;
  if (millis() - signalCheckTimer >= 3000) {
    signalCheckTimer = millis();
    checkGSMSignal();
  }
}

// ----------------------------------------------------
// PARSE SMS COMMANDS (COLOR: & BRIGHTNESS:)
// ----------------------------------------------------
void parseSMS(String msg) {

  noSignal = false; // if SMS arrived → GSM has signal

  msg.replace("\r", "");
  msg.replace("\n", "");
  msg.trim();
  msg.toUpperCase();

  int brightness = currentBrightness;
  String colorName = "";

  // ----- BRIGHTNESS -----
  int bIndex = msg.indexOf("BRIGHTNESS:");
  if (bIndex >= 0) {
    int end = msg.indexOf(" ", bIndex);
    if (end == -1) end = msg.length();
    brightness = msg.substring(bIndex + 11, end).toInt();
    brightness = constrain(brightness, 0, 255);
  }

  // ----- COLOR -----
  int cIndex = msg.indexOf("COLOR:");
  if (cIndex >= 0) {
    int end = msg.indexOf(" ", cIndex);
    if (end == -1) end = msg.length();
    colorName = msg.substring(cIndex + 6, end);
    colorName.trim();
  }

  // Save brightness
  currentBrightness = brightness;

  // If a new color was specified, update it
  if (colorName.length() > 0) {
    lastColor = getColor(colorName);
  }

  // Update the LEDs
  setNeoPixelColor(lastColor, currentBrightness);

  Serial.print("APPLIED → Color: ");
  Serial.print(colorName.length() > 0 ? colorName : "previous color");
  Serial.print(" | Brightness: ");
  Serial.println(currentBrightness);
}
