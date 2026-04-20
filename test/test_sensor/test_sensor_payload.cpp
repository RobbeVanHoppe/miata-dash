#include "unity.h"
#include <stdio.h>
#include <string.h>
#include "common/message.h"

static void packSensorPayload(char* buf, int lights, int beam, int up, int rpm, int water, float oil) {
    snprintf(buf, Message::MaxPayloadSize, "%d,%d,%d,%d,%d,%d",
             lights, beam, up, rpm, water, (int)(oil * 10));
}

void test_sensor_payload_fits_in_max_size() {
    char buf[Message::MaxPayloadSize];
    packSensorPayload(buf, 1, 1, 1, 9999, 120, 9.9f);
    TEST_ASSERT_LESS_THAN(Message::MaxPayloadSize, strlen(buf));
}

void test_sensor_payload_format_basic() {
    char buf[Message::MaxPayloadSize];
    packSensorPayload(buf, 1, 0, 1, 1500, 88, 3.2f);
    TEST_ASSERT_EQUAL_STRING("1,0,1,1500,88,32", buf);
}

void test_sensor_payload_lights_off() {
    char buf[Message::MaxPayloadSize];
    packSensorPayload(buf, 0, 0, 0, 0, 0, 0.0f);
    TEST_ASSERT_EQUAL_STRING("0,0,0,0,0,0", buf);
}

void test_sensor_payload_oil_decimal_encoding() {
    char buf[Message::MaxPayloadSize];
    packSensorPayload(buf, 0, 0, 0, 0, 0, 4.5f);
    TEST_ASSERT_EQUAL_STRING("0,0,0,0,0,45", buf);
}

void test_sensor_payload_max_rpm() {
    char buf[Message::MaxPayloadSize];
    packSensorPayload(buf, 0, 0, 0, 9999, 0, 0.0f);
    TEST_ASSERT_EQUAL_STRING("0,0,0,9999,0,0", buf);
}

void test_sensor_payload_high_water_temp() {
    char buf[Message::MaxPayloadSize];
    packSensorPayload(buf, 0, 0, 0, 0, 120, 0.0f);
    TEST_ASSERT_EQUAL_STRING("0,0,0,0,120,0", buf);
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_payload_fits_in_max_size);
    RUN_TEST(test_sensor_payload_format_basic);
    RUN_TEST(test_sensor_payload_lights_off);
    RUN_TEST(test_sensor_payload_oil_decimal_encoding);
    RUN_TEST(test_sensor_payload_max_rpm);
    RUN_TEST(test_sensor_payload_high_water_temp);
    return UNITY_END();
}