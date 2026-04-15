#include <Wire.h>
#include "esp32_dash/TM1638/tmHandler.h"

static int globalLightState = 0;
static bool globalBrake = false;

void TmHandler::handleMessage(const Message& msg, CarState& state) {
    // handle event per page
    if (msg.type == TYPE_EVENT) {
        if (msg.payload[0] == 'P' && msg.payload[1] == '0') {
            handleGeneralPage(msg.payload, state);
        }
        else if (msg.payload[0] == 'P' && msg.payload[1] == '1') {
            handleSensorPage(msg.payload, state);
        }
    }
}

void TmHandler::handleGeneralPage(const char* buttonId, CarState &state) {
    bool stateChanged = false;

    // Toggle Lights using your Enum
    if (strcmp(buttonId, "P0_B2") == 0) {
        state.lightsOn = !state.lightsOn;
        if (!state.lightsOn) state.beamOn = false;

        if (state.lightsOn) sendCommand(NODE_SENSOR_ARDUINO, "RELAY_LIGHTS_ON");
        else sendCommand(NODE_SENSOR_ARDUINO, "RELAY_LIGHTS_OFF");
        stateChanged = true;
    }

    if (strcmp(buttonId, "P0_B3") == 0) {
        if (state.lightsOn) {
            state.beamOn = !state.beamOn;

            switch (state.beamOn) {
                case 1:
                    sendCommand(NODE_SENSOR_ARDUINO, "RELAY_BEAM_ON");
                    Serial.println("Beam turn on command sent");

                    break;
                case 0:
                    sendCommand(NODE_SENSOR_ARDUINO, "RELAY_BEAM_OFF");
                    Serial.println("Beam turn off command sent");

                    break;
                default:
                    break;
            }

        }
        else {
            state.beamOn = false;
        }
        stateChanged = true;
    }

    if (stateChanged) {
        sendStatusToDashboard(state);
    }
}

void TmHandler::handleSensorPage(const char* buttonId, CarState& state) {
    if (strcmp(buttonId, "P1_B1") == 0) {
        Serial.println(">>> ACTION: Calibrating Altimeter...");
    }
}

void TmHandler::sendStatusToDashboard(const CarState &state) {
    Message msg;

    // 1. Prepare the metadata
    msg.type = TYPE_DATA;
    msg.source = NODE_ESP32;
    msg.destination = NODE_TM1638_ARDUINO;

    // 2. Prepare the payload (L=Lights 0-3, P=Parking Brake 0-1)
    // Example: "L2,P1"
    snprintf(msg.payload, Message::MaxPayloadSize, "L:%d,B:%d,U:%d,R:%d,W:%d,O:%.1f",
             state.lightsOn,
             state.beamOn,
             state.lightsUp,
             state.RPM,
             state.WaterTempC,
             state.OilPressure);

    // 3. Start I2C Transmission to the TM1638 Arduino (Address 0x11)
    Wire.beginTransmission(nodeToAddress(NODE_TM1638_ARDUINO));

    // Write the entire struct as bytes
    Wire.write((uint8_t*)&msg, sizeof(Message));

    uint8_t error = Wire.endTransmission();
    delay(200);

    // 4. Debugging
    if (error == 0) {
        Serial.printf("[ESP32] Sync Success: %s\n", msg.payload);
    } else {
        Serial.printf("[ESP32] I2C Push Error: %d\n", error);
    }
}