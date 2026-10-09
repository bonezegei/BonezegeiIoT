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
               //const char* host = "iot.bonezegei.com", 
               const char* host = "192.168.254.139",
               uint16_t port = 443, 
               const char* path = "/api");

    void handle();

    // Property Registration
    void addProperty(const String& name, bool& var, PropertyCallback cb = nullptr);
    void addProperty(const String& name, int& var, PropertyCallback cb = nullptr);
    void addProperty(const String& name, float& var, PropertyCallback cb = nullptr);
    void addProperty(const String& name, String& var, PropertyCallback cb = nullptr);

    // Send updated local variable value to server
    void syncProperty(const String& name);

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
};

#endif