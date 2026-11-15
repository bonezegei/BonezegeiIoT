#ifndef BONEZEGEI_IOT_H
#define BONEZEGEI_IOT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <map>
#include <functional>

typedef std::function<void()> PropertyCallback;

enum PropertyType {
    TYPE_BOOL,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING
};

struct RegisteredProperty {
    void* varPtr;
    PropertyType type;
    PropertyCallback callback;
};

class BonezegeiIoT {
public:
    BonezegeiIoT();

    // Default target: wss://iot.bonezegei.com/api (TLS port 443)
    void begin(const char* secretKey, 
               const char* host = "iot.bonezegei.com", 
               uint16_t port = 443, 
               const char* path = "/api/device");

    void handle();

    // --- Core Property Registration ---
    void addProperty(const String& name, bool& var, PropertyCallback cb = nullptr);
    void addProperty(const String& name, int& var, PropertyCallback cb = nullptr);
    void addProperty(const String& name, float& var, PropertyCallback cb = nullptr);
    void addProperty(const String& name, String& var, PropertyCallback cb = nullptr);

    // --- Core Sync Property ---
    void syncProperty(const String& name);
    void syncAll();
    // --- Auto-Stringification Helpers ---
    
    // Modern 1-arg or 2-arg call: boneIoT.addProperty(deviceStatus) / boneIoT.addProperty(deviceStatus, callback)
    template <typename T>
    void addPropertyAuto(const char* autoName, T& var, PropertyCallback cb = nullptr) {
        addProperty(_cleanKeyName(autoName), var, cb);
    }

    // Legacy 2-arg or 3-arg call with explicit string name: boneIoT.addProperty("deviceStatus", deviceStatus)
    template <typename T>
    void addPropertyAuto(const char* autoName, const char* explicitName, T& var, PropertyCallback cb = nullptr) {
        addProperty(explicitName, var, cb);
    }

    template <typename T>
    void addPropertyAuto(const char* autoName, const String& explicitName, T& var, PropertyCallback cb = nullptr) {
        addProperty(explicitName, var, cb);
    }

    // Modern 1-arg sync: boneIoT.syncProperty(temperature)
    template <typename T>
    void syncPropertyAuto(const char* autoName, T& var) {
        syncProperty(_cleanKeyName(autoName));
    }

    // Legacy string literal sync: boneIoT.syncProperty("temperature")
    void syncPropertyAuto(const char* autoName, const char* explicitName) {
        syncProperty(explicitName);
    }

    void syncPropertyAuto(const char* autoName, const String& explicitName) {
        syncProperty(explicitName);
    }

    // Check connection status
    bool isConnected();

private:
    WiFiClientSecure _client;
    String _secretKey;
    String _host;
    uint16_t _port;
    String _path;

    bool _connected = false;
    unsigned long _lastReconnectAttempt = 0;
    unsigned long _lastPingSent = 0;

    std::map<String, RegisteredProperty> _properties;

    bool _connect();
    void _processFrame();
    void _sendFrame(uint8_t opcode, const uint8_t* payload, size_t length);
    void _sendPing();
    void _sendPong(const String& payload);
    void _parseIncomingJson(const String& jsonStr);

    // Helper to sanitize macro stringification if a callback is passed
    String _cleanKeyName(const char* raw) {
        String str = String(raw);
        int commaIdx = str.indexOf(',');
        if (commaIdx != -1) {
            str = str.substring(0, commaIdx);
        }
        str.trim();
        return str;
    }
};

// Only enable macro substitution outside of the library's own implementation file
#ifndef BONEZEGEI_IOT_CPP
#define addProperty(...) addPropertyAuto(#__VA_ARGS__, __VA_ARGS__)
#define syncProperty(var) syncPropertyAuto(#var, var)
#endif

#endif