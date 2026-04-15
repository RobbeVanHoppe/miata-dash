////
//// Created by robbe on 22/03/2026.
////
#include "arduino_gps/gpsForwarder.h"

// Create the global instance of our forwarder
GpsForwarder gpsNode;

void setup() {
    // 1. Start Serial for local debugging (view on your PC)
    Serial.begin(9600);
    while (!Serial) delay(10); // Wait for USB connection

    Serial.println("--- GPS NODE STARTING ---");

    // 2. Initialize the GPS Forwarder
    // This will internally call Wire.begin(0x10) and set up the GPS UART
    gpsNode.begin();

    Serial.println("System Ready. Listening for GPS NMEA and I2C requests.");
    Serial.print("Message Struct Size: ");
    Serial.println(sizeof(Message));
}

void loop() {
    // The update function handles:
    // - Reading raw NMEA characters from the GPS module
    // - Encoding them via TinyGPS++
    // - Checking the 1Hz timer to send a Message struct to the ESP32
    gpsNode.update();
}
