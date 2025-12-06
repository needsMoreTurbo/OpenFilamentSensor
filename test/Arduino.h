// Mock Arduino.h for unit testing
#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>
#include <string>
#include <cstring>

// Arduino timing
extern unsigned long millis();

// Mock String class
class String {
public:
    std::string _str;
    String() : _str() {}
    String(const char* s) : _str(s ? s : "") {}
    String(const std::string& s) : _str(s) {}
    String(int n) : _str(std::to_string(n)) {}
    String(unsigned long n) : _str(std::to_string(n)) {}
    String(float n, int decimals = 2) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimals, n);
        _str = buf;
    }

    const char* c_str() const { return _str.c_str(); }
    size_t length() const { return _str.length(); }
    bool isEmpty() const { return _str.empty(); }

    String& operator+=(const String& s) { _str += s._str; return *this; }
    String& operator+=(const char* s) { if(s) _str += s; return *this; }
    String operator+(const String& s) const { return String(_str + s._str); }
    String operator+(const char* s) const { return String(_str + (s ? s : "")); }

    bool operator==(const String& s) const { return _str == s._str; }
    bool operator==(const char* s) const { return _str == (s ? s : ""); }
};

// Mock __FlashStringHelper (used for PROGMEM strings on AVR)
class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper *>(string_literal))

// Common Arduino macros
#define PROGMEM

#endif // ARDUINO_H
