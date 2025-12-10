// === Joystick Pins ===
#define JOY_X_PIN 33      // swapped
#define JOY_Y_PIN 32      // swapped
#define JOY_SW_PIN 34     // floating, no pull-up

// === Buttons ===
#define BTN1_PIN 22       // has pull-up
#define BTN2_PIN 21       // has pull-up
#define BTN3_PIN 35       // floating, no pull-up
#define BTN4_PIN 25       // has pull-up

void setup() {
  Serial.begin(115200);

  // Buttons with real pullups
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);

  // Buttons without internal pull-up
  pinMode(BTN3_PIN, INPUT);  
  pinMode(JOY_SW_PIN, INPUT);
}

// --- Function to simulate pull-up for pins without pull-up ---
bool softwarePullupRead(int pin) {
  int countLow = 0;
  int countHigh = 0;

  for (int i = 0; i < 10; i++) {
    int raw = digitalRead(pin);
    if (raw == LOW) countLow++;
    else countHigh++;
    delayMicroseconds(300);
  }

  // PRESS = solid LOW
  return (countLow > 7);  
}

void loop() {
  // === Read Joystick ===
  int joyX = analogRead(JOY_X_PIN);
  int joyY = analogRead(JOY_Y_PIN);

  bool joyPressed = softwarePullupRead(JOY_SW_PIN);

  // === Read Buttons ===
  bool btn1 = (digitalRead(BTN1_PIN) == LOW);
  bool btn2 = (digitalRead(BTN2_PIN) == LOW);
  bool btn3 = softwarePullupRead(BTN3_PIN);  // floating pin
  bool btn4 = (digitalRead(BTN4_PIN) == LOW);

  // === Print all values ===
  Serial.print("X:");
  Serial.print(joyX);
  Serial.print(" Y:");
  Serial.print(joyY);
  Serial.print(" SW:");
  Serial.print(joyPressed ? "P" : "R");

  Serial.print(" | B1:");
  Serial.print(btn1 ? "P" : "R");

  Serial.print(" B2:");
  Serial.print(btn2 ? "P" : "R");

  Serial.print(" B3:");
  Serial.print(btn3 ? "P" : "R");

  Serial.print(" B4:");
  Serial.println(btn4 ? "P" : "R");

  delay(120);
}
