#define RELAY1 14  // Relay 1 on IO14
#define RELAY2 25  // Relay 2 on IO25

void setup() {
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);

  // Optional: start with both relays OFF
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  delay(3000);  // wait 3 seconds

  Serial.begin(115200);
  Serial.println("Relay toggling started...");
}

void loop() {
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  // Turn ON both relays
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  Serial.println("Relays ON");
  delay(3000);  // wait 3 seconds

  // Turn OFF both relays
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  Serial.println("Relays OFF");
  delay(3000);  // wait 3 seconds
}
