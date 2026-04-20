#include "arduino_mpu9250/imuForwarder.h"

static IMUForwarder* instance = nullptr;

void IMUForwarder::begin() {
    instance = this;

    Serial.begin(115200);
    while(!Serial);
    Serial.println("--- IMU Node Startup ---");

    Wire.begin(nodeToAddress(nodeType_));
    Wire.onRequest([](){
        if (instance) instance->onI2CRequest();
    });

    if (!imu.init()) {
        Serial.println("ERROR: MPU9250 not found!");
    } else {
        Serial.println("SUCCESS: MPU9250 Initialized.");
    }

    imu.setAccRange(MPU9250_ACC_RANGE_2G);

    // Pre-fill so first I2C request before update() runs gets zeros not garbage
    readyMsg.type   = TYPE_DATA;
    readyMsg.source = nodeType_;
    snprintf(readyMsg.payload, Message::MaxPayloadSize, "0.00,0.00,0.00");
}

void IMUForwarder::update() {
    xyzFloat g = imu.getGValues();

    Serial.print("X: "); Serial.print(g.x);
    Serial.print(" | Y: "); Serial.print(g.y);
    Serial.print(" | Z: "); Serial.println(g.z);

    // Format: "x,y,z"
    // Example: "0.02,0.98,0.15"
    readyMsg.type   = TYPE_DATA;
    readyMsg.source = nodeType_;
    snprintf(readyMsg.payload, Message::MaxPayloadSize, "%.2f,%.2f,%.2f",
             (double)g.x, (double)g.y, (double)g.z);
}

void IMUForwarder::onI2CRequest() {
    Wire.write((uint8_t*)&readyMsg, sizeof(Message));
}