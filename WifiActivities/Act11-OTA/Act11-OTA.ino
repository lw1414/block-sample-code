#include <WiFi.h>               // This library allows the ESP32 to use WiFi features.
#include <HTTPUpdateServer.h>   // This library gives us the OTA (Over-The-Air) update capability.
#include <WebServer.h>          // This library creates a simple web server so we can access the OTA page.

// -----------------------------------------------------
// WiFi CREDENTIALS
// These are the WiFi name and password that the ESP32
// will connect to during startup.
//http://10.95.204.178/update
// -----------------------------------------------------
const char* ssid = "WIFI";
const char* password = "PASS1234";

// -----------------------------------------------------
// CREATE A WEBSERVER INSTANCE
// We create a web server object that listens on port 80.
// Port 80 is the standard port used for HTTP websites.
// -----------------------------------------------------
WebServer server(80);

// -----------------------------------------------------
// CREATE AN OTA UPDATE SERVER OBJECT
// This object will add the "/update" webpage where we can
// upload a .bin firmware file to update the ESP32.
// -----------------------------------------------------
HTTPUpdateServer httpUpdater;

void setup() {
  Serial.begin(115200);   // Start the Serial Monitor for debugging.
  delay(1000);            // Small delay for stability during boot.

  // -----------------------------------------------------
  // CONNECTING TO WIFI
  // -----------------------------------------------------
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);  // Start the connection using the credentials above.

  // This loop keeps checking until the ESP32 successfully connects.
  // While it is trying, it prints dots every 500ms for visual progress.
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Once connected, print the IP address of the ESP32.
  // This IP address is important because you will use it
  // to access the OTA update page in a browser.
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // -----------------------------------------------------
  // STARTING THE OTA UPDATE SERVER
  // -----------------------------------------------------
  // The httpUpdater.setup() function automatically creates a webpage
  // at http://your-esp32-ip/update
  // On that page, you can upload a new firmware .bin file.
  httpUpdater.setup(&server);

  // Start the main web server after OTA is configured.
  server.begin();

  // Let the user know the OTA server is ready.
  Serial.println("HTTP Update server ready. Go to /update in browser to upload new firmware");
}

void loop() {
  // -----------------------------------------------------
  // ALWAYS LISTEN FOR WEB REQUESTS
  //
  // This command constantly checks if a device (like your laptop
  // or phone) is trying to access the ESP32 via the browser.
  //
  // It is also what allows the upload page to work smoothly.
  // -----------------------------------------------------
  server.handleClient();
}
