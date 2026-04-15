//
// Created by robbe on 22/03/2026.
//
#pragma once

#include <Arduino.h>
#include <TinyGPS++.h>
#include "common/BusNode.h"

class GpsForwarder : public BusNode {
public:
    // Initialize as the GPS Arduino node
    GpsForwarder() : BusNode(NODE_GPS_ARDUINO) {}

    void begin() override;
    void update();

private:
    void onI2CRequest();
    void readHardwareSerial();

    TinyGPSPlus gps_;
};