#include "arduino_mpu9250/imuForwarder.h"

// Pointer for the I2C callback
static IMUForwarder* instance = nullptr;

void IMUForwarder::begin() {
    instance = this;

    // Start Serial for testing
    Serial.begin(115200);
    while(!Serial); // Wait for monitor to open
    Serial.println("--- IMU Node Startup ---");

    Wire.begin(nodeToAddress(nodeType_));
    Wire.onRequest([](){
        if (instance) instance->onI2CRequest();
    });

    if(!imu.init()){
        Serial.println("EROR: MPU9250 not found!");
    } else {
        Serial.println("SUCCESS: MPU9250 Initialized.");
    }

    // Set to 2G for maximum sensitivity during testing
    imu.setAccRange(MPU9250_ACC_RANGE_2G);
}

void IMUForwarder::update() {
    // 1. Get the values from the sensor
    xyzFloat g = imu.getGValues();

    // 2. Print to Serial Console for testing
    // Format: [LongG (X), LatG (Y), VertG (Z)]
    Serial.print("X: "); Serial.print(g.x);
    Serial.print(" | Y: "); Serial.print(g.y);
    Serial.print(" | Z: "); Serial.println(g.z);

    // 3. Pack into the message for the ESP32
    readyMsg.type = TYPE_DATA;
    readyMsg.source = nodeType_;
    snprintf(readyMsg.payload, Message::MaxPayloadSize, "%.2f,%.2f,%.2f", (double)g.x, (double)g.y, (double)g.z);

    // Small delay so the Serial monitor is readable
    delay(100);
}