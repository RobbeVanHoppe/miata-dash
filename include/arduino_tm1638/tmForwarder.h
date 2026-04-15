//
// Created by robbe on 22/03/2026.
//
#pragma once

#include "main.h"
#include <TM1638Plus.h>

#include "arduino_tm1638/pages/generalPage.h"
#include "arduino_tm1638/pages/sensorPage.h"

class TMForwarder : public BusNode {
public:
    TMForwarder() : BusNode(NODE_TM1638_ARDUINO) {}

    void begin() override;
    void update();
    void showPageSplash(const char* text);

private:
    CarState tm1638State;

    void onI2CRequest();
    void onI2CReceive(int howMany);

    Page* pages[PAGE_COUNT];
    Page* activePage;

    unsigned long pageTimer = 0;
    bool splashActive = false;
    Message readyMsg;

    void handleInputs();

    void manageDisplay();

    void renderCurrentPage();

    void handleButtonEvents(uint8_t buttons);
};
