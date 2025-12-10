#include <SPI.h>
#define SS_PIN 5
#define RST_PIN 4

byte readReg(byte reg) {
  digitalWrite(SS_PIN, LOW);
  SPI.transfer(((reg << 1) & 0x7E) | 0x80);
  byte v = SPI.transfer(0x00);
  digitalWrite(SS_PIN, HIGH);
  return v;
}
void writeReg(byte reg, byte val) {
  digitalWrite(SS_PIN, LOW);
  SPI.transfer((reg << 1) & 0x7E);
  SPI.transfer(val);
  digitalWrite(SS_PIN, HIGH);
}

void hardReset() {
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW);
  delay(20);
  digitalWrite(RST_PIN, HIGH);
  delay(80);
}

void setup() {
  Serial.begin(115200);
  pinMode(SS_PIN, OUTPUT); digitalWrite(SS_PIN, HIGH);
  hardReset();
  SPI.begin(18, 19, 23);
  delay(10);
  Serial.println("\n=== RF RECOVERY LOOP START ===");
}

void loop() {
  // Print current state
  Serial.print("Ver=0x"); Serial.print(readReg(0x37), HEX);
  Serial.print("  Cmd=0x"); Serial.print(readReg(0x01), HEX);
  Serial.print("  Tx=0x"); Serial.print(readReg(0x14), HEX);
  Serial.print("  RFCfg=0x"); Serial.print(readReg(0x26), HEX);
  Serial.print("  RF(3B,3C)=0x"); Serial.print(readReg(0x3B), HEX);
  Serial.print("  0x"); Serial.println(readReg(0x3C), HEX);

  // Attempt to clear CommandReg -> Idle
  Serial.println("Attempting to write CommandReg = 0x00 (Idle) ...");
  writeReg(0x01, 0x00);
  delay(20);

  // Read back immediately and then after a small delay
  Serial.print("Readback Cmd now = 0x"); Serial.println(readReg(0x01), HEX);
  delay(50);

  // If it stuck to Idle, attempt RF writes
  if ((readReg(0x01) & 0x3F) == 0x00) {
    Serial.println("CommandReg is Idle — trying RFCfg/TxCtrl writes...");
    writeReg(0x26, 0x7F); delay(10);
    writeReg(0x14, 0x83); delay(10);

    Serial.print("After write: Tx=0x"); Serial.print(readReg(0x14), HEX);
    Serial.print("  RFCfg=0x"); Serial.println(readReg(0x26), HEX);
    Serial.print("RF(3B,3C)=0x"); Serial.print(readReg(0x3B), HEX);
    Serial.print("  0x"); Serial.println(readReg(0x3C), HEX);

    // If success show and stop loop
    if ((readReg(0x14) & 0x03) != 0) {
      Serial.println("SUCCESS: Tx drivers enabled.");
      while(true) delay(1000);
    } else {
      Serial.println("Writes did not stick — continuing attempts...");
    }
  } else {
    Serial.println("CommandReg still not Idle (writes ignored).");
  }

  // Try toggling RST once more if we can't make progress
  Serial.println("Toggling RST and retrying...");
  hardReset();

  delay(700);
}
