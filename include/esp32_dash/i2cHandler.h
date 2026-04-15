//
// Created by robbe on 05/04/2026.
//
#pragma once

#include <Arduino.h>
#include "Wire.h"
#include "common/Message.h"
#include "common/carState.h"

class I2cHandler{
public:
    virtual void handleMessage(const Message& msg) {
        // 1. Universal Logging Format: [Node] [Type] Payload
        Serial.print("[");
        Serial.print(messageNodeToString(msg.source));
        Serial.print("] [");
        Serial.print(messageTypeToString(msg.type));
        Serial.print("] ");
        Serial.println(msg.payload);
    };

    void sendCommand(MessageNode dest, const char* cmd) {
        Message msg(TYPE_COMMAND, NODE_ESP32, dest, cmd);
        Wire.beginTransmission(nodeToAddress(dest));
        Wire.write((uint8_t*)&msg, sizeof(Message));
        Wire.endTransmission();
    }
};
