//
// Created by robbe on 22/03/2026.
//
#pragma once

#include <Arduino.h>
#include "common/Message.h"
#include "esp32_dash/i2cHandler.h"

struct GpsState {
    double lat = 0.0;
    double lon = 0.0;
    float speed = 0.0f;
    float alt = 0.0f;
    uint8_t sats = 0;
    bool valid = false;
    uint32_t lastUpdateMs = 0;
};

class GpsHandler : I2cHandler {
public:
    // This is the entry point called by your main loop
    void handleMessage(const Message& msg);

    // Getters for your app
    const GpsState& getState() const { return state_; }
    bool isStale() const { return (millis() - state_.lastUpdateMs > 5000); }

private:
    void parseGpsData(const char* payload);
    GpsState state_;
};