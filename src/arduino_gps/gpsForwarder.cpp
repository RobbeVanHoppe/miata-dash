#include "arduino_gps/gpsForwarder.h"
#include <Wire.h>

static GpsForwarder* instance = nullptr;

void blinker() {
    for (int i = 0; i < 2; ++i) {
        digitalWrite(LED_BUILTIN, 1);
        delay(200);
        digitalWrite(LED_BUILTIN, 0);
        delay(200);
    }
}

// ISR -- keep it minimal: just set a flag, debounce happens in update().
void GpsForwarder::onBtn() {
    if (instance) instance->btnRaw_ = true;
}

void GpsForwarder::begin() {
    instance = this;

    Wire.begin(nodeToAddress(nodeType_));
    digitalWrite(SDA, LOW);
    digitalWrite(SCL, LOW);
    pinMode(LED_BUILTIN, OUTPUT);

    // Active-low button with internal pull-up on INT0.
    // Wire one side of the button to GND, the other to pin 2.
    pinMode(BtnPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BtnPin), onBtn, FALLING);

    Wire.onRequest([]() {
        if (instance) instance->onI2CRequest();
    });

    Serial.begin(9600);
}

void GpsForwarder::readHardwareSerial() {
    while (Serial.available() > 0) {
        gps_.encode(Serial.read());
    }
}

void GpsForwarder::update() {
    readHardwareSerial();

    // Debounce button (50 ms minimum between events)
    if (btnRaw_) {
        btnRaw_ = false;
        uint32_t now = millis();
        if (now - lastBtnMs_ > 50) {
            lastBtnMs_ = now;
            btnPending_ = true;
        }
    }

    // Nav message (TYPE_DATA) -- format unchanged
    readyMsg.type        = gps_.location.isValid() ? TYPE_DATA : TYPE_ERROR;
    readyMsg.source      = nodeType_;
    readyMsg.destination = NODE_ESP32;

    if (gps_.location.isValid()) {
        // Format: "lat,lon,speed,sats"
        snprintf(readyMsg.payload, Message::MaxPayloadSize, "%.4f,%.4f,%.1f,%d",
                 gps_.location.lat(),
                 gps_.location.lng(),
                 gps_.speed.kmph(),
                 (int)gps_.satellites.value());
    } else {
        strcpy(readyMsg.payload, "NO_FIX");
    }

    // Altitude message (TYPE_INFO) -- "A:<meters>"
    extMsg_.type        = TYPE_INFO;
    extMsg_.source      = nodeType_;
    extMsg_.destination = NODE_ESP32;
    snprintf(extMsg_.payload, Message::MaxPayloadSize, "A:%d",
             gps_.altitude.isValid() ? (int)gps_.altitude.meters() : 0);

    // Button event message (TYPE_EVENT) -- built when pending
    if (btnPending_) {
        btnMsg_.type        = TYPE_EVENT;
        btnMsg_.source      = nodeType_;
        btnMsg_.destination = NODE_ESP32;
        strcpy(btnMsg_.payload, "AVG_BTN");
    }
}

void GpsForwarder::onI2CRequest() {
    // Priority: pending button event > altitude info (alternating) > nav data.
    if (btnPending_) {
        Wire.write((uint8_t*)&btnMsg_, sizeof(Message));
        btnPending_ = false;
    } else if (sendExt_) {
        Wire.write((uint8_t*)&extMsg_, sizeof(Message));
        sendExt_ = false;
    } else {
        Wire.write((uint8_t*)&readyMsg, sizeof(Message));
        sendExt_ = true;
    }
    blinker();
}