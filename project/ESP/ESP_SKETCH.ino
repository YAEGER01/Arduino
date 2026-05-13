#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const char *ssid = "#GigaSmartWiFi";
const char *password = "no-passwor";
const char *mqtt_server = "10.169.151.16";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

void setup()
{
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  client.setServer(mqtt_server, mqtt_port);
}

void reconnect()
{
  while (!client.connected())
  {
    if (client.connect("ESP8266_Bridge", "admin", "admin123"))
    {
      Serial.println("MQTT Connected");
    }
    else
    {
      delay(5000);
    }
  }
}

void loop()
{
  if (!client.connected())
    reconnect();
  client.loop();

  if (Serial.available())
  {
    String raw = Serial.readStringUntil('\n');
    raw.trim();

    int firstPipe = raw.indexOf('|');
    if (firstPipe != -1)
    {
      String setId = raw.substring(0, firstPipe);
      String payload = raw.substring(firstPipe + 1);
      String topic = "hallway/" + setId;

      client.publish(topic.c_str(), payload.c_str());
      Serial.println("Published to " + topic + ": " + payload);
    }
  }
}