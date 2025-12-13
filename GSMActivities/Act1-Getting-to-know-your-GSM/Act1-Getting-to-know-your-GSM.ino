#include <HardwareSerial.h>

/*
  -------------------------
  ESP32 <-> SIM800C Terminal
  -------------------------

  HOW TO USE (summary for beginners)
  1) Upload this sketch to your ESP32.
  2) Open Serial Monitor (Arduino IDE).
     - Baud rate: 115200
     - Line endings: Both NL & CR (or "Both NL & CR" / "CR+LF")
  3) Type AT commands in the Serial Monitor input box and press Enter.
     - Example: AT
     - Example: AT+CSQ
  4) To send an SMS:
     - AT+CMGF=1               // put SIM800 into text mode
     - AT+CMGS="+639XXXXXXXXX" // type this and press Enter
       wait for the single greater-than sign ">" prompt
     - Type your message text (e.g. Hello!)
     - Type /send and press Enter in the Serial Monitor (this sketch will send ASCII 26 = CTRL+Z)
       (You may also press CTRL+Z on your keyboard if your terminal supports it)
  5) Look for responses from the SIM800 printed back in the Serial Monitor.

  NOTES:
  - This sketch is a direct passthrough: anything you type is forwarded to the SIM800
    except special keyword '/send' which sends CTRL+Z for you.
  - If you see "ERROR" when trying to send SMS, read the troubleshooting section below.
  - If you want a hardware LED showing status, tell me and I'll add one — but this sketch
    does not control any LED on the ESP32.

  TROUBLESHOOTING (common causes of ERROR):
  - You sent CTRL+Z before SIM800 showed the '>' prompt. Wait for '>' first.
  - You did not set text mode: run AT+CMGF=1 first.
  - Wrong phone number format: include country code and plus sign: +639XXXXXXXXX
  - No network registration (AT+CREG? shows 0,0 or 0,2) — move to area with signal.
  - SIM blocked with PIN: run AT+CPIN? and unlock if necessary.
  - Insufficient credit / SMS disabled by operator for that SIM.

  QUICK AT-COMMAND CHEAT-SHEET (type these in Serial Monitor):
  - AT                -> test module (expect OK)
  - AT+CPIN?          -> SIM status (expect +CPIN: READY)
  - AT+CSQ            -> signal quality (example +CSQ: 15,0)
  - AT+CREG?          -> network registration (example +CREG: 0,1 means registered)
  - AT+CMGF=1         -> set SMS text mode
  - AT+CMGS="+63..."  -> begin SMS send (wait for '>')
  - /send             -> special command in this sketch: sends CTRL+Z (ASCII 26)

  INTERPRETING +CSQ (signal quality returned by AT+CSQ):
    +CSQ: <rssi>,<ber>
    rssi values:
      0    -> -113 dBm or less (very poor)
      1    -> -111 dBm (very poor)
      2..9 -> weak
      10..14 -> OK
      15..19 -> good
      20..30 -> excellent
      99   -> unknown / not detectable

  INTERPRETING +CREG? (network registration):
    +CREG: <n>,<stat>
    stat values:
      0 -> not registered, not searching
      1 -> registered, home network (good)
      2 -> searching (still trying)
      3 -> registration denied
      4 -> unknown
      5 -> registered, roaming (also good)

  COMMON SMS FLOW (what you should see):
    1) AT+CMGF=1
       -> OK
    2) AT+CMGS="+639xxxxxxxxx"
       -> >
    3) (type text)
    4) /send    // this sketch sends ASCII 26
       -> +CMGS: <msg_id>
       -> OK

  NETLIGHT (SIM800 LED) BLINK INTERPRETATION — typical behavior:
    - OFF: module powered down or no power
    - FAST blink (~0.1–0.5s): searching for network or not registered
    - SLOW blink (~1s on / 1s off or ~800ms): registered to network (normal)
    Note: timings differ slightly between SIM800 variants, but the above is typical.
*/

/* Pin definitions - do not change unless your wiring differs */
#define GSM_RX 17   // ESP32 RX (connect to SIM800C TX)
#define GSM_TX 16   // ESP32 TX (connect to SIM800C RX)
#define PWRKEY 4    // connect to SIM800C PWRKEY to perform power-on pulse

HardwareSerial gsm(1); // use UART1

/* Power-on function:
   Many GSM modules require a PWRKEY pulse (hold low for ~1.2s) to boot.
   We simulate pressing the PWRKEY button here. */
void powerOnSIM800() {
  pinMode(PWRKEY, OUTPUT);
  digitalWrite(PWRKEY, LOW);   // press (active-low on many SIM modules)
  delay(1200);                 // hold ~1.2 seconds
  digitalWrite(PWRKEY, HIGH);  // release
  delay(5000);                 // wait for module to initialize
}

/* userInput buffer:
   We accumulate characters typed in Serial Monitor until a newline or carriage return
   is received. Then we either forward the command to the SIM800 or process the special
   "/send" keyword to send a CTRL+Z. */
String userInput = "";

void setup() {
  Serial.begin(115200); // Serial Monitor
  gsm.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX); // SIM800 default is often 9600

  Serial.println("\n=== SIM800C AT Command Terminal ===");
  Serial.println("Type AT commands below and press Enter.");
  Serial.println("Type /send to send CTRL+Z (end-of-message) for SMS.");
  Serial.println("------------------------------------");

  // Power ON the SIM module (only if module PWRKEY pin connected)
  powerOnSIM800();

  // Tip: After powering up, try typing "AT" and pressing Enter.
  // You should see "OK" from the module.
}

void loop() {
  // -----------------------
  // Read typed characters
  // -----------------------
  if (Serial.available()) {
    char c = Serial.read();

    // If user pressed Enter (newline or CR), handle the full line:
    if (c == '\n' || c == '\r') {
      // If the user typed "/send" as a full line, we send ASCII 26 (CTRL+Z)
      // to tell SIM800 that the SMS message is finished.
      if (userInput == "/send") {
        Serial.println(">>> Sending CTRL+Z to SIM800 (end-of-message)...");
        gsm.write(26);   // ASCII 26 = CTRL+Z
      } else {
        // Normal case: send the entire line as an AT command to SIM800.
        // Use println so the module receives CR+LF.
        gsm.println(userInput);
      }
      // Clear the buffer for the next command.
      userInput = "";
    } else {
      // Append visible characters to the command buffer
      userInput += c;
    }
  }

  // -----------------------
  // Forward any GSM replies to Serial Monitor
  // -----------------------
  while (gsm.available()) {
    char c = gsm.read();
    Serial.write(c);
  }

  // Note: This loop intentionally does nothing else so it's a transparent passthrough.
}
