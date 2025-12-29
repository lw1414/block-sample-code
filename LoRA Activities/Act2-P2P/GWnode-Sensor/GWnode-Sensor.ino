// ==================== LIBRARIES ====================

// SPI library for LoRa module communication
#include <SPI.h>

// Sandeep Mistry LoRa library
#include <LoRa.h>

// NeoPixel RGB LED library
#include <Adafruit_NeoPixel.h>


// ==================== LORA PIN CONFIG ====================

// SPI clock pin
#define LORA_SCK   18

// SPI MISO pin
#define LORA_MISO  19

// SPI MOSI pin
#define LORA_MOSI  23

// LoRa chip select (NSS)
#define LORA_SS    5

// LoRa reset pin
#define LORA_RST   4

// LoRa DIO0 (IRQ pin for RX/TX done)
#define LORA_DIO0  26

// LoRa frequency (must match sender)
#define LORA_BAND  433E6


// ==================== RGB & BUZZER CONFIG ====================

// Buzzer output pin
#define BUZZER_PIN 13

// NeoPixel data pin
#define LED_PIN    14

// Number of LEDs in the strip
#define NUM_LEDS   10

// Create NeoPixel object
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);


// ==================== THRESHOLD DEFINITIONS ====================

// Maximum temperature considered "cool"
#define TEMP_COOL_MAX     25.0

// Maximum temperature before temperature alarm
#define TEMP_NORMAL_MAX   40.0

// Humidity alarm threshold (%)
#define HUM_ALARM_LIMIT   80.0

// Light alarm threshold (lux)
#define LUX_ALARM_LIMIT  1000.0


// ==================== SETUP FUNCTION ====================
void setup() {

  // Initialize Serial Monitor
  Serial.begin(115200);

  // Configure buzzer pin as output
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize NeoPixel strip
  strip.begin();

  // Turn all LEDs OFF initially
  strip.show();

  // Initialize SPI bus with custom pins
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);

  // Set LoRa control pins
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // Start LoRa radio
  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("❌ LoRa init failed!");
    while (1); // Stop if LoRa fails
  }

  // IMPORTANT: Must match sender radio settings
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12); // Private network

  // Indicate receiver is ready
  Serial.println("✅ LoRa Receiver Ready");
}


// ==================== MAIN LOOP ====================
void loop() {

  // Check if a LoRa packet has arrived
  int packetSize = LoRa.parsePacket();

  // If packet received
  if (packetSize) {

    // String to store incoming message
    String incoming = "";

    // Read all received bytes
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    // Print raw received message
    Serial.println("📥 Received: " + incoming);

    // ==================== PARSE DATA ====================
    // Expected format: temp|hum|lux

    // Extract temperature
    float temp = incoming.substring(0, incoming.indexOf('|')).toFloat();

    // Remove parsed part
    incoming.remove(0, incoming.indexOf('|') + 1);

    // Extract humidity
    float hum = incoming.substring(0, incoming.indexOf('|')).toFloat();

    // Remove parsed part
    incoming.remove(0, incoming.indexOf('|') + 1);

    // Extract light value
    float lux = incoming.toFloat();

    // Display parsed sensor values
    Serial.printf("🌡 %.1f °C | 💧 %.1f %% | 💡 %.1f lux\n", temp, hum, lux);


    // ==================== DECISION LOGIC ====================

    // --- TEMPERATURE ALARM (Highest Priority) ---
    if (temp > TEMP_NORMAL_MAX) {
      alarmState(255, 0, 0);   // RED alarm for temperature
    }

    // --- HUMIDITY ALARM ---
    else if (hum > HUM_ALARM_LIMIT) {
      alarmState(128, 0, 128); // PURPLE alarm for humidity
    }

    // --- LIGHT ALARM ---
    else if (lux > LUX_ALARM_LIMIT) {
      alarmState(0, 0, 255);   // BLUE alarm for light
    }

    // --- NORMAL STATUS INDICATION ---
    else {

      // COOL temperature
      if (temp < TEMP_COOL_MAX) {
        setColor(0, 0, 255);   // BLUE
      }

      // NORMAL temperature
      else {
        setColor(0, 255, 0);   // GREEN
      }
    }
  }
}


// ==================== LED COLOR FUNCTION ====================

// Set all LEDs to one color
void setColor(uint8_t r, uint8_t g, uint8_t b) {

  // Loop through all LEDs
  for (int i = 0; i < NUM_LEDS; i++) {

    // Set LED color
    strip.setPixelColor(i, strip.Color(r, g, b));
  }

  // Update LEDs
  strip.show();
}


// ==================== ALARM FUNCTION ====================

// Alarm with color + buzzer
void alarmState(uint8_t r, uint8_t g, uint8_t b) {

  // Print alarm message
  Serial.println("🚨 ALARM TRIGGERED!");

  // Flash alarm 3 times
  for (int i = 0; i < 3; i++) {

    // Turn LEDs ON with alarm color
    setColor(r, g, b);

    // Play buzzer tone
    tone(BUZZER_PIN, 1000, 200);

    // Hold ON state
    delay(300);

    // Turn LEDs OFF
    setColor(0, 0, 0);

    // Pause between flashes
    delay(200);
  }
}
