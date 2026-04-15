#pragma once

#include <Arduino.h>
#include "common/message.h"
#include "esp32_dash/i2cHandler.h"

class TmHandler : public I2cHandler {
public:
    // Main entry point from the I2C polling loop
    void handleMessage(const Message& msg, CarState& state);
    void sendStatusToDashboard(const CarState &state);
private:
    // Sub-handlers for different "pages" on the TM1638
    void handleGeneralPage(const char* buttonId, CarState& state);
    void handleSensorPage(const char* buttonId, CarState& state);


};