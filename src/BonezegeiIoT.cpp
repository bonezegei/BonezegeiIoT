#define BONEZEGEI_IOT_CPP
#include "BonezegeiIoT.h"

BonezegeiIoT::BonezegeiIoT() {}

void BonezegeiIoT::begin(const char* secretKey, const char* host, uint16_t port, const char* path) {
    _secretKey = secretKey;
    _host = host;
    _port = port;
    _path = path;
    _client.setInsecure(); // SSL without requiring hardcoded CA certs
    _connect();
}

void BonezegeiIoT::handle() {
    // Reconnection handler
    if (!_connected || !_client.connected()) {
        _connected = false;
        if (millis() - _lastReconnectAttempt > 5000) {
            _lastReconnectAttempt = millis();
            _connect();
        }
        return;
    }

    // Process WebSocket frames
    if (_client.available()) {
        _processFrame();
    }
}

bool BonezegeiIoT::isConnected() {
    return _connected && _client.connected();
}

void BonezegeiIoT::addProperty(const String& name, bool& var, PropertyCallback cb) {
    _properties[name] = { &var, TYPE_BOOL, cb };
}

void BonezegeiIoT::addProperty(const String& name, int& var, PropertyCallback cb) {
    _properties[name] = { &var, TYPE_INT, cb };
}

void BonezegeiIoT::addProperty(const String& name, float& var, PropertyCallback cb) {
    _properties[name] = { &var, TYPE_FLOAT, cb };
}

void BonezegeiIoT::addProperty(const String& name, String& var, PropertyCallback cb) {
    _properties[name] = { &var, TYPE_STRING, cb };
}

void BonezegeiIoT::syncProperty(const String& name) {
    if (!isConnected() || _properties.find(name) == _properties.end()) return;

    RegisteredProperty prop = _properties[name];
    String payload = "{\"" + name + "\":";

    switch (prop.type) {
        case TYPE_BOOL:
            payload += (*(static_cast<bool*>(prop.varPtr)) ? "true" : "false");
            break;
        case TYPE_INT:
            payload += String(*(static_cast<int*>(prop.varPtr)));
            break;
        case TYPE_FLOAT:
            payload += String(*(static_cast<float*>(prop.varPtr)), 4);
            break;
        case TYPE_STRING:
            payload += "\"" + *(static_cast<String*>(prop.varPtr)) + "\"";
            break;
    }
    payload += "}";

    _sendFrame(0x01, (const uint8_t*)payload.c_str(), payload.length());
}

bool BonezegeiIoT::_connect() {
    Serial.print("[BonezegeiIoT] Connecting to ");
    Serial.print(_host);
    Serial.println("...");

    _client.stop();

    if (!_client.connect(_host.c_str(), _port)) {
        Serial.println("[BonezegeiIoT] Connection failed.");
        return false;
    }

    // WSS Handshake with Secret Key Auth
    String handshake = "GET " + _path + " HTTP/1.1\r\n" +
                       "Host: " + _host + "\r\n" +
                       "Upgrade: websocket\r\n" +
                       "Connection: Upgrade\r\n" +
                       "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" +
                       "Sec-WebSocket-Version: 13\r\n" +
                       "Authorization: Bearer " + _secretKey + "\r\n" +
                       "X-Secret-Key: " + _secretKey + "\r\n\r\n";

    _client.print(handshake);

    unsigned long timeout = millis();
    bool isHandshakeOk = false;

    while (_client.connected() && millis() - timeout < 5000) {
        String line = _client.readStringUntil('\n');
        if (line.indexOf("101") != -1) {
            isHandshakeOk = true;
        }
        if (line == "\r" || line.length() == 0) {
            break;
        }
    }

    if (isHandshakeOk) {
        _connected = true;
        Serial.println("[BonezegeiIoT] Connected to wss://iot.bonezegei.com/api");
        return true;
    } else {
        Serial.println("[BonezegeiIoT] Handshake failed!");
        _client.stop();
        return false;
    }
}

void BonezegeiIoT::_sendFrame(uint8_t opcode, const uint8_t* payload, size_t length) {
    if (!_client.connected()) return;

    uint8_t header[14];
    size_t headerLen = 0;

    header[0] = 0x80 | (opcode & 0x0F);

    if (length <= 125) {
        header[1] = 0x80 | (uint8_t)length;
        headerLen = 2;
    } else if (length <= 65535) {
        header[1] = 0x80 | 126;
        header[2] = (length >> 8) & 0xFF;
        header[3] = length & 0xFF;
        headerLen = 4;
    } else {
        header[1] = 0x80 | 127;
        for (int i = 0; i < 8; i++) {
            header[2 + i] = (length >> (56 - i * 8)) & 0xFF;
        }
        headerLen = 10;
    }

    uint32_t randomVal = esp_random();
    uint8_t mask[4] = {
        (uint8_t)(randomVal >> 24),
        (uint8_t)(randomVal >> 16),
        (uint8_t)(randomVal >> 8),
        (uint8_t)(randomVal)
    };

    for (int i = 0; i < 4; i++) {
        header[headerLen++] = mask[i];
    }

    _client.write(header, headerLen);

    if (length > 0) {
        uint8_t maskedPayload[length];
        for (size_t i = 0; i < length; i++) {
            maskedPayload[i] = payload[i] ^ mask[i % 4];
        }
        _client.write(maskedPayload, length);
    }
}

void BonezegeiIoT::_sendPing() {
    _sendFrame(0x09, NULL, 0);
}

void BonezegeiIoT::_sendPong(const String& payload) {
    _sendFrame(0x0A, (const uint8_t*)payload.c_str(), payload.length());
}

void BonezegeiIoT::_processFrame() {
    if (!_client.available()) return;

    uint8_t b0 = _client.read();
    uint8_t b1 = _client.read();

    uint8_t opcode = b0 & 0x0F;
    bool masked = (b1 & 0x80) != 0;
    uint64_t payloadLen = b1 & 0x7F;

    if (payloadLen == 126) {
        uint8_t lenBuf[2];
        _client.readBytes(lenBuf, 2);
        payloadLen = ((uint64_t)lenBuf[0] << 8) | lenBuf[1];
    } else if (payloadLen == 127) {
        uint8_t lenBuf[8];
        _client.readBytes(lenBuf, 8);
        payloadLen = 0;
        for (int i = 0; i < 8; i++) {
            payloadLen = (payloadLen << 8) | lenBuf[i];
        }
    }

    uint8_t maskKey[4] = {0};
    if (masked) {
        _client.readBytes(maskKey, 4);
    }

    String payload = "";
    payload.reserve(payloadLen);

    for (uint64_t i = 0; i < payloadLen; i++) {
        while (!_client.available() && _client.connected()) {
            delay(1);
        }
        uint8_t c = _client.read();
        if (masked) {
            c ^= maskKey[i % 4];
        }
        payload += (char)c;
    }

    if (opcode == 0x01) {
        _parseIncomingJson(payload);
    } else if (opcode == 0x08) {
        _connected = false;
        _client.stop();
    } else if (opcode == 0x09) {
        _sendPong(payload);
    }
}

void BonezegeiIoT::_parseIncomingJson(const String& jsonStr) {
    for (auto const& item : _properties) {
        String key = item.first;
        RegisteredProperty prop = item.second;

        String keyPattern = "\"" + key + "\"";
        int keyIndex = jsonStr.indexOf(keyPattern);
        if (keyIndex != -1) {
            int colonIndex = jsonStr.indexOf(':', keyIndex + keyPattern.length());
            if (colonIndex != -1) {
                int startIdx = colonIndex + 1;
                while (startIdx < jsonStr.length() && (jsonStr[startIdx] == ' ' || jsonStr[startIdx] == '\t' || jsonStr[startIdx] == '\r' || jsonStr[startIdx] == '\n')) {
                    startIdx++;
                }

                String valStr = "";
                if (prop.type == TYPE_STRING) {
                    if (jsonStr[startIdx] == '"') {
                        startIdx++;
                        int endQuote = jsonStr.indexOf('"', startIdx);
                        if (endQuote != -1) {
                            valStr = jsonStr.substring(startIdx, endQuote);
                        }
                    }
                } else {
                    int endIdx = startIdx;
                    while (endIdx < jsonStr.length() && jsonStr[endIdx] != ',' && jsonStr[endIdx] != '}' && jsonStr[endIdx] != ']' && jsonStr[endIdx] != '\r' && jsonStr[endIdx] != '\n') {
                        endIdx++;
                    }
                    valStr = jsonStr.substring(startIdx, endIdx);
                    valStr.trim();
                }

                if (valStr.length() > 0) {
                    switch (prop.type) {
                        case TYPE_BOOL:
                            *(static_cast<bool*>(prop.varPtr)) = (valStr == "true" || valStr == "1");
                            break;
                        case TYPE_INT:
                            *(static_cast<int*>(prop.varPtr)) = valStr.toInt();
                            break;
                        case TYPE_FLOAT:
                            *(static_cast<float*>(prop.varPtr)) = valStr.toFloat();
                            break;
                        case TYPE_STRING:
                            *(static_cast<String*>(prop.varPtr)) = valStr;
                            break;
                    }

                    if (prop.callback) {
                        prop.callback();
                    }
                }
            }
        }
    }
}