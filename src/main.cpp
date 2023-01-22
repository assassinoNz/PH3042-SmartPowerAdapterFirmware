#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

const char *WS_HOST = "smart-power-adapter-httpServer.fly.dev";
const short WS_HOST_PORT = 443;
const char *WS_HOST_ENDPOINT = "/";

const char WS_HOST_CERTIFICATE[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

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

WebSocketsClient webSocketClient;
AsyncWebServer httpServer(80);

const unsigned char PIN_RESET = D4;

void handleSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED: {
            Serial.printf("[WS] DISCONNECTED\n");
            break;
        }
        case WStype_CONNECTED: {
            Serial.printf("[WS] CONNECTED TO: %s\n", payload);
            webSocketClient.sendTXT("{\"event\": \"introduce\", \"deviceId\": \"123456\"}");
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

    //LittleFS
    LittleFS.begin();

    if (digitalRead(PIN_RESET) == HIGH) {
        //CASE: Reset pin is active
        LittleFS.remove("/HomeAPCredentials.txt");
    }

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

        webSocketClient.beginSslWithCA(WS_HOST, WS_HOST_PORT, WS_HOST_ENDPOINT, WS_HOST_CERTIFICATE);
        webSocketClient.onEvent(handleSocketEvent);
        Serial.println("\n[SETUP]: Established WebSocket connection HOST: " + String(WS_HOST) + ":" + String(WS_HOST_PORT));
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
    if (WiFi.status() == WL_CONNECTED) {
        webSocketClient.loop();
    }
}