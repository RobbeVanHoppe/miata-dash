//
// Created by robbe on 22/03/2026.
//
#pragma once

#include <Arduino.h>
#include <TinyGPS++.h>
#include "common/BusNode.h"

// INT0 (ATmega328 hardware interrupt), active-low with internal pull-up
static constexpr uint8_t BtnPin = 2;

class GpsForwarder : public BusNode {
public:
    GpsForwarder() : BusNode(NODE_GPS_ARDUINO) {}

    void begin() override;
    void update();

private:
    void onI2CRequest() override;
    void readHardwareSerial();
    static void onBtn();

    TinyGPSPlus gps_;

    Message extMsg_;   // TYPE_INFO: altitude, sent alternating with nav
    Message btnMsg_;   // TYPE_EVENT: average-speed button press

    volatile bool btnRaw_     = false;  // set by ISR, cleared in update()
    bool          btnPending_ = false;  // debounced event, consumed in onI2CRequest()
    bool          sendExt_    = false;  // toggles nav <-> alt on each I2C request
    uint32_t      lastBtnMs_  = 0;
};