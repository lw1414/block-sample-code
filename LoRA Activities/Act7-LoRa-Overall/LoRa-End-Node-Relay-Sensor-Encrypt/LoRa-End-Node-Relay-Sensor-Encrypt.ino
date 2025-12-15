#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <AESLib.h>

/* ==============================
   LORA CONFIG
   ============================== */
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   4
#define LORA_DIO0  26
#define LORA_BAND  433E6

/* ==============================
   NODE CONFIG
   ============================== */
String nodeAddr = "NODE_001";

// Relays
#define RELAY1_PIN 14
#define RELAY2_PIN 25
bool relay1State = true;  // HIGH = OFF
bool relay2State = true;  // HIGH = OFF

// Sensors
#define MQ136_PIN 32

// Send interval (ms)
unsigned long SEND_INTERVAL = 5000;
unsigned long lastSend = 0;
unsigned long mqttInterval = 5000;  // store interval received via MQTT/LoRa

/* ==============================
   SENSOR THRESHOLDS
   ============================== */
#define TEMP_THRESHOLD 35.0
#define HUM_THRESHOLD  70.0
#define LUX_THRESHOLD  800.0
#define MQ136_THRESHOLD 200.0

/* ==============================
   AES CONFIG
   ============================== */
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

/* ==============================
   SENSOR READ FUNCTIONS
   ============================== */
float readMQ136PPM() {
  int raw = analogRead(MQ136_PIN);
  return ((float)raw / 4095.0) * 1000.0;
}

float readTemperature() {
  float t, h;
  if(readAHT20(&t, &h)) return t;
  return 25.0;
}

float readHumidity() {
  float t, h;
  if(readAHT20(&t, &h)) return h;
  return 50.0;
}

float readLux() {
  float lux = readBH1750();
  if(lux >= 0) return lux;
  return 400.0;
}

/* ==============================
   AHT20 + BH1750 LOW LEVEL
   ============================== */
#define AHT20_ADDR 0x38
#define BH1750_ADDR 0x5C

bool readAHT20(float *tempC, float *humidity) {
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xAC); Wire.write(0x33); Wire.write(0x00);
  Wire.endTransmission();
  delay(80);

  Wire.requestFrom(AHT20_ADDR, 6);
  if(Wire.available() != 6) return false;
  uint8_t data[6]; for(int i=0;i<6;i++) data[i] = Wire.read();

  uint32_t hum_raw = ((uint32_t)data[1]<<12) | ((uint32_t)data[2]<<4) | ((uint32_t)(data[3]>>4));
  uint32_t temp_raw = (((uint32_t)data[3]&0x0F)<<16) | ((uint32_t)data[4]<<8) | ((uint32_t)data[5]);

  *humidity = ((float)hum_raw / 1048576.0) * 100.0;
  *tempC   = ((float)temp_raw / 1048576.0) * 200.0 - 50.0;
  return true;
}

float readBH1750() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10); // continuous H-res mode
  Wire.endTransmission();
  delay(180);
  Wire.requestFrom(BH1750_ADDR, 2);
  if(Wire.available()==2) {
    uint16_t raw = Wire.read()<<8 | Wire.read();
    return raw/1.2;
  }
  return -1;
}

/* ==============================
   AES ENCRYPT FUNCTION
   ============================== */
String encryptPayload(String payload) {
  strncpy(encBuffer, payload.c_str(), sizeof(encBuffer));
  encBuffer[sizeof(encBuffer)-1]='\0';

  char cipher[128];
  aes.encrypt64((byte*)encBuffer, strlen(encBuffer), cipher, aes_key, 128, aes_iv);
  return String(cipher);
}

/* ==============================
   SETUP
   ============================== */
void setup() {
  Serial.begin(115200);
  while(!Serial);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, relay1State?HIGH:LOW);
  digitalWrite(RELAY2_PIN, relay2State?HIGH:LOW);

  SPI.begin(LORA_SCK,LORA_MISO,LORA_MOSI,LORA_SS);
  LoRa.setPins(LORA_SS,LORA_RST,LORA_DIO0);
  if(!LoRa.begin(LORA_BAND)) { Serial.println("❌ LoRa init failed"); while(1); }
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setTxPower(17);

  Wire.begin(21,22);

  Serial.println("✅ End Node Started");
}

/* ==============================
   LOOP
   ============================== */
void loop() {
  receiveLoRa(); // Relay2 and interval commands

  unsigned long now = millis();
  if(now - lastSend >= SEND_INTERVAL) {
    lastSend = now;
    sendSensorData();
  }
}

/* ==============================
   SEND SENSOR DATA
   ============================== */
void sendSensorData() {
  float t = readTemperature();
  float h = readHumidity();
  float l = readLux();
  float mq = readMQ136PPM();

  // Relay1 ONLY threshold-controlled
  if(t>=TEMP_THRESHOLD || h>=HUM_THRESHOLD || l>=LUX_THRESHOLD || mq>=MQ136_THRESHOLD) {
    relay1State = false; // ON
    digitalWrite(RELAY1_PIN, LOW);
  } else {
    relay1State = true; // OFF
    digitalWrite(RELAY1_PIN, HIGH);
  }

  String payload = String("T=")+t+",H="+h+",L="+l+",MQ136="+mq;
  String encPayload = encryptPayload(payload);

  // Send LoRa packet
  LoRa.beginPacket();
  LoRa.print("DATA|");
  LoRa.print(nodeAddr);
  LoRa.print("|");
  LoRa.print(encPayload);
  LoRa.endPacket();

  // Serial only prints encrypted payload
  Serial.printf("[%lu ms] Encrypted -> %s\n", millis(), encPayload.c_str());
}


/* ==============================
   RECEIVE LORA COMMANDS
   ============================== */
void receiveLoRa() {
  int packetSize = LoRa.parsePacket();
  if(!packetSize) return;

  String msg="";
  while(LoRa.available()) msg += (char)LoRa.read();
  Serial.printf("[%lu ms] LoRa RX -> %s\n", millis(), msg.c_str());

  if(!msg.startsWith("CMD|")) return;

  int idx1 = msg.indexOf('|');
  int idx2 = msg.indexOf('|',idx1+1);
  int idx3 = msg.indexOf('|',idx2+1);

  String targetNode = msg.substring(idx1+1,idx2);
  if(targetNode!=nodeAddr) return;

  String relayCmd = msg.substring(idx2+1,idx3);
  String action   = msg.substring(idx3+1);

  // Relay1 excluded, only Relay2 and INTERVAL handled
  if(relayCmd=="RELAY2") {
    relay2State = (action=="OFF");
    digitalWrite(RELAY2_PIN, relay2State?HIGH:LOW);
    Serial.printf("[%lu ms] RELAY2 -> %s\n", millis(), relay2State?"OFF":"ON");
  } 
  else if(relayCmd=="INTERVAL") {
    mqttInterval = action.toInt();    // store MQTT interval
    SEND_INTERVAL = mqttInterval;     // apply immediately
    lastSend = millis();              // reset timer
    Serial.printf("[%lu ms] MQTT Interval -> %lu ms\n", millis(), mqttInterval);
  }

  // ACK back
  String ackMsg = "ACK|" + nodeAddr + "|" + relayCmd + "|" + action;
  LoRa.beginPacket();
  LoRa.print(ackMsg);
  LoRa.endPacket();
  Serial.printf("[%lu ms] LoRa TX -> %s\n", millis(), ackMsg.c_str());
}
