#include <WiFi.h>
#include <BonezegeiIoT.h>

const char* ssid     = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";
const char* secretKey = "your-bonezegei-secret-key";

BonezegeiIoT boneIoT;

// Bound Variables
bool relayState = false;
int thresholdValue = 50;
float sensorTemp = 24.5;
String deviceStatus = "Booting";

#define LED_BUILTIN 2

void onRelayStateChange() {
    Serial.print("Relay state updated: ");
    Serial.println(relayState ? "ON" : "OFF");
    digitalWrite(LED_BUILTIN, relayState ? HIGH : LOW);
}

void onThresholdChange() {
    Serial.print("Threshold value updated to: ");
    Serial.println(thresholdValue);
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");

    // Bind variables with optional callbacks
    boneIoT.addProperty("relayState", relayState, onRelayStateChange);
    boneIoT.addProperty("thresholdValue", thresholdValue, onThresholdChange);
    boneIoT.addProperty("sensorTemp", sensorTemp);
    boneIoT.addProperty("deviceStatus", deviceStatus);

    boneIoT.begin(secretKey);
}

unsigned long lastSend = 0;

void loop() {
    boneIoT.handle();

    // Push local updates every 5 seconds
    if (millis() - lastSend > 5000) {
        lastSend = millis();

        sensorTemp += 0.25;
        deviceStatus = "Online";

        boneIoT.syncProperty("sensorTemp");
        boneIoT.syncProperty("deviceStatus");
    }
}