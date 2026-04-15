//
// Created by robbe on 05/04/2026.
//
#pragma once

#include <Arduino.h>
#include "common/Message.h"
#include "esp32_dash/i2cHandler.h"

class SensorHandler : I2cHandler {
public:
    void handleMessage(const Message& msg, CarState& state);
    void parseMiataSensors(const char* payload, CarState& state);
};