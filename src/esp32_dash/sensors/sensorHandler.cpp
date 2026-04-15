#include "esp32_dash/Sensors/sensorHandler.h"

void SensorHandler::handleMessage(const Message& msg, CarState& state) {
    // --- DEBUG START ---
//    Serial.print("[I2C Incoming] Raw Payload: ");
//    Serial.println(msg.payload);
    // --- DEBUG END ---
    if (msg.type == TYPE_DATA) {
        parseMiataSensors(msg.payload, state);
    }
}

void SensorHandler::parseMiataSensors(const char* payload, CarState& state) {
    int lightsBit = 0;
    int beamBit = 0;
    int retractBit = 0;
    int rawRpm = 0;
    int waterTemp = 0;
    float oilPressure = 0.0f;

    // Expected format from Nano: "L:%d,B:%d,R:%d,T:%f"
    // L=Lights, B=HighBeam, R=Retractor, T=Tacho
    if (sscanf(payload, "L:%d,B:%d,U:%d,R:%d,W:%d,O:%.1f",
               &lightsBit,
               &beamBit,
               &retractBit,
               &rawRpm,
               &waterTemp,
               &oilPressure) >= 3) {

        // Map state
        state.lightsOn = (bool)lightsBit;
        state.beamOn = (bool)beamBit;
        state.lightsUp = (bool)retractBit;
        state.RPM = rawRpm;
        state.WaterTempC = waterTemp;
        state.OilPressure = oilPressure;
    }
}