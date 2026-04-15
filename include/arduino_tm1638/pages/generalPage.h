//
// Created by robbe on 28/03/2026.
//
#pragma once

#include "arduino_tm1638/pages/page.h"

class GeneralPage : public Page {
public:
    GeneralPage() : Page(PAGE_GENERAL) {}

    void onEnter(TM1638plus& tm) override {
        tm.displayText("GENERAL");
        tm.setLEDs(0xFF); // 1-second splash is usually handled by the Manager
    }

    void update(TM1638plus& tm,const CarState& state) override {
        // We assume currentLights and parkingBrake are updated via I2C
        char buf[9];
        snprintf(buf, 9, "  LBR  P");
        tm.displayText(buf);

        // Handle LEDs based on external state
        updateLEDs(tm, state);
    }

    const char* getButtonAction(uint8_t bit) override {
        static char action[16];
        snprintf(action, 16, "P0_B%d", bit + 1);
        return action;
    }

private:
    void updateLEDs(TM1638plus& tm, const CarState& state) {
        // LED 1: Light State (Position 1)
        // 0=Off, 1=Green, 2=Red
        tm.setLED(1, state.lightsOn ? 1 : 0);
        tm.setLED(2, state.beamOn ? 1 : 0);

        // LED 7: Parking Brake (Position 7)
        tm.setLED(7, state.parkingBrakeOn ? 1 : 0); // Red if engaged
    }
};