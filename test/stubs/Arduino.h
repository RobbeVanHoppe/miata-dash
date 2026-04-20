#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// Timing
inline uint32_t millis() { return 0; }
inline void delay(uint32_t) {}

// GPIO stubs
#define INPUT_PULLUP  0
#define OUTPUT        1
#define LOW           0
#define HIGH          1
#define DEFAULT       0
#define FALLING       0
#define LED_BUILTIN   13
#define A0            14
#define A1            15

inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int  digitalRead(uint8_t) { return HIGH; }
inline int  analogRead(uint8_t)  { return 512; }
inline void analogReference(uint8_t) {}
inline void attachInterrupt(uint8_t, void(*)(), uint8_t) {}
inline int  digitalPinToInterrupt(uint8_t pin) { return pin; }
inline void noInterrupts() {}
inline void interrupts() {}

// map/constrain
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
template<typename T> inline T constrain(T x, T lo, T hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

// Serial stub
struct SerialStub {
    void begin(uint32_t) {}
    void print(const char*)   {}
    void print(int)           {}
    void print(float)         {}
    void println(const char*) {}
    void println(int)         {}
    void println(float)       {}
    bool operator!() { return false; }
};
extern SerialStub Serial;

// F() macro
#define F(x) (x)