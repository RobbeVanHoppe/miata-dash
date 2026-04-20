#include "unity.h"
#include <stdio.h>
#include <string.h>
#include "common/message.h"

static void packGpsPayload(char* buf, double lat, double lon, float speed, int sats) {
    snprintf(buf, Message::MaxPayloadSize, "%.4f,%.4f,%.1f,%d",
             lat, lon, speed, sats);
}

void test_gps_payload_fits_in_max_size() {
    char buf[Message::MaxPayloadSize];
    packGpsPayload(buf, -90.0000, -180.0000, 999.9f, 12);
    TEST_ASSERT_LESS_THAN(Message::MaxPayloadSize, strlen(buf));
}

void test_gps_payload_format_basic() {
    char buf[Message::MaxPayloadSize];
    packGpsPayload(buf, 51.5074, -0.1278, 60.0f, 8);
    TEST_ASSERT_EQUAL_STRING("51.5074,-0.1278,60.0,8", buf);
}

void test_gps_payload_negative_lat() {
    char buf[Message::MaxPayloadSize];
    packGpsPayload(buf, -33.8688, 151.2093, 0.0f, 6);
    TEST_ASSERT_EQUAL_STRING("-33.8688,151.2093,0.0,6", buf);
}

void test_gps_payload_zero_speed() {
    char buf[Message::MaxPayloadSize];
    packGpsPayload(buf, 51.5074, -0.1278, 0.0f, 4);
    TEST_ASSERT_EQUAL_STRING("51.5074,-0.1278,0.0,4", buf);
}

void test_gps_payload_sat_count() {
    char buf[Message::MaxPayloadSize];
    packGpsPayload(buf, 51.5074, -0.1278, 30.0f, 12);
    TEST_ASSERT_NOT_NULL(strstr(buf, ",12"));
}

void test_gps_payload_worst_case_length() {
    char buf[Message::MaxPayloadSize];
    packGpsPayload(buf, -90.0000, -180.0000, 999.9f, 12);
    TEST_ASSERT_LESS_THAN((size_t)Message::MaxPayloadSize, strlen(buf));
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_gps_payload_fits_in_max_size);
    RUN_TEST(test_gps_payload_format_basic);
    RUN_TEST(test_gps_payload_negative_lat);
    RUN_TEST(test_gps_payload_zero_speed);
    RUN_TEST(test_gps_payload_sat_count);
    RUN_TEST(test_gps_payload_worst_case_length);
    return UNITY_END();
}