//
// Created by robbe on 22/03/2026.
//
#pragma once

#include "main.h"

class BusNode {
public:
    BusNode(MessageNode nodeType) : nodeType_(nodeType) {}
    virtual void begin() = 0;

protected:
    MessageNode nodeType_;
    Message readyMsg;

    // It reads the raw bytes from the I2C buffer into the struct memory.
    bool receiveToStruct(Message& msg) {
        // Double check we have enough bytes waiting in the I2C buffer
        if (Wire.available() < (int)sizeof(Message)) {
            return false;
        }

        uint8_t* ptr = (uint8_t*)&msg;
        for (size_t i = 0; i < sizeof(Message); i++) {
            *ptr++ = Wire.read();
        }
        return true;
    }

    // Common send function for all children
    void sendMessage(MessageNode dest, MessageType type, const char* text) {
        Message msg(type, nodeType_, dest, text);
        uint8_t addr = nodeToAddress(dest);

        Wire.beginTransmission(addr);
        Wire.write((uint8_t*)&msg, sizeof(Message));
        Wire.endTransmission();
    }

    void sendMessage(Message msg) {
        Wire.beginTransmission(nodeToAddress(msg.destination));
        Wire.write((uint8_t*)&msg, sizeof(Message));
        Wire.endTransmission();
    }

    void onI2CRequest() {
        Wire.write((uint8_t*)&readyMsg, sizeof(Message));
    }
};