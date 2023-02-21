#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <ESP8266httpUpdate.h>
#include <sensorRead.h>
#include <ESP8266HTTPClient.h>


WiFiClientSecure client;

HTTPClient http;

namespace IO {
    const unsigned char PIN_RESET = D4;
}

namespace LFS {
    const char *SELF_AP_CREDENTIALS_PATH = "/HomeAPCredentials.txt";
    const char *MQTT_CLIENT_ID_PATH = "/MQTTClientId.txt";

    void writeID(String s){
        File file = LittleFS.open(LFS::MQTT_CLIENT_ID_PATH, "w");
        file.print(s);
        file.close();
    }

    String getID() {
        String id;
        File file = LittleFS.open(LFS::MQTT_CLIENT_ID_PATH, "r");
        if (!file) {
            Serial.println("Failed to open file for reading");
            return ("failed");
        }
        while (file.available()) { id = (file.readString()); }
        file.close();
        return id;
    }
}

namespace SIN {
    bool relayState = false; 
    const int nReads = 3;
    const unsigned long rDelay = 100;

    const int vSrg[3] = {245,   250,    255};
    const int iSrg[3] = {8,     9,      10};

    void surgeProtect(int m){
        if((getV()> SIN::vSrg[0]) || (getI()> SIN::iSrg[m])) relayOn(false);
    }

    bool switchMain(){
        bool s = false;
        switch (switchAct()){
            case 1:
                s = relayOn(!getState());
                Serial.println("Toggle");
                break;

            case 2:
                LittleFS.remove(LFS::SELF_AP_CREDENTIALS_PATH);
                Serial.println("Full Reset");
                break;
        }
        return s;
    }
}

namespace WIFI {
    WiFiClient client;
}

namespace HTTP {
    const char *HOST = "150.230.238.30";
    const short HOST_PORT = 8080;
    const char *FIRMWARE_URL = "/update/firmware.bin";
    const char SELF_AP_HTML[] PROGMEM = R"rawliteral(
        <!DOCTYPE html><html lang=en><head><meta charset=UTF-8><meta http-equiv=X-UA-Compatible content="IE=edge"><meta name=viewport content="width=device-width,initial-scale=1.0"><style>form{border:5px solid #000;padding:20px}form>div>label{display:block}input{padding:10px;margin:10px 0}</style></head><body><div style="display:flex;justify-content:center;align-items:center;height:100vh;"><form action=credentials><div><label for=ssid>SSID:</label><input type=text id=ssid name=ssid></div><div><label for=psk>PSK:</label><input type=psk id=psk name=psk></div><button type=submit>Save credentials and restart</button></form></div></body></html>
    )rawliteral";

    AsyncWebServer server(80);
    

}

namespace MQTT {
    const char *HOST = "150.230.238.30";
    const short HOST_PORT = 1883;
    const char *HOST_USERNAME = "assassino";
    const char *HOST_PASSWORD = "assassino@HiveMQ";

    //const char *POWER_TOPIC = strcat((char *) CLIENT_ID, "/power");
    const char *POWER_TOPIC = "/power";
    const char *READINGS_TOPIC = "/readings";//strcat((char *) CLIENT_ID, "/readings");

    AsyncMqttClient client;

    void onMqttConnect(bool sessionPresent) {
        Serial.println("\n[MQTT]: Established connection HOST: " + String(MQTT::HOST) + ":" + String(MQTT::HOST_PORT));

        Serial.println("inTopic:" + (LFS::getID() + (MQTT::POWER_TOPIC)));
        client.subscribe((LFS::getID() + (MQTT::POWER_TOPIC)).c_str(), 0);
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

        if (strcmp(topic, (LFS::getID() + (MQTT::POWER_TOPIC)).c_str()) == 0) {
            DynamicJsonDocument message(1024);
            deserializeJson(message, payload);
            delay(3000);

            const bool state = message["state"];
            if(state) relayOn(true);//Serial.println("TURN ON");
            else relayOn(false);//Serial.println("TURN OFF");
        }
    }
}




void setup() {
    setup_Analog();


    //Serial communication
    Serial.begin(115200);
    Serial.println("\n==================================================================");
    Serial.println("[SETUP]: Configured Serial communication AT: 115200");

    pinMode(LED_BUILTIN, OUTPUT);               //temp
    client.setInsecure();

    //LittleFS
    LittleFS.begin();
    //LittleFS.remove(LFS::SELF_AP_CREDENTIALS_PATH);

    // if (digitalRead(IO::PIN_RESET) == HIGH) {
    //     //CASE: Reset pin is active
    //     LittleFS.remove(LFS::SELF_AP_CREDENTIALS_PATH);
    //     Serial.println("[LittleFS]: Removed all home AP credentials");
    // }


    // Serial.println("\n[SETUP]: Retrieving MQTT client ID");
    // if (LittleFS.exists(LFS::MQTT_CLIENT_ID_PATH)) {
    //     File mqttClientId = LittleFS.open(LFS::MQTT_CLIENT_ID_PATH, "r");
    //     MQTT::CLIENT_ID = mqttClientId.readString().c_str();
    //     Serial.println("MQTT::CLIENT_ID");
    //     mqttClientId.close();
    // } else {
    //     Serial.println("\n[LFS]: No MQTT client ID found. Please contact support");
    //     while (true);
    // }

    LFS::writeID("esp8266-52b801");
    Serial.println("Device ID : " + LFS::getID());
    

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
            //bool Success = switchMain();
        }
        Serial.println("\n[WIFI]: Connected to WiFi SSID: " + homeApSSID);

        // relayOn(true);
        // relayOn(false);
        // relayOn(true);
        // relayOn(false);

        //Updates
        Serial.println("[SETUP]: Checking for updates");
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

        // const char *tID = (LFS::getID()).c_str();
        //MQTT::client.setClientId((LFS::getID() + (MQTT::READINGS_TOPIC)).c_str());
        // MQTT::client.setClientId((const char *)(LFS::getID()).c_str());
        MQTT::client.setCredentials(MQTT::HOST_USERNAME, MQTT::HOST_PASSWORD);
        MQTT::client.onConnect(MQTT::onMqttConnect);
        MQTT::client.onDisconnect(MQTT::onMqttDisconnect);
        MQTT::client.onPublish(MQTT::onMqttPublish);
        MQTT::client.onSubscribe(MQTT::onMqttSubscribe);
        MQTT::client.onMessage(MQTT::onMqttMessage);
        

        
        while (true) {
            //bool Success = switchMain();
            //delay(2000);
            if (!MQTT::client.connected()) {
                MQTT::client.connect();
                delay(2000);
            } else {

                DynamicJsonDocument doc(1024);
                DynamicJsonDocument doc2(1024);
                
                for (int i = 0; i < SIN::nReads; i++){
                    doc["v"] = getV();
                    doc["i"] = getI();
                    doc["time"] = time(NULL);

                    doc2[i] = doc;
                    delay(SIN::rDelay);

                    long t0 = millis();
                    while ((millis()<t0+SIN::rDelay)&&(millis()-t0 >= 0)){
                        SIN::surgeProtect(0);
                    }
                } 

                char buffer[256];
                serializeJson(doc2, buffer);
                Serial.println(buffer);

                //String t = getID() + (MQTT::READINGS_TOPIC);
                MQTT::client.publish((LFS::getID() + (MQTT::READINGS_TOPIC)).c_str(), 0, true, buffer);
                delay(10000);


                // DynamicJsonDocument doc3(1024);
                //    doc3["device_id"] = LFS::getID();
                //    doc3["data reading"] = buffer;
                   
                //    char buffer2[256];
                //    serializeJson(doc3, buffer2);
                //    Serial.println(buffer2);

                // http.begin(client, "wandering-water-6831.fly.dev", 443, "/predict", true);
                // http.addHeader("Content-Type", "application/json");
                // //int httpResponseCode = http.POST("{\"device_id\":\"QEIZrUmZGUuzBqRnw0jZ\",\"data reading\":{\"i\":0.9900436818128375,\"time\":1676362048419,\"v\":0.5681495890359043}}");
                // int httpResponseCode = http.POST(buffer2);
                // String payload = http.getString(); // Get the response payload
                // Serial.println(httpResponseCode);
                // Serial.println(payload); // Print request response payload

                // http.end(); // Close connection         
            }
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
    // if (WiFi.status() == WL_CONNECTED) {
    //     webSocketClient.loop();
    // }
}