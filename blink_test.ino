#include <WiFi.h>
#include <Arduino.h>

#define SIM800L_IP5306_VERSION_20200811
#include "utilities.h"
#include <PubSubClient.h>

#define LED 13
#define SerialMon Serial

const char* ssid = "ikramatic-EP1";
const char* password = "91707369";

uint32_t lastReconnectAttempt = 0;
// MQTT details
const char *broker = "broker.emqx.io";

const char *topicLed = "GsmClientTest/led";
const char *topicInit = "GsmClientTest/init";
const char *topicLedStatus = "GsmClientTest/ledStatus";
const char *topicFaiz = "faiz";
const char *topicRespond = "faiz/respond";

WiFiClient espClient;
PubSubClient client(espClient);
PubSubClient mqtt(client);

void mqttCallback(char *topic, byte *payload, unsigned int len)
{
    SerialMon.print("Message arrived [");
    SerialMon.print(topic);
    SerialMon.print("]: ");
    SerialMon.write(payload, len);
    SerialMon.println();

    // Only proceed if incoming message's topic matches
    if (String(topic) == topicFaiz) {
        
        mqtt.publish(topicRespond, "Terima Message");
    }
}

boolean mqttConnect()
{
    SerialMon.print("Connecting to ");
    SerialMon.print(broker);

    // Connect to MQTT Broker
    boolean status = mqtt.connect("arduinoFaiz");

    // Or, if you want to authenticate MQTT:
    //boolean status = mqtt.connect("GsmClientName", "mqtt_user", "mqtt_pass");

    if (status == false) {
        SerialMon.println(" fail");
        return false;
    }
    SerialMon.println(" success");
    mqtt.publish(topicFaiz, "Arduino Berjaya Connected");
    mqtt.subscribe(topicFaiz);
    return mqtt.connected();
}


void setup() {
  SerialMon.begin(115200);
  delay(100);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    SerialMon.println("Connecting to WiFi...");
  }

  SerialMon.println("Connected to WiFi");
  pinMode(LED, OUTPUT);

  // MQTT Broker setup
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
}

void loop() {
  // Your code here
  if(WiFi.status()==WL_CONNECTED){
  digitalWrite(LED, HIGH);
  // SerialMon.println("LED is on");
  delay(2000);
  digitalWrite(LED, LOW);
  // SerialMon.println("LED is off");
  delay(2000);
  }
  if (!mqtt.connected()) {
        SerialMon.println("=== MQTT NOT CONNECTED ===");
        // Reconnect every 10 seconds
        uint32_t t = millis();
        if (t - lastReconnectAttempt > 10000L) {
            lastReconnectAttempt = t;
            if (mqttConnect()) {
                lastReconnectAttempt = 0;
            }
        }
        delay(100);
        return;
    }

    mqtt.loop();
}
