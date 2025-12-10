#include <Adafruit_NeoPixel.h>

#define BUZZER_PIN 13
#define LED_PIN    14
#define NUM_LEDS   10

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Note frequencies
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784

// Full "Jingle Bells" melody
int melody[] = {
  NOTE_E4, NOTE_E4, NOTE_E4,
  NOTE_E4, NOTE_E4, NOTE_E4,
  NOTE_E4, NOTE_G4, NOTE_C4, NOTE_D4, NOTE_E4,
  
  NOTE_F4, NOTE_F4, NOTE_F4, NOTE_F4,
  NOTE_F4, NOTE_E4, NOTE_E4,
  NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_E4, NOTE_D4, NOTE_G4
};

// Note durations (4 = quarter note, 8 = eighth note)
int noteDurations[] = {
  8,8,4,
  8,8,4,
  8,8,8,8,4,

  8,8,8,8,
  8,8,8,
  8,8,8,8,8,8,4
};

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  strip.begin();
  strip.show(); // Initialize all LEDs to off
}

void loop() {
  playJingleBells();
  delay(2000); // Pause before repeating
}

void playJingleBells() {
  int numNotes = sizeof(melody)/sizeof(melody[0]);

  for (int thisNote = 0; thisNote < numNotes; thisNote++) {
    int noteDuration = 1000 / noteDurations[thisNote];

    // Play the note on the buzzer
    tone(BUZZER_PIN, melody[thisNote], noteDuration);

    // LED effect: moving rainbow along the strip
    for (int i = 0; i < NUM_LEDS; i++) {
      int colorIndex = (i*256/NUM_LEDS + thisNote*20) % 256;
      strip.setPixelColor(i, Wheel(colorIndex));
    }
    strip.show();

    // Wait for the note to finish
    delay(noteDuration * 1.30);

    // Turn off LEDs between notes
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, 0);
    strip.show();
  }
}

// Helper function to create rainbow colors
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
    WheelPos -= 170;
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}
