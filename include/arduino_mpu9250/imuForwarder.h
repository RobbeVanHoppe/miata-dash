//
// Created by robbe on 29/03/2026.
//
#pragma once

#include <MPU9250_WE.h>
#include <Wire.h>
#include "common/Message.h"
#include "common/BusNode.h"

class IMUForwarder : public BusNode {
public:
    IMUForwarder() : BusNode(NODE_MPU9250_ARDUINO) {}

    void begin() override;
    void update();


private:
    void onI2CRequest() override;
    MPU9250_WE imu = MPU9250_WE(0x68);
};