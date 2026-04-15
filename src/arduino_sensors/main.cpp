
#include "arduino_sensors/sensorForwarder.h"

// Instantiate the controller
SensorForwarder sensorNode;

void setup() {
    // 1. Start Serial for local debugging
    Serial.begin(115200);
    Serial.println(F("--- SENSOR NODE STARTING ---"));

    // 2. Initialize the TM1638 and I2C Bus
    // This calls the begin() we wrote in TMForwarder.cpp
    sensorNode.begin();

    // 3. Print struct size to verify 32-byte I2C limit (Should be 28)
    Serial.print(F("Message Struct Size: "));
    Serial.println(sizeof(Message));

    Serial.println(F("System Ready. Listening for I2C requests from ESP32."));
}

void loop() {
    // Keep the state machine running (checking buttons/timers)
    sensorNode.update();

    // Small delay to prevent CPU hogging, though not strictly necessary
    // as the I2C communication happens via interrupts.
    delay(10);
}