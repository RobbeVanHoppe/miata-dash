#include "arduino_gps/gpsForwarder.h"
#include <Wire.h>

// Static pointer so the I2C interrupt knows which object to talk to
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

    // Start I2C as Peripheral
    Wire.begin(nodeToAddress(nodeType_));

    // Disable internal pull-up
    digitalWrite(SDA, LOW);
    digitalWrite(SCL, LOW);
    pinMode(LED_BUILTIN, OUTPUT);

    // Register the callback using a lambda
    Wire.onRequest([]() {
        if (instance) instance->onI2CRequest();
    });

    // Start Hardware Serial for the GPS module
    Serial.begin(9600);
}

void GpsForwarder::readHardwareSerial() {
    while (Serial.available() > 0) {
        gps_.encode(Serial.read());
    }
}

void GpsForwarder::update() {
    readHardwareSerial();

    // Pre-calculate the message every loop so it's "Ready" for the I2C interrupt
    readyMsg.type = (gps_.location.isValid()) ? TYPE_DATA : TYPE_ERROR;
    readyMsg.source = nodeType_;
    readyMsg.destination = NODE_ESP32;

    if (gps_.location.isValid()) {
        snprintf(readyMsg.payload, Message::MaxPayloadSize, "%.4f,%.4f,%.1f",
                 gps_.location.lat(), gps_.location.lng(), gps_.speed.kmph());
    } else {
        strcpy(readyMsg.payload, "NO_FIX");
    }
}

void GpsForwarder::onI2CRequest() {
    // DO NOT do snprintf here. Just send the pre-baked message.
    Wire.write((uint8_t*)&readyMsg, sizeof(Message));
    blinker();
}

