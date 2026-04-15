#include "esp32_dash/GPS/gpsHandler.h"

void GpsHandler::handleMessage(const Message& msg) {
//    // 1. Print the log in your requested format: [Node] [Type] Payload
//    Serial.print("[");
//    Serial.print(messageNodeToString(msg.source));
//    Serial.print("] [");
//    Serial.print(messageTypeToString(msg.type));
//    Serial.print("] ");
//    Serial.println(msg.payload);
//
    // 2. Route the data based on type
    switch (msg.type) {
        case TYPE_DATA:
            parseGpsData(msg.payload);
            break;

        case TYPE_ERROR:
            state_.valid = false;
            // No need for extra prints here; the log above already shows the error text
            break;

        case TYPE_INFO:
        case TYPE_DEBUG:
            // Just logging these is enough for now
            break;

        default:
            Serial.println("   --> Unknown message type received.");
            break;
    }
}

void GpsHandler::parseGpsData(const char* payload) {
    // Expected Format: "lat,lon,speed,alt,sats"
    // We use %hhu for the uint8_t (satellites)
    int parsed = sscanf(payload, "%lf,%lf,%f,%f,%hhu",
                        &state_.lat,
                        &state_.lon,
                        &state_.speed,
                        &state_.alt,
                        &state_.sats);

    if (parsed == 5) {
        state_.valid = true;
        state_.lastUpdateMs = millis();
    } else {
        state_.valid = false;
        Serial.println("   --> [Internal] Failed to parse CSV string.");
    }
}