#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char *ssid = "Galaxy M02s5404";
const char *password = "trfx8957";

ESP8266WebServer server(80);

void handleRoot() {
  server.send(200, "text/plain", "CricBOT is connected");
}

void handleStatus() {
  server.send(200, "text/plain", "OK");
  Serial.println("CONNECT");
}

void handleStart() {
  if (server.hasArg("mode") && server.hasArg("balls") && server.hasArg("delay")) {
    String mode = server.arg("mode");
    String balls = server.arg("balls");
    String delay = server.arg("delay");
    String command = "START:" + mode + ":" + balls + ":" + delay;
    Serial.println(command);
    server.send(200, "text/plain", "Started");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleStop() {
  Serial.println("STOP");
  server.send(200, "text/plain", "Stopped");
}

void handleConnect() {
  Serial.println("CONNECT");
  server.send(200, "text/plain", "Connected");
}

void setup() {
  Serial.begin(9600);
  delay(1000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/connect", handleConnect);
  
  server.begin();

  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  server.handleClient();
  
  if (Serial.available()) {
    String response = Serial.readStringUntil('\n');
  }

  // Blink LED to indicate the board is running
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
}
