#include "arduino_mpu9250/imuForwarder.h"

IMUForwarder forwarder;

void setup() {
    forwarder.begin();
}

void loop() {
    forwarder.update();
    delay(20); // 50Hz is smooth for G-meters
}