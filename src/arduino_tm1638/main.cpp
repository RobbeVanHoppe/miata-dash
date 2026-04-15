#include <Arduino.h>
#include <Wire.h>
#include "arduino_tm1638/tmForwarder.h"

// Instantiate the controller
TMForwarder tmNode;

void setup() {
    // 1. Start Serial for local debugging
    Serial.begin(115200);
    Serial.println(F("--- TM1638 NODE STARTING ---"));

    // 2. Initialize the TM1638 and I2C Bus
    // This calls the begin() we wrote in TMForwarder.cpp
    tmNode.begin();

    // 3. Print struct size to verify 32-byte I2C limit (Should be 28)
    Serial.print(F("Message Struct Size: "));
    Serial.println(sizeof(Message));

    Serial.println(F("System Ready. Listening for I2C requests from ESP32."));
}

void loop() {
    // Keep the state machine running (checking buttons/timers)
    tmNode.update();

    // Small delay to prevent CPU hogging, though not strictly necessary
    // as the I2C communication happens via interrupts.
    delay(10);
}