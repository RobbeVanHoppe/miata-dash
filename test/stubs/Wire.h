#pragma once
#include <stdint.h>

struct WireStub {
    void begin(uint8_t) {}
    void onRequest(void(*)()) {}
    void onReceive(void(*)(int)) {}
    void write(const uint8_t*, size_t) {}
    void write(uint8_t) {}
    int  available() { return 0; }
    uint8_t read()   { return 0; }
    void beginTransmission(uint8_t) {}
    uint8_t endTransmission() { return 0; }
    uint8_t requestFrom(uint8_t, uint8_t) { return 0; }

    // I2C pin stubs (SDA/SCL)
    static const uint8_t SDA = 20;
    static const uint8_t SCL = 21;
};
extern WireStub Wire;

// so digitalWrite(SDA, LOW) compiles
static const uint8_t SDA = 20;
static const uint8_t SCL = 21;