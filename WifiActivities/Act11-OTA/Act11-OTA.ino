#include <WiFi.h>
#include <HTTPUpdateServer.h>
#include <WebServer.h>

// -------------------------
// WiFi credentials
// -------------------------
const char* ssid = "RiveraWIFI";
const char* password = "@Rivera20214";

// -------------------------
// WebServer on port 80
// -------------------------
WebServer server(80);

// -------------------------
// HTTP Update server
// -------------------------
HTTPUpdateServer httpUpdater;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // -------------------------
  // Connect to WiFi
  // -------------------------
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // -------------------------
  // Start HTTP Update server
  // -------------------------
  httpUpdater.setup(&server);
  server.begin();
  Serial.println("HTTP Update server ready. Go to /update in browser to upload new firmware");
}

void loop() {
  // Handle client requests
  server.handleClient();
}
