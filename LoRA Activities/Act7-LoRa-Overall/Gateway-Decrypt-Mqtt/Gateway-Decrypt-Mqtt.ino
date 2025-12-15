#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <AESLib.h>
#include <Adafruit_NeoPixel.h>

/* =====================================================
   SECURITY
   ===================================================== */
#define APP_KEY "APPKEY123"

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
#define MQ136_THRESHOLD 200.0

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
   NODE / RELAY CONFIG
   ===================================================== */
String nodeAddr = "NODE_001";
char button1Topic[64];
char button2Topic[64];
char intervalTopic[64];

/* =====================================================
   HELPERS
   ===================================================== */
void setStripColor(uint8_t r, uint8_t g, uint8_t b){
  for(int i=0;i<NUM_LEDS;i++) strip.setPixelColor(i, strip.Color(r,g,b));
  strip.show();
}

void updateSiren(){
  if(!alarmActive){ noTone(BUZZER_PIN); return; }
  unsigned long now = millis();
  if(now - lastSirenToggle >= 180){
    lastSirenToggle = now;
    sirenHigh = !sirenHigh;
    tone(BUZZER_PIN, sirenHigh?900:1200);
  }
}

bool handleThresholds(float t,float h,float l,float mq){
  bool danger=false;
  if(t>=TEMP_DANGER) { setStripColor(255,0,0); danger=true; } 
  else if(t>=TEMP_WARM) setStripColor(255,140,0); 
  else setStripColor(0,0,255);
  
  if(h>=HUM_DANGER) { setStripColor(255,0,0); danger=true; } 
  else if(h>=HUM_WARM) setStripColor(255,255,0);

  if(l>=LUX_DANGER) { setStripColor(255,0,0); danger=true; } 
  else if(l>=LUX_WARM) setStripColor(255,255,255);

  if(mq>=MQ136_THRESHOLD) { setStripColor(255,0,0); danger=true; }

  return danger;
}

/* =====================================================
   AES DECRYPT
   ===================================================== */
void decryptMessage(const char* msg,char* out){
  strncpy(encBuffer,msg,sizeof(encBuffer));
  encBuffer[sizeof(encBuffer)-1]='\0';
  uint16_t len = aes.decrypt64(encBuffer,strlen(encBuffer),decBuffer,aes_key,128,aes_iv);
  memcpy(out,decBuffer,len);
  out[len]='\0';
}

/* =====================================================
   SETUP
   ===================================================== */
void setup(){
  Serial.begin(115200);

  pinMode(BUZZER_PIN,OUTPUT);
  digitalWrite(BUZZER_PIN,LOW);

  strip.begin();
  strip.setBrightness(80);
  strip.show();

  WiFi.begin(ssid,password);
  Serial.print("Connecting WiFi");
  while(WiFi.status()!=WL_CONNECTED){delay(500);Serial.print(".");}
  Serial.println(" OK");

  mqtt.setServer(mqtt_server,mqtt_port);

  snprintf(button1Topic,sizeof(button1Topic),"LoRa/act7/%s/button1",nodeAddr.c_str());
  snprintf(button2Topic,sizeof(button2Topic),"LoRa/act7/%s/button2",nodeAddr.c_str());
  snprintf(intervalTopic,sizeof(intervalTopic),"LoRa/act7/%s/interval",nodeAddr.c_str());

  mqtt.setCallback(mqttCallback);

  SPI.begin(LORA_SCK,LORA_MISO,LORA_MOSI,LORA_SS);
  LoRa.setPins(LORA_SS,LORA_RST,LORA_DIO0);
  if(!LoRa.begin(LORA_BAND)){ Serial.println("❌ LoRa init failed"); while(1);}
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setTxPower(17);

  Serial.println("🟢 LoRa Gateway Ready");
  setStripColor(0,0,255);
}

/* =====================================================
   LOOP
   ===================================================== */
void loop(){
  if(!mqtt.connected()) connectMQTT();
  mqtt.loop();
  receiveLoRa();
  updateSiren();
}

/* =====================================================
   MQTT CALLBACK
   ===================================================== */
void mqttCallback(char* topic, byte* payload, unsigned int length){
  payload[length]='\0';
  String msg=String((char*)payload); msg.trim();
  Serial.printf("[%lu ms] MQTT RX -> %s | %s\n", millis(), topic, msg.c_str());

  String cmd="CMD|"+nodeAddr+"|";

  if(String(topic)==button1Topic) cmd+="RELAY1|"+msg;
  else if(String(topic)==button2Topic) cmd+="RELAY2|"+msg;
  else if(String(topic)==intervalTopic) cmd+="INTERVAL|"+msg;
  else return;

  LoRa.beginPacket();
  LoRa.print(cmd);
  LoRa.endPacket();
  Serial.printf("[%lu ms] LoRa TX -> %s\n", millis(), cmd.c_str());
}

/* =====================================================
   CONNECT MQTT
   ===================================================== */
void connectMQTT(){
  if(mqtt.connect(nodeAddr.c_str(),mqtt_user,mqtt_pass)){
    Serial.println("✅ MQTT Connected");
    mqtt.subscribe(button1Topic);
    mqtt.subscribe(button2Topic);
    mqtt.subscribe(intervalTopic);
  }else{
    Serial.printf("❌ MQTT Connect failed, rc=%d\n",mqtt.state());
  }
}

/* =====================================================
   RECEIVE LORA
   ===================================================== */
void receiveLoRa(){
  int packetSize=LoRa.parsePacket();
  if(!packetSize) return;
  String msg="";
  while(LoRa.available()) msg += (char)LoRa.read();
  Serial.printf("[%lu ms] LoRa RX -> %s | RSSI: %d dBm\n", millis(), msg.c_str(), LoRa.packetRssi());

  if(msg.startsWith("DATA")) handleData(msg);
}

/* =====================================================
   HANDLE DATA
   ===================================================== */
void handleData(String msg){
  int p1=msg.indexOf('|');
  int p2=msg.indexOf('|',p1+1);

  String devAddr = msg.substring(p1+1,p2);
  String enc     = msg.substring(p2+1);

  decryptMessage(enc.c_str(),decrypted);
  Serial.printf("🔐 Decrypted: %s\n", decrypted);

  float t,h,l,mq;
  sscanf(decrypted,"T=%f,H=%f,L=%f,MQ136=%f",&t,&h,&l,&mq);
  Serial.printf("📊 Temp=%.1f°C | Hum=%.1f%% | Lux=%.1f lx | MQ136=%.1f ppm\n",t,h,l,mq);

  alarmActive = handleThresholds(t,h,l,mq);
  if(alarmActive) Serial.println("🚨 ALARM ACTIVE"); 
  else { Serial.println("✅ NORMAL"); noTone(BUZZER_PIN); }

  // Publish MQTT
  char buf[16];
  dtostrf(t,4,1,buf); mqtt.publish(("LoRa/act7/"+devAddr+"/temperature").c_str(),buf);
  dtostrf(h,4,1,buf); mqtt.publish(("LoRa/act7/"+devAddr+"/humidity").c_str(),buf);
  dtostrf(l,5,1,buf); mqtt.publish(("LoRa/act7/"+devAddr+"/light").c_str(),buf);
  dtostrf(mq,5,1,buf); mqtt.publish(("LoRa/act7/"+devAddr+"/mq136").c_str(),buf);

  // ACK back
  LoRa.beginPacket();
  LoRa.print("ACK|"); LoRa.print(devAddr); LoRa.print("|RECEIVED");
  LoRa.endPacket();
  Serial.println("✅ ACK sent\n");
}
