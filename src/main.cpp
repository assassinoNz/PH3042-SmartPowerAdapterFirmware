#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266httpUpdate.h>

namespace IO {
    const unsigned char PIN_RESET = D4;
}

namespace LFS {
    const char *SELF_AP_CREDENTIALS_PATH = "/HomeAPCredentials.txt";
    const char *MQTT_CLIENT_ID_PATH = "/MQTTClientId.txt";
}

namespace WIFI {
    WiFiClientSecure client;
}

namespace HTTP {
    const char *HOST = "192.168.1.5";
    const short HOST_PORT = 8080;
    const char *FIRMWARE_URL = "/update/firmware.bin";
    const char SELF_AP_HTML[] PROGMEM = R"rawliteral(
        <!DOCTYPE html><html lang=en><head><meta charset=UTF-8><meta http-equiv=X-UA-Compatible content="IE=edge"><meta name=viewport content="width=device-width,initial-scale=1.0"><style>form{border:5px solid #000;padding:20px}form>div>label{display:block}input{padding:10px;margin:10px 0}</style></head><body><div style="display:flex;justify-content:center;align-items:center;height:100vh;"><form action=credentials><div><label for=ssid>SSID:</label><input type=text id=ssid name=ssid></div><div><label for=psk>PSK:</label><input type=psk id=psk name=psk></div><button type=submit>Save credentials and restart</button></form></div></body></html>
    )rawliteral";

    AsyncWebServer server(80);
}

namespace MQTT {
    const char *HOST = "192.168.1.5";
    const short HOST_PORT = 1883;
    const char *HOST_USERNAME = "assassino";
    const char *HOST_PASSWORD = "assassino";
    const char *CLIENT_ID = "<CLIENT_ID>";

    const char *POWER_TOPIC = strcat((char *) CLIENT_ID, "/power");
    const char *READINGS_TOPIC = strcat((char *) CLIENT_ID, "/readings");

    PubSubClient client(WIFI::client);

    void onMqttConnect(bool sessionPresent) {
        Serial.println("\n[MQTT]: Established connection HOST: " + String(MQTT::HOST) + ":" + String(MQTT::HOST_PORT));
        client.subscribe(POWER_TOPIC, 0);
    }

    void onMqttMessage(const char* topic, byte* payload, unsigned int length) {
        Serial.println("\n[MQTT]: Recieved TOPIC: " + String(topic) + " PAYLOAD: " + String((char*) payload));

        if (strcmp(topic, MQTT::POWER_TOPIC) == 0) {
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
    //     Serial.println("[LittleFS]: Removed all home AP credentials");
    // }
    File mqttClientId = LittleFS.open(LFS::MQTT_CLIENT_ID_PATH, "w");
    mqttClientId.println("abcdefghi");
    mqttClientId.close();

    Serial.println("\n[SETUP]: Retrieving MQTT client ID");
    if (LittleFS.exists(LFS::MQTT_CLIENT_ID_PATH)) {
        File mqttClientId = LittleFS.open(LFS::MQTT_CLIENT_ID_PATH, "r");
        // MQTT::CLIENT_ID = (char *) mqttClientId.readString().c_str();
        Serial.println("\n[LFS]: Retrieved MQTT client ID: " + String(MQTT::CLIENT_ID));
    } else {
        Serial.println("\n[LFS]: No MQTT client ID found. Please contact support");
        while (true);
    }

    Serial.println("\n[SETUP]: Searching for stored credentials for your home AP");
    if (LittleFS.exists(LFS::SELF_AP_CREDENTIALS_PATH)) {
        //CASE: Credentials for home AP is available
        File homeAPCredentials = LittleFS.open(LFS::SELF_AP_CREDENTIALS_PATH, "r");
        // String homeApSSID = homeAPCredentials.readStringUntil(',');
        // String homeApPSK = homeAPCredentials.readStringUntil(',');
        String homeApSSID = "Scorpius";
        String homeApPSK = "LogIn.WiFi.NSMTFS";
        homeAPCredentials.close();
        Serial.println("[SETUP]: Stored credentials found SSID: " + homeApSSID + " PSK: " + homeApPSK);

        //WiFi
        Serial.println("[SETUP]: Connecting to WiFi SSID: " + homeApSSID);
        WIFI::client.setInsecure();
        WiFi.begin(homeApSSID, homeApPSK);
        while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            delay(500);
        }
        Serial.println("\n[WIFI]: Connected to WiFi SSID: " + homeApSSID);

        //Updates
        Serial.println("[SETUP]: Checking for OTA updates");
        switch(ESPhttpUpdate.update(WIFI::client, HTTP::HOST, HTTP::HOST_PORT, HTTP::FIRMWARE_URL)) {
            case HTTP_UPDATE_FAILED: {
                Serial.println("[OTA] Failed");
                break;
            }
            case HTTP_UPDATE_NO_UPDATES: {
                Serial.println("[OTA] No updates available");
                break;
            }
            case HTTP_UPDATE_OK: {
                Serial.println("[OTA] Successful");
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
        MQTT::client.setCallback(MQTT::onMqttMessage);

        Serial.println("[MQTT]: Connecting to broker");
        while (!MQTT::client.connected()) {
            if (MQTT::client.connect("helloworld", MQTT::HOST_USERNAME, MQTT::HOST_PASSWORD)) {
                Serial.println("[MQTT]: Connected BROKER: " + String(MQTT::HOST) + ":" + String(MQTT::HOST_PORT));
                MQTT::client.subscribe(MQTT::POWER_TOPIC);
            } else {
                Serial.print(".");
                delay(500);
            }
        }

        while (true) {
            MQTT::client.publish(MQTT::READINGS_TOPIC, 0, true, "[{\"v\":0.123,\"i\":0.345,\"time\":1674890175442},{\"v\":0.456,\"i\":0.456,\"time\":1674890175442},{\"v\":0.123,\"i\":0.345,\"time\":1674890175442},{\"v\":0.789,\"i\":0.567,\"time\":1674890175442}]");
            
            MQTT::client.loop();
        }
    } else {
        //CASE: Create a softAP to change the authentication details
        Serial.println("\n[SETUP]: No stored credentials found. Configuring self AP");

        WiFi.softAP("Smart Power Adapter", "123456789");
        Serial.print("\n[SELF_AP]: Configured self AP SSID: Smart Power Adapter PSK: 123456789");
        
        HTTP::server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send_P(200, "text/html", HTTP::SELF_AP_HTML);
        });
        HTTP::server.on("/credentials", HTTP_GET, [](AsyncWebServerRequest *request) {
            File homeAPCredentials = LittleFS.open("/HomeAPCredentials.txt", "w");
            String ssid = request->arg("ssid");
            String psk = request->arg("psk");
            homeAPCredentials.print(ssid);
            homeAPCredentials.print(',');
            homeAPCredentials.print(psk);
            homeAPCredentials.print(',');
            homeAPCredentials.close();
            Serial.println("[SELF_AP]: Stored credentials for your home AP SSID: " + ssid + " PSK: " + psk);
            request->send_P(200, "text/plain", ("[SELF_AP]: Stored credentials for your home AP SSID: " + ssid + " PSK: " + psk).c_str());
            delay(2000);
            ESP.restart();
        });
        HTTP::server.begin();
        Serial.println("\n[SELF_AP]: Configured server PORT: 80");
    }
}

void loop() {
    Serial.println("[ERR]: Loop reached");
    delay(2000);
}