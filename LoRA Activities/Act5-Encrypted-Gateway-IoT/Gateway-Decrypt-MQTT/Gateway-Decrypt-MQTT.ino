#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <AESLib.h>
#include <Adafruit_NeoPixel.h>

/* =====================================================
   SECURITY
   ===================================================== */
#define APP_KEY "APPKEY1234"

/* =====================================================
   LORA CONFIG
   ===================================================== */
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   4
#define LORA_DIO0  26
#define LORA_BAND  433E6

/* =====================================================
   NEOPIXEL + BUZZER
   ===================================================== */
#define LED_PIN    14
#define NUM_LEDS   10
#define BUZZER_PIN 13

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

/* =====================================================
   THRESHOLDS
   ===================================================== */
#define TEMP_WARM     35
#define TEMP_DANGER   45

#define HUM_WARM      70
#define HUM_DANGER    85

#define LUX_WARM      800
#define LUX_DANGER    1500

/* =====================================================
   WIFI / MQTT
   ===================================================== */
const char* ssid = "PLDTinnov";
const char* password = "Password12345!";
const char* mqtt_server = "innovph.com";
const int   mqtt_port = 1883;
const char* mqtt_user = "mqtt";
const char* mqtt_pass = "ICPHmqtt!";

WiFiClient espClient;
PubSubClient mqtt(espClient);

/* =====================================================
   AES CONFIG
   ===================================================== */
AESLib aes;

byte aes_key[16] = {
  0x11,0x22,0x33,0x44,
  0x55,0x66,0x77,0x88,
  0x99,0xAA,0xBB,0xCC,
  0xDD,0xEE,0xFF,0x00
};

byte aes_iv[16] = {
  0x00,0x01,0x02,0x03,
  0x04,0x05,0x06,0x07,
  0x08,0x09,0x0A,0x0B,
  0x0C,0x0D,0x0E,0x0F
};

char encBuffer[128];
byte decBuffer[128];
char decrypted[128];

/* =====================================================
   ALARM STATE
   ===================================================== */
bool alarmActive = false;
unsigned long lastSirenToggle = 0;
bool sirenHigh = false;

/* =====================================================
   HELPERS
   ===================================================== */
void setStripColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

/* ---- NON-BLOCKING POLICE SIREN ---- */
void updateSiren() {
  if (!alarmActive) {
    noTone(BUZZER_PIN);
    return;
  }

  unsigned long now = millis();
  if (now - lastSirenToggle >= 180) {
    lastSirenToggle = now;
    sirenHigh = !sirenHigh;
    tone(BUZZER_PIN, sirenHigh ? 900 : 1200);
  }
}

bool handleThresholds(float t, float h, float l) {

  bool danger = false;

  // Temperature
  if (t >= TEMP_DANGER) {
    setStripColor(255, 0, 0);
    danger = true;
  } else if (t >= TEMP_WARM) {
    setStripColor(255, 140, 0);
  } else {
    setStripColor(0, 0, 255);
  }

  // Humidity
  if (h >= HUM_DANGER) {
    setStripColor(255, 0, 0);
    danger = true;
  } else if (h >= HUM_WARM) {
    setStripColor(255, 255, 0);
  }

  // Lux
  if (l >= LUX_DANGER) {
    setStripColor(255, 0, 0);
    danger = true;
  } else if (l >= LUX_WARM) {
    setStripColor(255, 255, 255);
  }

  return danger;
}

/* =====================================================
   AES DECRYPT
   ===================================================== */
void decryptMessage(const char* msg, char* out) {

  strncpy(encBuffer, msg, sizeof(encBuffer));
  encBuffer[sizeof(encBuffer) - 1] = '\0';

  uint16_t len = aes.decrypt64(
    encBuffer,
    strlen(encBuffer),
    decBuffer,
    aes_key,
    128,
    aes_iv
  );

  memcpy(out, decBuffer, len);
  out[len] = '\0';
}

/* =====================================================
   SETUP
   ===================================================== */
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  strip.begin();
  strip.setBrightness(80);
  strip.show();

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" OK");

  mqtt.setServer(mqtt_server, mqtt_port);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("❌ LoRa init failed");
    while (1);
  }

  Serial.println("🟢 LoRa Gateway Ready");
  setStripColor(0, 0, 255);
}

/* =====================================================
   LOOP
   ===================================================== */
void loop() {

  if (!mqtt.connected()) {
    mqtt.connect("LoRaGateway", mqtt_user, mqtt_pass);
  }
  mqtt.loop();

  receiveLoRa();
  updateSiren();   // <-- keeps alarm running
}

/* =====================================================
   RECEIVE LORA
   ===================================================== */
void receiveLoRa() {

  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  Serial.print("📩 RX RAW: ");
  Serial.print(msg);
  Serial.print(" | RSSI: ");
  Serial.print(LoRa.packetRssi());
  Serial.print(" dBm | SNR: ");
  Serial.println(LoRa.packetSnr());

  if (msg.startsWith("JOIN_REQ")) handleJoin(msg);
  if (msg.startsWith("DATA")) handleData(msg);
}

/* =====================================================
   HANDLE JOIN
   ===================================================== */
void handleJoin(String msg) {

  int p2 = msg.lastIndexOf('|');
  if (msg.substring(p2 + 1) != APP_KEY) {
    Serial.println("❌ Invalid APP_KEY");
    return;
  }

  // 👉 MANUALLY SET DEV ADDRESS HERE
  String devAddr = "A1B14";

  LoRa.beginPacket();
  LoRa.print("JOIN_ACCEPT|");
  LoRa.print(devAddr);
  LoRa.print("|SESS123");
  LoRa.endPacket();

  Serial.print("🤝 JOIN accepted, devAddr = ");
  Serial.println(devAddr);
}


/* =====================================================
   HANDLE DATA
   ===================================================== */
void handleData(String msg) {

  int p1 = msg.indexOf('|');
  int p2 = msg.indexOf('|', p1 + 1);
  int p3 = msg.indexOf('|', p2 + 1);
  int p4 = msg.indexOf('|', p3 + 1);

  String devAddr = msg.substring(p1 + 1, p2);
  String seq     = msg.substring(p2 + 1, p3);
  String enc     = msg.substring(p3 + 1, p4);

  decryptMessage(enc.c_str(), decrypted);

  Serial.print("🔐 Decrypted: ");
  Serial.println(decrypted);

  float t, h, l;
  sscanf(decrypted, "T=%f,H=%f,L=%f", &t, &h, &l);

  Serial.printf(
    "📊 Temp=%.1f°C | Hum=%.1f%% | Lux=%.1f lx\n",
    t, h, l
  );

  alarmActive = handleThresholds(t, h, l);

  if (alarmActive) {
    Serial.println("🚨 ALARM ACTIVE");
  } else {
    Serial.println("✅ NORMAL");
    noTone(BUZZER_PIN);
  }

  char buf[16];

  dtostrf(t, 4, 1, buf);
  mqtt.publish(("LoRa/act5/" + devAddr + "/temperature").c_str(), buf);

  dtostrf(h, 4, 1, buf);
  mqtt.publish(("LoRa/act5/" + devAddr + "/humidity").c_str(), buf);

  dtostrf(l, 5, 1, buf);
  mqtt.publish(("LoRa/act5/" + devAddr + "/light").c_str(), buf);

  Serial.println("📤 MQTT published");

  LoRa.beginPacket();
  LoRa.print("ACK|");
  LoRa.print(devAddr);
  LoRa.print("|");
  LoRa.print(seq);
  LoRa.endPacket();

  Serial.println("✅ ACK sent\n");
}
