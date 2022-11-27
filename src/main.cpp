#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

WebSocketsClient webSocket;
const char *SSID = "Scorpius";
const char *PSK = "LogIn.WiFi.NSMTFS";
const char *HOST_IP = "192.168.1.4";
const short HOST_PORT = 8081;
const char *HOST_WS_ENDPOINT = "/";

void handleSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED: {
            Serial.printf("[WS] DISCONNECTED\n");
            break;
        }
        case WStype_CONNECTED: {
            Serial.printf("[WS] CONNECTED TO: %s\n", payload);
            webSocket.sendTXT("{\"event\": \"introduce\", \"deviceId\": \"123456\"}");
            Serial.println("[FE_OUT]: {\"deviceId\": \"123456\", \"event\": \"introduce\"} STATUS: Ok");
            break;
        }
        case WStype_TEXT: {
            Serial.printf("[FE_IN]: %s STATUS: Ok\n", payload);

            DynamicJsonDocument event(1024);
            deserializeJson(event, payload);

            const char* name = event["event"];
            if (strcmp(name, "set-power") == 0) {
                boolean state = event["state"];
                pinMode(LED_BUILTIN, OUTPUT);
                digitalWrite(LED_BUILTIN, !state); //WARNING: LED_BUILTIN seems to be active low
            }

            break;
        }
        case WStype_PING: {
            Serial.printf("[WS] SERVER PINGED\n");
            break;
        }
        case WStype_PONG: {
            Serial.printf("[WS] SERVER PONGED\n");
            break;
        }
        case WStype_ERROR: {
            Serial.printf("[WS] ERROR OCCURRED\n");
            break;
        }
        case WStype_BIN: {
            break;
        }
        case WStype_FRAGMENT_TEXT_START: {
            break;
        }
        case WStype_FRAGMENT_BIN_START: {
            break;
        }
        case WStype_FRAGMENT: {
            break;
        }
        case WStype_FRAGMENT_FIN: {
            break;
        }
    }
}

void setup() {
    //Serial communication
    Serial.begin(115200);
    Serial.println("\n==================================================================");
    Serial.println("[SETUP]: Configured Serial communication AT: 115200");

    //WiFi
    Serial.print("[SETUP]: Configuring WiFi WITH: " + String(SSID) + " ");
    WiFi.begin(SSID, PSK);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[SETUP]: Configured WiFi WITH: " + String(SSID));

        if (time(NULL) < 1663485411) {
            //CASE: System time is not configured
            configTime(19800, 0, "pool.ntp.org");
            Serial.println("[SETUP]: Configured system time WITH: pool.ntp.org");
        }
    }

    webSocket.begin(HOST_IP, HOST_PORT, HOST_WS_ENDPOINT);
    webSocket.onEvent(handleSocketEvent);
}

void loop() {
    webSocket.loop();
}