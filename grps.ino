/**************************************************************
 *
 * For this example, you need to install PubSubClient library:
 *   https://github.com/knolleary/pubsubclient
 *   or from http://librarymanager/all#PubSubClient
 *
 * TinyGSM Getting Started guide:
 *   https://tiny.cc/tinygsm-readme
 *
 * For more MQTT examples, see PubSubClient library
 *
 **************************************************************
 * Use Mosquitto client tools to work with MQTT
 *   Ubuntu/Linux: sudo apt-get install mosquitto-clients
 *   Windows:      https://mosquitto.org/download/
 *
 * Subscribe for messages:
 *   mosquitto_sub -h test.mosquitto.org -t GsmClientTest/init -t GsmClientTest/ledStatus -q 1
 * Toggle led:
 *   mosquitto_pub -h test.mosquitto.org -t GsmClientTest/led -q 1 -m "toggle"
 *
 * You can use Node-RED for wiring together MQTT-enabled devices
 *   https://nodered.org/
 * Also, take a look at these additional Node-RED modules:
 *   node-red-contrib-blynk-ws
 *   node-red-dashboard
 *
 **************************************************************/

// Please select the corresponding model

  #define SIM800L_IP5306_VERSION_20190610
// #define SIM800L_AXP192_VERSION_20200327
// #define SIM800C_AXP192_VERSION_20200609
// #define SIM800L_IP5306_VERSION_20200811

#include "utilities.h"

// Select your modem:
#define TINY_GSM_MODEM_SIM800

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial1

// See all AT commands, if wanted
//#define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon

// Add a reception delay - may be needed for a fast processor at a slow baud rate
// #define TINY_GSM_YIELD() { delay(2); }

// Define how you're planning to connect to the internet
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// set GSM PIN, if any
#define GSM_PIN ""

// Your GPRS credentials, if any
const char apn[] = "celcom4g";
const char gprsUser[] = "";
const char gprsPass[] = "";

// MQTT details
const char *broker = "broker.hivemq.com";

const char *topicLed = "GsmClientTest/led";
const char *topicInit = "GsmClientTest/init";
const char *topicLedStatus = "GsmClientTest/ledStatus";
const char *sendLocation = "Faizgps/send";

#include <TinyGsmClient.h>
#include <PubSubClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif
TinyGsmClient client(modem);
PubSubClient mqtt(client);

int ledStatus = LOW;

uint32_t lastReconnectAttempt = 0;

//****************************gps*********************
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
SoftwareSerial gpsSerial(22, 23);//rx,tx
float Glongitude = 0.0000;
float Glatitude = 0.0000;

#include <ArduinoJson.h>
// Create a JSON document
StaticJsonDocument<200> jsonDoc;
unsigned long lastMqttPublish = 0;
const unsigned long mqttInterval = 5000; // 5 seconds

//*************************************************


void mqttCallback(char *topic, byte *payload, unsigned int len)
{
    SerialMon.print("Message arrived [");
    SerialMon.print(topic);
    SerialMon.print("]: ");
    SerialMon.write(payload, len);
    SerialMon.println();

    // Only proceed if incoming message's topic matches
    if (String(topic) == topicLed) {
        ledStatus = !ledStatus;
        digitalWrite(LED_GPIO, ledStatus);
        mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
    }
   
}

boolean mqttConnect()
{
    SerialMon.print("Connecting to ");
    SerialMon.print(broker);

    // Connect to MQTT Broker
    boolean status = mqtt.connect("GsmClientTest");

    // Or, if you want to authenticate MQTT:
    //boolean status = mqtt.connect("GsmClientName", "mqtt_user", "mqtt_pass");

    if (status == false) {
        SerialMon.println(" fail");
        return false;
    }
    SerialMon.println(" success");
    mqtt.publish(topicInit, "GsmClientTest started");
    mqtt.subscribe(topicLed);
    return mqtt.connected();
}


void setup()
{
    gpsSerial.begin(9600);
    // Set console baud rate
    SerialMon.begin(115200);

    delay(10);

    setupModem();

    SerialMon.println("Wait...");

    // Set GSM module baud rate and UART pins
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

    delay(6000);

    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    SerialMon.println("Initializing modem...");
    modem.restart();
    // modem.init();

    String modemInfo = modem.getModemInfo();
    SerialMon.print("Modem Info: ");
    SerialMon.println(modemInfo);

#if TINY_GSM_USE_GPRS
    // Unlock your SIM card with a PIN if needed
    if ( GSM_PIN && modem.getSimStatus() != 3 ) {
        modem.simUnlock(GSM_PIN);
    }
#endif

    SerialMon.print("Waiting for network...");
    if (!modem.waitForNetwork()) {
        SerialMon.println(" fail");
        delay(10000);
        return;
    }
    SerialMon.println(" success");

    if (modem.isNetworkConnected()) {
        SerialMon.println("Network connected");
    }

    // GPRS connection parameters are usually set after network registration
    SerialMon.print(F("Connecting to "));
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
        delay(10000);
        return;
    }
    SerialMon.println(" success");

    if (modem.isGprsConnected()) {
        SerialMon.println("GPRS connected");
    }

    // MQTT Broker setup
    mqtt.setServer(broker, 1883);
    mqtt.setCallback(mqttCallback);
}

void loop()
{

  
  if (gpsSerial.available()) {
    ledStatus = !ledStatus;
    digitalWrite(LED_GPIO, ledStatus);
    String sentence = gpsSerial.readStringUntil('\n'); // Read a line of NMEA sentence

    // Check if the sentence starts with "$GPGGA" (Global Positioning System Fix Data)
    if (sentence.startsWith("$GPGGA")) {
      // Split the sentence into individual fields
      String fields[15]; // Assuming a maximum of 15 fields
      int fieldCount = 0;
      int startPos = 0;
      for (int i = 0; i < sentence.length(); i++) {
        if (sentence[i] == ',') {
          fields[fieldCount++] = sentence.substring(startPos, i);
          startPos = i + 1;
        }
      }

      // Extract latitude and longitude values (assuming they are in fields[2] and fields[4])
      String latitudeStr = fields[2];
      String longitudeStr = fields[4];

      // Convert latitude and longitude from string to float (you may need to handle the format)
      float latitude = latitudeStr.toFloat();
      float longitude = longitudeStr.toFloat();

      // Print the latitude and longitude
      Serial.print("Latitude: ");
      Serial.println(latitude, 6); // Display 6 decimal places for better accuracy
      Serial.print("Longitude: ");
      Serial.println(longitude, 6);
      jsonDoc["latitude"] = latitude;
      jsonDoc["longitude"] = longitude;
      
    }
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
      unsigned long currentMillis = millis();
    if (currentMillis - lastMqttPublish >= mqttInterval) {
        lastMqttPublish = currentMillis;

        // Code to prepare and publish MQTT data
        if (jsonDoc.containsKey("latitude") && jsonDoc.containsKey("longitude")) {
            char jsonBuffer[200];
            serializeJson(jsonDoc, jsonBuffer);
            mqtt.publish(sendLocation, jsonBuffer);
            jsonDoc.clear();
        }
    }

    mqtt.loop();
}