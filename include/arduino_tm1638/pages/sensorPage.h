#pragma once
#include "arduino_tm1638/pages/page.h"

class SensorPage : public Page {
private:
    int subMode = 0; // 0: Overview (TWO), 1: Tacho, 2: Water, 3: Oil

public:
    SensorPage() : Page(PAGE_SENSORS) {}

    void onEnter(TM1638plus& tm) override {
        subMode = 0; // Always start at overview
        tm.displayText("SENSORS ");
    }

    void update(TM1638plus& tm, const CarState& state) override {
        char buf[9];

        if (subMode == 0) {
            // --- OVERVIEW MODE ---
            snprintf(buf, 9, "  TWO   ");
            tm.displayText(buf);

            tm.setLED(1, (state.RPM > 0) ? 1 : 2); // Tacho

            // Fix: Water on 2, Oil on 3
            if (state.WaterTempC > 70 && state.WaterTempC < 105) tm.setLED(2, 1);
            else tm.setLED(2, 2);

            tm.setLED(3, (state.OilPressure > 0.5) ? 1 : 2);

            for(int i=4; i<8; i++) tm.setLED(i, 0);

        } else {
            // --- SUB-MENU MODES ---
            switch(subMode) {
                case 1: { // Detailed Tacho
                    snprintf(buf, 9, "RPM %4d", state.RPM);

                    // 1. Progress Bar (LEDs 0-4) for 0-5000 RPM
                    int greenBar = map(constrain(state.RPM, 0, 5000), 0, 5000, 0, 5);
                    for(int i = 0; i < 5; i++) {
                        tm.setLED(i, (i < greenBar) ? 1 : 0); // Green
                    }

                    // 2. Blink Logic for Redline (LEDs 5-7)
                    if (state.RPM >= 5000) {
                        // Toggle every 100ms
                        bool blinkOn = (millis() / 100) % 2;
                        for(int i = 5; i < 8; i++) {
                            tm.setLED(i, blinkOn ? 2 : 0); // 2 = Red
                        }
                    } else {
                        // Turn off red LEDs if under 5k
                        for(int i = 5; i < 8; i++) tm.setLED(i, 0);
                    }
                    break;
                }

                case 2: // Detailed Water
                    tm.setLEDs(0x0000); // Clear LEDs in detail view
                    snprintf(buf, 9, "H2O %2d", state.WaterTempC);
                    break;

                case 3: // Detailed Oil
                    tm.setLEDs(0x0000); // Clear LEDs in detail view
                    snprintf(buf, 9, "OIL %3.1f", (double)state.OilPressure);
                    break;
            }
            tm.displayText(buf);
        }
    }

    const char* getButtonAction(uint8_t bit) override {
        // Use Buttons 1-4 to jump directly to views
        // Button 1: Overview
        // Button 2: Tacho
        // Button 3: Water
        // Button 4: Oil
        if (bit < 4) {
            subMode = bit;
            return "P1_SUB_CHG";
        }

        static char action[16];
        snprintf(action, 16, "P1_B%d", bit + 1);
        return action;
    }
};