#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <ESP8266httpUpdate.h>

namespace IO {
    const unsigned char PIN_RESET = D4;
}

namespace LFS {
    const char *SELF_AP_CREDENTIALS_PATH = "/HomeAPCredentials.txt";
}

namespace WIFI {
    WiFiClient client;
}

namespace HTTP {
    AsyncWebServer server(80);

    const char *HOST = "192.168.1.4";
    const short HOST_PORT = 8080;
    const char *FIRMWARE_URL = "/update/firmware.bin";
    const char SELF_AP_HTML[] PROGMEM = R"rawliteral(
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
}

namespace MQTT {
    AsyncMqttClient client;

    const char *HOST = "192.168.1.4";
    const short HOST_PORT = 1883;
    const char *HOST_USERNAME = "assassino";
    const char *HOST_PASSWORD = "assassino@HiveMQ";
    const char *CLIENT_ID = "QEIZrUmZGUuzBqRnw0jZ";

    void onMqttConnect(bool sessionPresent) {
        Serial.println("\n[MQTT]: Established connection HOST: " + String(MQTT::HOST) + ":" + String(MQTT::HOST_PORT));

        client.subscribe("power", 0);
    }

    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
        Serial.print("\n[MQTT]: Disconnected BECAUSE: " + String((unsigned char) reason));
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
}

void setup() {
    //Serial communication
    Serial.begin(115200);
    Serial.println("\n==================================================================");
    Serial.println("[SETUP]: Configured Serial communication AT: 115200");

    //LittleFS
    LittleFS.begin();

    // if (digitalRead(IO::PIN_RESET) == HIGH) {
    //     //CASE: Reset pin is active
    //     LittleFS.remove(LFS::SELF_AP_CREDENTIALS_PATH);
    //     Serial.println("[SETUP]: Removed stored home AP credentials");
    // }

    Serial.println("\n[SETUP]: Searching for stored credentials for your home AP");
    if (LittleFS.exists(LFS::SELF_AP_CREDENTIALS_PATH)) {
        //CASE: Credentials for home AP is available
        File homeAPCredentials = LittleFS.open(LFS::SELF_AP_CREDENTIALS_PATH, "r");
        String homeApSSID = homeAPCredentials.readStringUntil(',');
        String homeApPSK = homeAPCredentials.readStringUntil(',');
        homeAPCredentials.close();
        Serial.println("[SETUP]: Stored credentials found SSID: " + homeApSSID + " PSK: " + homeApPSK);

        //WiFi
        Serial.println("[SETUP]: Connecting to WiFi SSID: " + homeApSSID);
        WiFi.begin(homeApSSID, homeApPSK);
        while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            delay(500);
        }
        Serial.println("\n[WIFI]: Connected to WiFi SSID: " + homeApSSID);

        //Updates
        Serial.println("[SETUP]: Checking for updates");
        switch(ESPhttpUpdate.update(WIFI::client, HTTP::HOST, HTTP::HOST_PORT, HTTP::FIRMWARE_URL)) {
            case HTTP_UPDATE_FAILED: {
                Serial.println("[UPDATE] Failed");
                break;
            }
            case HTTP_UPDATE_NO_UPDATES: {
                Serial.println("[UPDATE] No updates available");
                break;
            }
            case HTTP_UPDATE_OK: {
                Serial.println("[UPDATE] Successful");
                break;
            }
        }

        //Time
        Serial.println("[SETUP]: Configuring system time");
        if (time(NULL) < 1663485411) {
            //CASE: System time is not configured
            configTime(19800, 0, "pool.ntp.org");
            Serial.println("[TIME]: Configured system time HOST: pool.ntp.org");
        }

        Serial.println("[SETUP]: Configuring MQTT");
        MQTT::client.setServer(MQTT::HOST, MQTT::HOST_PORT);
        MQTT::client.setClientId(MQTT::CLIENT_ID);
        MQTT::client.setCredentials(MQTT::HOST_USERNAME, MQTT::HOST_PASSWORD);
        MQTT::client.onConnect(MQTT::onMqttConnect);
        MQTT::client.onDisconnect(MQTT::onMqttDisconnect);
        MQTT::client.onPublish(MQTT::onMqttPublish);
        MQTT::client.onSubscribe(MQTT::onMqttSubscribe);
        MQTT::client.onMessage(MQTT::onMqttMessage);
        MQTT::client.connect();

        while (true) {
            MQTT::client.publish("readings", 0, true, "[{\"v\":0.123,\"i\":0.345,\"time\":1674890175442},{\"v\":0.456,\"i\":0.456,\"time\":1674890175442},{\"v\":0.123,\"i\":0.345,\"time\":1674890175442},{\"v\":0.789,\"i\":0.567,\"time\":1674890175442}]");
            delay(10000);
        }
    } else {
        //CASE: Create a softAP to change the authentication details
        Serial.println("\n[SETUP]: No stored credentials found. COnfiguring self AP");

        WiFi.softAP("Smart Power Adapter", "123456789");
        Serial.print("\n[SELF_AP]: Configured self AP SSID: Smart Power Adapter PSK: 123456789");
        
        HTTP::server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send_P(200, "text/html", HTTP::SELF_AP_HTML);
        });
        HTTP::server.on("/connect", HTTP_GET, [](AsyncWebServerRequest *request) {
            File homeAPCredentials = LittleFS.open("/HomeAPCredentials.txt", "w");
            String ssid = request->arg("ssid");
            String psk = request->arg("psk");
            homeAPCredentials.print(ssid);
            homeAPCredentials.print(',');
            homeAPCredentials.print(psk);
            homeAPCredentials.print(',');
            homeAPCredentials.close();
            Serial.println("[SELF_AP]: Stored credentials for your home AP SSID: " + ssid + " PSK: " + psk);
            request->send_P(200, "text/plain", "[SELF_AP]: Stored credentials for your home AP successfully");
            ESP.restart();
        });
        HTTP::server.begin();
        Serial.println("\n[SELF_AP]: Configured server PORT: 80");
    }
}

void loop() {
    // if (WiFi.status() == WL_CONNECTED) {
    //     webSocketClient.loop();
    // }
}