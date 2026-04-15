//
// Created by robbe on 05/04/2026.
//
#pragma once

#include <Arduino.h>
#include <Wire.h>

enum sensorPins {
    OptoTachoPin = 2,
    OptoLightPin = 3,
    OptoBeamPin = 4,
    OptoBrakePin = 5,
    RelayRetractorPin = 7,
    RelayBeamPin = 8,
    RelayLightsPin = 9,
    RelayUNDEFINEDPin = 10,
    ADCWaterPin = A0,
    ADCOilPin = A1

};

#include "main.h"

class SensorForwarder : public BusNode {
public:
    SensorForwarder() : BusNode(NODE_SENSOR_ARDUINO) {}

    void begin() override;
    void update();

    void handleI2CRequest() {this->onI2CRequest();};
    void handleI2CReceive(int len);

private:
    const uint8_t optoPins[4] = {OptoLightPin, OptoBeamPin, OptoTachoPin, OptoBrakePin};
    const uint8_t relayPins[4] = {RelayBeamPin, RelayLightsPin, RelayRetractorPin, RelayUNDEFINEDPin};

    void processCommand(const Message& msg);
};