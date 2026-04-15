#include "main.h"
#include "esp32_dash/GPS/gpsHandler.h"
#include "esp32_dash/TM1638/tmHandler.h"
#include "esp32_dash/sensors/sensorHandler.h"

// 1. Single source of truth
CarState masterVehicleState;

// 2. Handlers
GpsHandler gpsHandler;
TmHandler tmHandler;
 SensorHandler sensorHandler;

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);
    Wire.setClock(100000);
    Serial.println("--- ESP32 BUS MASTER ONLINE ---");

    //TODO: Reset all arduinos
}

void requestFromNode(MessageNode node) {
    uint8_t addr = nodeToAddress(node);
    uint8_t bytesReceived = Wire.requestFrom(addr, (uint8_t)sizeof(Message));

    if (bytesReceived == sizeof(Message)) {
        Message incoming;
        auto* ptr = (uint8_t*)&incoming;
        while (Wire.available()) { *ptr++ = Wire.read(); }

        // ROUTING LOGIC - Now passing the state reference!
        switch (node) {
            case NODE_GPS_ARDUINO:
                // If you want GPS to affect car state later:
                // gpsHandler.handleMessage(incoming, masterVehicleState);
                gpsHandler.handleMessage(incoming);
                break;

            case NODE_TM1638_ARDUINO:
                // TM1638 reads and writes to the state
                tmHandler.handleMessage(incoming, masterVehicleState);
                break;

            case NODE_SENSOR_ARDUINO:
                // Optos & Relays will read and write to the state
                 sensorHandler.handleMessage(incoming, masterVehicleState);
                break;

            default:
                break;
        }
    }
}

void loop() {
    static uint32_t lastPoll = 0;
    if (millis() - lastPoll >= 100) { // Poll every 100ms
        lastPoll = millis();

        requestFromNode(NODE_SENSOR_ARDUINO);
        tmHandler.sendStatusToDashboard(masterVehicleState);
        delay(5);
//        requestFromNode(NODE_GPS_ARDUINO);
//        delay(5);

        requestFromNode(NODE_TM1638_ARDUINO);
        delay(5);

    }
}