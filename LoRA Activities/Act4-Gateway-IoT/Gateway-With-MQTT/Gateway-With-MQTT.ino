#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>

/* =====================================================
   GATEWAY SECURITY
   ===================================================== */
#define APP_KEY "APPKEY123"

/* =====================================================
   LORA CONFIG (RA-02)
   ===================================================== */
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   4
#define LORA_DIO0  26
#define LORA_BAND  433E6

/* =====================================================
   WIFI CONFIG
   ===================================================== */
const char* ssid     = "PLDTinnov";
const char* password = "Password12345!";

/* =====================================================
   MQTT CONFIG
   ===================================================== */
const char* mqtt_server = "innovph.com";
const int   mqtt_port   = 1883;
const char* mqtt_user   = "mqtt";
const char* mqtt_pass   = "ICPHmqtt!";

/* =====================================================
   MQTT CLIENT
   ===================================================== */
WiFiClient espClient;
PubSubClient mqtt(espClient);

/* =====================================================
   FUNCTION PROTOTYPES
   ===================================================== */
void receiveLoRa();
void handleJoin(String msg);
void handleData(String msg);
void connectWiFi();
void connectMQTT();

/* =====================================================
   SETUP
   ===================================================== */
void setup() {
  Serial.begin(115200);

  // --- WiFi ---
  connectWiFi();

  // --- MQTT ---
  mqtt.setServer(mqtt_server, mqtt_port);

  // --- LoRa ---
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("❌ LoRa init failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setTxPower(17);

  Serial.println("🟢 LoRa Gateway Started");
}

/* =====================================================
   LOOP
   ===================================================== */
void loop() {

  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();

  receiveLoRa();
}

/* =====================================================
   WIFI CONNECT
   ===================================================== */
void connectWiFi() {
  Serial.print("Connecting WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" OK");
}

/* =====================================================
   MQTT CONNECT
   ===================================================== */
void connectMQTT() {
  Serial.print("Connecting MQTT...");
  while (!mqtt.connected()) {
    if (mqtt.connect("LoRaGateway", mqtt_user, mqtt_pass)) {
      Serial.println(" OK");
    } else {
      Serial.print(" FAILED rc=");
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

/* =====================================================
   RECEIVE LORA
   ===================================================== */
void receiveLoRa() {

  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  Serial.print("📩 RX: ");
  Serial.println(msg);

  if (msg.startsWith("JOIN_REQ")) {
    handleJoin(msg);
  }

  if (msg.startsWith("DATA")) {
    handleData(msg);
  }
}

/* =====================================================
   HANDLE JOIN
   ===================================================== */
void handleJoin(String msg) {

  int p1 = msg.indexOf('|');
  int p2 = msg.indexOf('|', p1 + 1);

  String devEUI = msg.substring(p1 + 1, p2);
  String key    = msg.substring(p2 + 1);

  if (key != APP_KEY) {
    Serial.println("❌ Invalid APP KEY");
    return;
  }

  LoRa.beginPacket();
  LoRa.print("JOIN_ACCEPT|A1B2|SESS123");
  LoRa.endPacket();

  Serial.println("✅ JOIN ACCEPT SENT");
}

/* =====================================================
   HANDLE DATA + MQTT PUBLISH
   ===================================================== */
void handleData(String msg) {

  int p1 = msg.indexOf('|');
  int p2 = msg.indexOf('|', p1 + 1);
  int p3 = msg.indexOf('|', p2 + 1);
  int p4 = msg.indexOf('|', p3 + 1);

  String devAddr = msg.substring(p1 + 1, p2);
  String seq     = msg.substring(p2 + 1, p3);
  String payload = msg.substring(p3 + 1, p4);

  Serial.printf("📦 From %s → %s\n", devAddr.c_str(), payload.c_str());

  /* ---- Parse Payload ----
     Expected: T=31.4,H=62.8,L=450.2
  */
  float temp = 0, hum = 0, lux = 0;

  sscanf(payload.c_str(), "T=%f,H=%f,L=%f", &temp, &hum, &lux);

  char buf[16];

  dtostrf(temp, 4, 1, buf);
  mqtt.publish(("LoRa/act4/" + devAddr + "/temperature").c_str(), buf);

  dtostrf(hum, 4, 1, buf);
  mqtt.publish(("LoRa/act4/" + devAddr + "/humidity").c_str(), buf);

  dtostrf(lux, 5, 1, buf);
  mqtt.publish(("LoRa/act4/" + devAddr + "/light").c_str(), buf);

  Serial.println("📤 MQTT Published");

  /* ---- ACK ---- */
  LoRa.beginPacket();
  LoRa.print("ACK|");
  LoRa.print(devAddr);
  LoRa.print("|");
  LoRa.print(seq);
  LoRa.endPacket();
}
