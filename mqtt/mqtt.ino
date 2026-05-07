#include <ESP8266WiFi.h>  // Correct library for ESP8266
#include <PubSubClient.h>

const char* ssid = "#GigaSmartWiFi";
const char* wifiPassword = "no-passwor";

// This must match the IP of your Windows machine from the previous steps
const char* mqttServer = "10.74.69.16"; 
const int mqttPort = 1883;
const char* mqttUser = "admin";
const char* mqttPassword = "admin123"; // Fixed typo in variable name

WiFiClient espClient; // Fixed casing: WiFiClient
PubSubClient client(espClient);

void setupWiFi() { // Fixed casing: setupWiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, wifiPassword); // Fixed casing: WiFi
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
}

void reconnectMQTT(){
  while (!client.connected()) { // Fixed typo: connected()
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client", mqttUser, mqttPassword)) {
      Serial.println("connected");
      client.publish("iot/status", "ESP8266 connected to Windows Mosquitto");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setupWiFi();
  client.setServer(mqttServer, mqttPort);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  float temperature = 29.5;
  char message[10];
  dtostrf(temperature, 1, 2, message);
  
  Serial.print("Publishing temperature: ");
  Serial.println(message);
  client.publish("iot/temperature", message);

  delay(5000);
}