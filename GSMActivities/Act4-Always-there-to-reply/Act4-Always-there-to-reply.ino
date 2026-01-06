#include <HardwareSerial.h>       // Include library for extra serial ports (we need this for SIM800C GSM module)
#include <Adafruit_NeoPixel.h>    // Include library to control NeoPixel RGB LEDs
//Message formate RGB STATUS
// ======================================================
// RGB COLOR STRUCTURE
// ======================================================
// This struct stores a color as three numbers: red, green, blue (0-255)
struct RGB { 
  uint8_t r, g, b; 
};

// ======================================================
// GSM CONFIGURATION (SIM800C)
// ======================================================
// Define pins connected to the GSM module
#define GSM_RX 17    // ESP32 RX -> GSM TX
#define GSM_TX 16    // ESP32 TX -> GSM RX
#define PWRKEY 4    // Pin to power on/off SIM800C

HardwareSerial gsm(1);  // Use UART1 for SIM800C communication

// ======================================================
// RGB NEOPIXEL CONFIGURATION
// ======================================================
#define LED_PIN 14      // Pin where NeoPixel strip is connected
#define NUM_LEDS 10     // Number of LEDs on the strip
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800); // Initialize strip

// ======================================================
// ANALOG INPUTS
// ======================================================
#define SLIDER_PIN 33     // Analog pin for slider (brightness control)
#define ROTARY_PIN 32     // Analog pin for rotary knob (to select color)

// ======================================================
// GLOBAL VARIABLES
// ======================================================
String smsBuffer = "";    // Buffer to store incoming SMS characters
RGB lastColor = {0, 255, 0}; // Default LED color is GREEN
int currentBrightness = 150;  // Default brightness

unsigned long blinkTimer = 0;  // Timer for blinking LED when no GSM signal
bool blinkState = false;       // Track blink on/off
bool noSignal = false;         // Flag when GSM has no signal

String replyNumber = "+639XXXXXXXXX"; // Placeholder number to reply to
bool replyingSMS = false;             // Priority flag to prevent conflicts while sending SMS

// ======================================================
// FUNCTION: Convert text to RGB color
// ======================================================
RGB getColor(String color) {
  color.trim();          // Remove spaces at the start/end
  color.toUpperCase();   // Make all letters uppercase for consistency

  // Compare input string and return corresponding RGB
  if (color == "RED")     return {255, 0, 0};
  if (color == "GREEN")   return {0, 255, 0};
  if (color == "BLUE")    return {0, 0, 255};
  if (color == "YELLOW")  return {255, 255, 0};
  if (color == "CYAN")    return {0, 255, 255};
  if (color == "MAGENTA") return {255, 0, 255};
  if (color == "WHITE")   return {255, 255, 255};
  if (color == "OFF")     return {0, 0, 0};

  return {0, 255, 0}; // Default GREEN if input is unknown
}

// ======================================================
// FUNCTION: Apply color + brightness to NeoPixel
// ======================================================
void setNeoPixelColor(RGB col, int brightness = 255) {
  brightness = constrain(brightness, 0, 255); // Limit brightness between 0 and 255

  // Calculate adjusted color according to brightness
  uint8_t r = (col.r * brightness) / 255;
  uint8_t g = (col.g * brightness) / 255;
  uint8_t b = (col.b * brightness) / 255;

  // Apply color to every LED on the strip
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show(); // Update the LEDs
}

// ======================================================
// FUNCTION: Read brightness from slider
// ======================================================
int getSliderBrightness() {
  int raw = analogRead(SLIDER_PIN);  // Read analog value (0-4095)
  return map(raw, 0, 4095, 0, 255);  // Map it to 0-255 for brightness
}

// ======================================================
// FUNCTION: Read rotary analog and map to color
// ======================================================
String getRotaryColor() {
  int raw = analogRead(ROTARY_PIN);  // Read rotary analog value

  // Map value ranges to 7 colors
  if (raw >= 2600 && raw < 2720) return "RED";
  else if (raw >= 2720 && raw < 2880) return "GREEN";
  else if (raw >= 2880 && raw < 3050) return "BLUE";
  else if (raw >= 3050 && raw < 3250) return "YELLOW";
  else if (raw >= 3250 && raw < 3450) return "MAGENTA";
  else if (raw >= 3450 && raw < 3600) return "CYAN";
  else return "WHITE";  // Highest position
}

// ======================================================
// FAST NON-BLOCKING SMS SEND
// ======================================================
void sendStatusSMS(String number) {
  replyingSMS = true; // Set priority so main loop doesn't interfere

  // Prepare message text
  String msg = "BRIGHTNESS:" + String(currentBrightness) + " COLOR:" + getRotaryColor();

  // Start SMS command
  gsm.print("AT+CMGS=\"");
  gsm.print(number);
  gsm.println("\"");

  // Wait for '>' prompt from SIM800C (non-blocking)
  unsigned long startWait = millis();
  while (!gsm.available() || gsm.read() != '>') {
    if (millis() - startWait > 3000) { // Timeout after 3 seconds
      Serial.println("Error: No > prompt received");
      replyingSMS = false;
      return;
    }
  }

  // Send message and CTRL+Z to finish
  gsm.print(msg);
  gsm.write(26); // CTRL+Z ASCII code
  Serial.println("RGB STATUS sent to: " + number);

  replyingSMS = false; // Clear priority flag
}

// ======================================================
// FUNCTION: Parse incoming SMS body
// ======================================================
void parseSMS(String msg, String sender) {
  msg.replace("\r", "");  // Remove carriage return
  msg.replace("\n", "");  // Remove newline
  msg.trim();             // Remove extra spaces
  msg.toUpperCase();      // Standardize text

  // Instant reply if message contains "RGB STATUS"
  if (msg.indexOf("RGB STATUS") != -1) {
    sendStatusSMS(sender);
  }

  // Handle brightness command
  int bIndex = msg.indexOf("BRIGHTNESS:");
  if (bIndex >= 0) {
    int end = msg.indexOf(" ", bIndex);
    if (end == -1) end = msg.length();
    currentBrightness = constrain(msg.substring(bIndex + 11, end).toInt(), 0, 255);
  }

  // Handle color command
  int cIndex = msg.indexOf("COLOR:");
  if (cIndex >= 0) {
    int end = msg.indexOf(" ", cIndex);
    if (end == -1) end = msg.length();
    lastColor = getColor(msg.substring(cIndex + 6, end));
  }

  // Apply changes to LED
  setNeoPixelColor(lastColor, currentBrightness);
  Serial.print("APPLIED → Color: "); Serial.print(getRotaryColor());
  Serial.print(" | Brightness: "); Serial.println(currentBrightness);
}

// ======================================================
// FUNCTION: Check GSM signal quality
// ======================================================
void checkGSMSignal() {
  gsm.println("AT+CSQ"); // Ask SIM800C for signal strength
}

// ======================================================
// SETUP
// ======================================================
void setup() {
  Serial.begin(115200);              // Start serial monitor
  gsm.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);  // Start GSM UART
  pinMode(PWRKEY, OUTPUT);           // Power key pin

  // Power on SIM800C
  digitalWrite(PWRKEY, HIGH); delay(1000);
  digitalWrite(PWRKEY, LOW);  delay(5000);

  // Set SIM800C SMS text mode
  gsm.println("AT+CMGF=1");

  // Auto push incoming SMS immediately
  gsm.println("AT+CNMI=2,2,0,0,0");

  // Initialize LED strip
  strip.begin();
  setNeoPixelColor(lastColor, currentBrightness);

  Serial.println("System ready."); // Notify user
}

// ======================================================
// LOOP
// ======================================================
void loop() {
  unsigned long currentMillis = millis(); // Track time

  // -------------------------
  // GSM handling (auto push)
  // -------------------------
  while (gsm.available()) {
    char c = gsm.read();    // Read character from GSM
    smsBuffer += c;         // Add to buffer
  }

  int newLineIndex = smsBuffer.indexOf('\n');
  while (newLineIndex != -1) {
    String line = smsBuffer.substring(0, newLineIndex + 1);
    smsBuffer = smsBuffer.substring(newLineIndex + 1);
    line.trim();

    // ---------- Only process SMS lines ----------
    if (line.startsWith("+CMT:")) {
      // Extract sender number
      int firstQuote = line.indexOf("\"") + 1;
      int secondQuote = line.indexOf("\"", firstQuote);
      if (firstQuote > 0 && secondQuote > firstQuote)
        replyNumber = line.substring(firstQuote, secondQuote);
      else
        replyNumber = "+639XXXXXXXXX";
    }
    else if (line.length() > 0 && !replyingSMS) {
      // Ignore responses like OK, AT, +CSQ
      if (!line.startsWith("AT") && !line.startsWith("+CSQ") && line != "OK") {
        parseSMS(line, replyNumber); // Process incoming SMS
        Serial.println("SMS BODY: " + line);
      }
    }

    newLineIndex = smsBuffer.indexOf('\n'); // check next line
  }

  // -------------------------
  // Update RGB from slider & rotary
  // -------------------------
  if (!replyingSMS) { 
    lastColor = getColor(getRotaryColor());
    currentBrightness = getSliderBrightness();
    setNeoPixelColor(lastColor, currentBrightness);
  }

  // -------------------------
  // GSM signal check every 3 seconds
  // -------------------------
  static unsigned long signalCheckTimer = 0;
  if (millis() - signalCheckTimer >= 3000 && !replyingSMS) {
    signalCheckTimer = millis();
    checkGSMSignal();
  }

  // -------------------------
  // Blink LED if no GSM signal
  // -------------------------
  if (noSignal && millis() - blinkTimer >= 500) {
    blinkTimer = millis();
    blinkState = !blinkState;
    if (blinkState) setNeoPixelColor({255, 0, 0}, 80); // Red blink
    else setNeoPixelColor({0, 0, 0}, 0);              // LED off
  }
}
