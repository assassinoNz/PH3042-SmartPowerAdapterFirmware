#include <Arduino.h>
#include <time.h>
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

const char *SSID = "Scorpius";
const char *PSK = "LogIn.WiFi.NSMTFS";
const char *HOST_IP = "192.168.1.4";
const short HOST_PORT = 8081;
const char *HOST_WS_ENDPOINT = "/";

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
            case WStype_DISCONNECTED:
                Serial.printf("[WSc] Disconnected!\n");
                break;
            case WStype_CONNECTED: {
                Serial.printf("[WSc] Connected to url: %s\n", payload);
                Serial.println("[WSc] SENT: Connected");
                break;
            }
            case WStype_TEXT:
                Serial.printf("[WSc] RESPONSE: %s\n", payload);
                break;

            case WStype_BIN:
                Serial.printf("[WSc] get binary length: %u\n", length);
                hexdump(payload, length);
                break;

            case WStype_PING:
                Serial.printf("[WSc] get ping\n");
                break;

            case WStype_PONG:
                Serial.printf("[WSc] get pong\n");
                break;

        // case WStype_DISCONNECTED: {
        //     Serial.printf("[WS] DISCONNECTED\n");
        //     break;
        // }
        // case WStype_CONNECTED: {
        //     Serial.printf("[WS] CONNECTED TO: %s\n", payload);
        //     break;
        // }
        // case WStype_TEXT: {
        //     Serial.printf("[WS] FIRMWARE_EVENT (IN): %s\n", payload);
        //     break;
        // }
        // case WStype_PING: {
        //     Serial.printf("[WS] SERVER PINGED\n");
        //     break;
        // }
        // case WStype_PONG: {
        //     Serial.printf("[WS] SERVER PONGED\n");
        //     break;
        // }
        // case WStype_ERROR: {
        //     Serial.printf("[WS] ERROR OCCURRED\n");
        //     break;
        // }
        // case WStype_BIN: {
        //     break;
        // }
        // case WStype_FRAGMENT_TEXT_START: {
        //     break;
        // }
        // case WStype_FRAGMENT_BIN_START: {
        //     break;
        // }
        // case WStype_FRAGMENT: {
        //     break;
        // }
        // case WStype_FRAGMENT_FIN: {
        //     break;
        // }
    }
}

WebSocketsClient webSocket;
void setup() {
    //Serial communication
    Serial.begin(115200);
    Serial.println("\n==================================================================");
    Serial.println("CONFIGURED: Serial communication AT: 115200");

    //WiFi
    Serial.print("CONFIGURING: WiFi WITH: " + String(SSID) + " ");
    WiFi.begin(SSID, PSK);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nCONFIGURED: WiFi WITH: " + String(SSID));

        // if (time(NULL) < 1663485411) {
        //     //CASE: System time is not configured
        //     configTime(19800, 0, "pool.ntp.org");
        //     Serial.println("CONFIGURED: System time WITH: pool.ntp.org");
        // }
    }
    webSocket.begin(HOST_IP, HOST_PORT, HOST_WS_ENDPOINT);
    webSocket.onEvent(webSocketEvent);
    webSocket.loop();
}

void loop() {
    webSocket.sendTXT("{\"event\": \"introduce\", \"deviceId\": \"123456\"}");
    Serial.println("{\"event\": \"introduce\", \"deviceId\": \"123456\"}");
    delay(1000);
    // if (webSocket.isConnected() && lastUpdate + messageInterval < millis()) {
    //     lastUpdate = millis();
    // }
}