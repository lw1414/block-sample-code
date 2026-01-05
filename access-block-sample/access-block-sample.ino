#include <SPI.h>
#include <MFRC522.h>

// ==================== PIN DEFINITIONS ====================
#define SS_PIN     5     // SDA / SS
#define RST_PIN    27
#define BUZZER_PIN 13

// ==================== RFID OBJECT ====================
MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(115200);

  // Initialize buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize SPI with ESP32 pins
  SPI.begin(18, 19, 23, SS_PIN);

  // Initialize RFID reader
  mfrc522.PCD_Init();
  Serial.println("RFID RC522 Ready");
  Serial.println("Tap an RFID card...");
}

void loop() {
  // Look for new card
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Read card UID
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Beep buzzer (tick)
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);

  // Print UID
  Serial.print("Card UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Halt card
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
