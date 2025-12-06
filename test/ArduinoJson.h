// Mock ArduinoJson.h for unit testing
#ifndef ARDUINOJSON_H
#define ARDUINOJSON_H

#include <string>
#include <cstring>

// Minimal mock of ArduinoJson types for compilation
// These are stubs - actual JSON functionality not needed for these tests

namespace ArduinoJson {
    class JsonDocument {
    public:
        void clear() {}
    };
}

// Mock DynamicJsonDocument
class DynamicJsonDocument {
public:
    DynamicJsonDocument(size_t) {}
    void clear() {}
    bool isNull() const { return true; }
};

// Mock StaticJsonDocument
template<size_t N>
class StaticJsonDocument {
public:
    void clear() {}
    bool isNull() const { return true; }
};

// Mock JsonObject
class JsonObject {
public:
    bool isNull() const { return true; }
};

// Mock JsonArray
class JsonArray {
public:
    bool isNull() const { return true; }
};

#endif // ARDUINOJSON_H
