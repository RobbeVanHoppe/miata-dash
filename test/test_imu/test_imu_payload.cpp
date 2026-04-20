#include "unity.h"
#include <stdio.h>
#include <string.h>
#include "common/message.h"

static void packImuPayload(char* buf, float x, float y, float z) {
    snprintf(buf, Message::MaxPayloadSize, "%.2f,%.2f,%.2f",
             (double)x, (double)y, (double)z);
}

void test_imu_payload_fits_in_max_size() {
    char buf[Message::MaxPayloadSize];
    packImuPayload(buf, -9.99f, -9.99f, -9.99f);
    TEST_ASSERT_LESS_THAN(Message::MaxPayloadSize, strlen(buf));
}

void test_imu_payload_format_basic() {
    char buf[Message::MaxPayloadSize];
    packImuPayload(buf, 0.02f, 0.98f, 0.15f);
    TEST_ASSERT_EQUAL_STRING("0.02,0.98,0.15", buf);
}

void test_imu_payload_negative_values() {
    char buf[Message::MaxPayloadSize];
    packImuPayload(buf, -0.50f, -1.00f, 0.00f);
    TEST_ASSERT_EQUAL_STRING("-0.50,-1.00,0.00", buf);
}

void test_imu_payload_zero_g() {
    char buf[Message::MaxPayloadSize];
    packImuPayload(buf, 0.00f, 0.00f, 0.00f);
    TEST_ASSERT_EQUAL_STRING("0.00,0.00,0.00", buf);
}

void test_imu_payload_high_g() {
    char buf[Message::MaxPayloadSize];
    packImuPayload(buf, 2.50f, -3.10f, 1.00f);
    TEST_ASSERT_EQUAL_STRING("2.50,-3.10,1.00", buf);
}

void test_imu_payload_two_decimal_precision() {
    char buf[Message::MaxPayloadSize];
    packImuPayload(buf, 0.129f, 0.125f, 0.999f);
    TEST_ASSERT_NOT_NULL(strstr(buf, "0.13"));
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_imu_payload_fits_in_max_size);
    RUN_TEST(test_imu_payload_format_basic);
    RUN_TEST(test_imu_payload_negative_values);
    RUN_TEST(test_imu_payload_zero_g);
    RUN_TEST(test_imu_payload_high_g);
    RUN_TEST(test_imu_payload_two_decimal_precision);
    return UNITY_END();
}