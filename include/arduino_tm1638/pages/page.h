//
// Created by robbe on 28/03/2026.
//
#pragma once
#include <TM1638Plus.h>
#include "common/carState.h"

enum PageName { PAGE_GENERAL, PAGE_SENSORS, PAGE_COUNT };

class Page {
public:
    Page(PageName name) : name(name) {}
    virtual ~Page() {}

    // Called once when switching TO this page
    virtual void onEnter(TM1638plus& tm) = 0;

    // Called every loop
    virtual void update(TM1638plus& tm, const CarState& state) = 0;

    // Returns the string to send to ESP32 (or empty string if no action)
    virtual const char* getButtonAction(uint8_t buttonBit) = 0;

    virtual PageName getPageName() { return name; }

protected:
    PageName name;
};
