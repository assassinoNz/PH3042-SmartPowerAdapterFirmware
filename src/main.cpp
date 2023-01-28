#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

const char *MQTT_HOST = "smart-power-adapter-server.fly.dev";
const short MQTT_HOST_PORT = 1883;
const char *MQTT_HOST_USERNAME = "assassino";
const char *MQTT_HOST_PASSWORD = "assassino@HiveMQ";
const char *MQTT_CLIENT_ID = "QEIZrUmZGUuzBqRnw0jZ";

const char INDEX_HTML[] PROGMEM = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta http-equiv="X-UA-Compatible" content="IE=edge">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Document</title>
    </head>
    <body>
        <h1>Hello World</h1>
    </body>
    </html>
)rawliteral";

AsyncMqttClient mqttClient;
AsyncWebServer httpServer(80);

const unsigned char PIN_RESET = D4;

void onMqttConnect(bool sessionPresent) {
    Serial.println("\n[MQTT]: Established connection HOST: " + String(MQTT_HOST) + ":" + String(MQTT_HOST_PORT));

    mqttClient.subscribe("power", 0);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.println("\n[MQTT]: Disconnected BECAUSE: ");
}

void onMqttPublish(uint16_t packetId) {
    Serial.println("\n[MQTT]: Published PACKET_ID: " + String(packetId));
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
    Serial.println("\n[MQTT]: Subscribed PACKET_ID: " + String(packetId) + " QOS: " + qos);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    Serial.println("\n[MQTT]: Recieved TOPIC: " + String(topic) + " PAYLOAD: " + String(payload));

    if (strcmp(topic, "power") == 0) {
        DynamicJsonDocument message(1024);
        deserializeJson(message, payload);

        const bool state = message["state"];
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, !state); //WARNING: LED_BUILTIN seems to be active low
    }
}

void setup() {
    //Serial communication
    Serial.begin(115200);
    Serial.println("\n==================================================================");
    Serial.println("[SETUP]: Configured Serial communication AT: 115200");

    //LittleFS
    LittleFS.begin();

    // if (digitalRead(PIN_RESET) == HIGH) {
    //     //CASE: Reset pin is active
    //     LittleFS.remove("/HomeAPCredentials.txt");
    //     Serial.println("[SETUP]: Removed stored home AP credentials");
    // }

    if (LittleFS.exists("/HomeAPCredentials.txt")) {
        //CASE: Credentials for home AP is available
        Serial.println("\n[SETUP]: Found stored credentials for a home AP");

        File homeAPCredentials = LittleFS.open("/HomeAPCredentials.txt", "r");
        String homeApSSID = homeAPCredentials.readStringUntil(',');
        String homeApPSK = homeAPCredentials.readStringUntil(',');
        homeAPCredentials.close();
        Serial.println(homeApSSID);
        Serial.println(homeApPSK);
        Serial.println("[SETUP]: Retrieved credentials SSID: " + homeApSSID + " PSK: " + homeApPSK);

        //WiFi
        Serial.println("[SETUP]: Connecting to WiFi SSID: " + homeApSSID);
        WiFi.begin("Scorpius", "LogIn.WiFi.NSMTFS");
        while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            delay(500);
        }
        Serial.println("\n[SETUP]: Connected to WiFi SSID: " + homeApSSID);

        if (time(NULL) < 1663485411) {
            //CASE: System time is not configured
            configTime(19800, 0, "pool.ntp.org");
            Serial.println("[SETUP]: Configured system time HOST: pool.ntp.org");
        }

        mqttClient.setServer(MQTT_HOST, MQTT_HOST_PORT);
        mqttClient.setClientId(MQTT_CLIENT_ID);
        mqttClient.setCredentials(MQTT_HOST_USERNAME, MQTT_HOST_PASSWORD);
        mqttClient.onConnect(onMqttConnect);
        mqttClient.onDisconnect(onMqttDisconnect);
        mqttClient.onPublish(onMqttPublish);
        mqttClient.onSubscribe(onMqttSubscribe);
        mqttClient.onMessage(onMqttMessage);
        mqttClient.connect();

        while (true) {
            mqttClient.publish("readings", 0, true, "[{\"v\":0.123,\"i\":0.345,\"time\":1674890175442},{\"v\":0.456,\"i\":0.456,\"time\":1674890175442},{\"v\":0.123,\"i\":0.345,\"time\":1674890175442},{\"v\":0.789,\"i\":0.567,\"time\":1674890175442}]");
            delay(10000);
        }
    } else {
        //CASE: Create a softAP to change the authentication details
        Serial.println("\n[SETUP]: No stored credentials for a home AP");

        WiFi.softAP("Smart Power Adapter", "123456789");
        Serial.print("\n[SETUP]: Configured self AP SSID: Smart Power Adapter PSK: 123456789");
        
        httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send_P(200, "text/html", INDEX_HTML);
        });
        httpServer.on("/connect", HTTP_GET, [](AsyncWebServerRequest *request) {
            File homeAPCredentials = LittleFS.open("/HomeAPCredentials.txt", "w");
            String ssid = request->arg("ssid");
            String psk = request->arg("psk");
            homeAPCredentials.print(ssid);
            homeAPCredentials.print(',');
            homeAPCredentials.print(psk);
            homeAPCredentials.print(',');
            homeAPCredentials.close();
            Serial.println("[SETUP]: Stored credentials for your home AP SSID: " + ssid + " PSK: " + psk);
            request->send_P(200, "text/plain", "[SETUP]: Stored credentials for your home AP successfully");
            ESP.restart();
        });
        httpServer.begin();
        Serial.println("\n[SETUP]: Configured HTTP httpServer PORT: 80");
    }
}

void loop() {
    // if (WiFi.status() == WL_CONNECTED) {
    //     webSocketClient.loop();
    // }
}