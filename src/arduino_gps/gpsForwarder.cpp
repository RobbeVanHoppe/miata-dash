#include "arduino_gps/gpsForwarder.h"
#include <Wire.h>

static GpsForwarder* instance = nullptr;
Message readyMsg;

void blinker() {
    for (int i = 0; i < 2; ++i) {
        digitalWrite(LED_BUILTIN, 1);
        delay(200);
        digitalWrite(LED_BUILTIN, 0);
        delay(200);
    }
}

void GpsForwarder::begin() {
    instance = this;

    Wire.begin(nodeToAddress(nodeType_));
    digitalWrite(SDA, LOW);
    digitalWrite(SCL, LOW);
    pinMode(LED_BUILTIN, OUTPUT);

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

    readyMsg.type        = (gps_.location.isValid()) ? TYPE_DATA : TYPE_ERROR;
    readyMsg.source      = nodeType_;
    readyMsg.destination = NODE_ESP32;

    if (gps_.location.isValid()) {
        // Format: "lat,lon,speed,sats"
        // Example: "51.5074,-0.1278,60.0,8"
        snprintf(readyMsg.payload, Message::MaxPayloadSize, "%.4f,%.4f,%.1f,%d",
                 gps_.location.lat(),
                 gps_.location.lng(),
                 gps_.speed.kmph(),
                 (int)gps_.satellites.value());
    } else {
        strcpy(readyMsg.payload, "NO_FIX");
    }
}

void GpsForwarder::onI2CRequest() {
    Wire.write((uint8_t*)&readyMsg, sizeof(Message));
    blinker();
}