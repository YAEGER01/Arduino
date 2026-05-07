#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// WiFi credentials
const char* ssid = "#GigaSmartWiFi";
const char* password = "no-passwor";

// Flask server
const char* serverUrl = "http://10.169.71.16:5000/data";

// ===== QUEUE SYSTEM =====
#define QUEUE_SIZE 10
String queue[QUEUE_SIZE];
int qStart = 0, qEnd = 0;

// ===== SERIAL PARSER =====
String jsonBuffer = "";
bool receiving = false;

void enqueue(String data) {
  int next = (qEnd + 1) % QUEUE_SIZE;

  if (next == qStart) {
    Serial.println("Queue FULL, dropping oldest...");
    qStart = (qStart + 1) % QUEUE_SIZE;
  }

  queue[qEnd] = data;
  qEnd = next;
}

bool dequeue(String &data) {
  if (qStart == qEnd) return false;

  data = queue[qStart];
  qStart = (qStart + 1) % QUEUE_SIZE;
  return true;
}

void setup() {
  Serial.begin(9600);

  WiFi.begin(ssid, password);

  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
}

void handleSerial() {
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line == "BEGIN") {
      jsonBuffer = "";
      receiving = true;
    }
    else if (line == "END") {
      receiving = false;

      Serial.println("Received packet:");
      Serial.println(jsonBuffer);

      enqueue(jsonBuffer);
    }
    else if (receiving) {
      jsonBuffer += line;
    }
  }
}

void sendToServer(String payload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, skipping send...");
    return;
  }

  WiFiClient client;
  HTTPClient http;

  http.begin(client, serverUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(2000);

  Serial.println("Sending:");
  Serial.println(payload);

  int code = http.POST(payload);

  Serial.print("HTTP Code: ");
  Serial.println(code);

  http.end();
}

void handleQueue() {
  static unsigned long lastSend = 0;

  if (millis() - lastSend < 2000) return; // send every 2 sec
  lastSend = millis();

  String data;
  if (dequeue(data)) {
    sendToServer(data);
  }
}

void loop() {
  handleSerial();
  handleQueue();

  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 2000) {
    lastDebug = millis();
    Serial.println("ESP alive...");
  }
}