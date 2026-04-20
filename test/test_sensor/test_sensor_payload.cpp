#include "unity.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "common/message.h"

// ─── mirror the production constants ────────────────────────────────────────
#define WAT_T_RESISTOR_VALUE 330.0f
#define OIL_P_RESISTOR_VALUE 180.0f
#define VCC 5.0f
#define ADC_MAX 1023.0f

// ─── mirror production functions (pure math, no Arduino deps) ───────────────

static float adcToRSensor(int raw, float rRef) {
    float voltage = raw * (VCC / ADC_MAX);
    return (rRef * voltage) / (VCC - voltage);
}

static int rawToWaterTemp(int raw) {
    if (raw < 10 || raw > 1010) return 0;
    float rSensor = adcToRSensor(raw, WAT_T_RESISTOR_VALUE);
    float lnR = logf(rSensor);
    float tempK = 1.0f / (0.0011934494f + (0.0002837733f * lnR) + (0.000000006840434f * lnR * lnR * lnR));
    return (int)(tempK - 273.15f);
}

static float rawToOilPressure(int raw) {
    if (raw < 10) return 0.0f;
    float rSensor = adcToRSensor(raw, OIL_P_RESISTOR_VALUE);
    float psi = (rSensor - 154.0f) / -1.4f;
    if (psi < 0) psi = 0;
    return psi * 0.0689476f;
}

static void packPayload(char* buf, int lights, int beam, int up, int rpm, int water, float oil) {
    snprintf(buf, Message::MaxPayloadSize, "%d,%d,%d,%d,%d,%d",
             lights, beam, up, rpm, water, (int)(oil * 10));
}

// ─── voltage divider math ────────────────────────────────────────────────────

// Given a known sensor resistance, what ADC raw value should we expect?
// Inverse of adcToRSensor — lets us synthesise test inputs from manual spec values
static int rSensorToRaw(float rSensor, float rRef) {
    // V = Vcc * rSensor / (rRef + rSensor)  (sensor to GND, rRef to Vcc)
    float voltage = VCC * rSensor / (rRef + rSensor);
    return (int)(voltage * ADC_MAX / VCC);
}

// ─── water temp tests ────────────────────────────────────────────────────────

// Manual page F-136: three anchor points the coefficients were derived from
void test_water_temp_minus20c() {
    // R = 16200Ω at -20°C
    int raw = rSensorToRaw(16200.0f, WAT_T_RESISTOR_VALUE);
    int result = rawToWaterTemp(raw);
    TEST_ASSERT_INT_WITHIN(2, -20, result);
}

void test_water_temp_20c() {
    // R = 2450Ω at 20°C
    int raw = rSensorToRaw(2450.0f, WAT_T_RESISTOR_VALUE);
    int result = rawToWaterTemp(raw);
    TEST_ASSERT_INT_WITHIN(2, 20, result);
}

void test_water_temp_80c() {
    // R = 320Ω at 80°C — normal operating temp
    int raw = rSensorToRaw(320.0f, WAT_T_RESISTOR_VALUE);
    int result = rawToWaterTemp(raw);
    TEST_ASSERT_INT_WITHIN(2, 80, result);
}

void test_water_temp_normal_running() {
    // ~90°C is a typical fully-warmed engine — should read between 85 and 95
    // R roughly 230Ω at 90°C (extrapolated from curve)
    int raw = rSensorToRaw(230.0f, WAT_T_RESISTOR_VALUE);
    int result = rawToWaterTemp(raw);
    TEST_ASSERT_INT_WITHIN(5, 90, result);
}

void test_water_temp_open_circuit_returns_zero() {
    // raw < 10 = open circuit / disconnected sensor
    TEST_ASSERT_EQUAL_INT(0, rawToWaterTemp(0));
    TEST_ASSERT_EQUAL_INT(0, rawToWaterTemp(9));
}

void test_water_temp_short_circuit_returns_zero() {
    // raw > 1010 = sensor shorted to GND
    TEST_ASSERT_EQUAL_INT(0, rawToWaterTemp(1011));
    TEST_ASSERT_EQUAL_INT(0, rawToWaterTemp(1023));
}

// ─── oil pressure tests ──────────────────────────────────────────────────────

// Manual page T-32: sender is ~154Ω at 0 PSI, ~16Ω at 90 PSI (28Ω per the
// linear mapping in production code — using production constants here)

void test_oil_pressure_zero_psi() {
    // R = 154Ω → 0 PSI → 0.0 bar
    int raw = rSensorToRaw(154.0f, OIL_P_RESISTOR_VALUE);
    float result = rawToOilPressure(raw);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, result);
}

void test_oil_pressure_idle_range() {
    // ~30 PSI at idle is normal — roughly 111Ω on the sender
    // 30 PSI = 2.07 bar
    int raw = rSensorToRaw(111.0f, OIL_P_RESISTOR_VALUE);
    float result = rawToOilPressure(raw);
    TEST_ASSERT_FLOAT_WITHIN(0.15f, 2.07f, result);
}

void test_oil_pressure_max_90psi() {
    // R = 28Ω → 90 PSI → 6.21 bar
    int raw = rSensorToRaw(28.0f, OIL_P_RESISTOR_VALUE);
    float result = rawToOilPressure(raw);
    TEST_ASSERT_FLOAT_WITHIN(0.15f, 6.21f, result);
}

void test_oil_pressure_below_threshold_returns_zero() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, rawToOilPressure(0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, rawToOilPressure(9));
}

void test_oil_pressure_clamps_at_zero() {
    // Resistance above 154Ω (e.g. fault) should not return negative pressure
    int raw = rSensorToRaw(200.0f, OIL_P_RESISTOR_VALUE);
    float result = rawToOilPressure(raw);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, result);
}

// ─── payload format tests (kept, still valid) ────────────────────────────────

void test_payload_fits_in_max_size() {
    char buf[Message::MaxPayloadSize];
    packPayload(buf, 1, 1, 1, 9999, 120, 9.9f);
    TEST_ASSERT_LESS_THAN(Message::MaxPayloadSize, strlen(buf));
}

void test_payload_format_basic() {
    char buf[Message::MaxPayloadSize];
    packPayload(buf, 1, 0, 1, 1500, 88, 3.2f);
    TEST_ASSERT_EQUAL_STRING("1,0,1,1500,88,32", buf);
}

void test_payload_all_zero() {
    char buf[Message::MaxPayloadSize];
    packPayload(buf, 0, 0, 0, 0, 0, 0.0f);
    TEST_ASSERT_EQUAL_STRING("0,0,0,0,0,0", buf);
}

void test_payload_oil_decimal_encoding() {
    char buf[Message::MaxPayloadSize];
    packPayload(buf, 0, 0, 0, 0, 0, 4.5f);
    TEST_ASSERT_EQUAL_STRING("0,0,0,0,0,45", buf);
}

// ─── runner ──────────────────────────────────────────────────────────────────

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();

    // Water temp — manual anchor points (page F-136)
    RUN_TEST(test_water_temp_minus20c);
    RUN_TEST(test_water_temp_20c);
    RUN_TEST(test_water_temp_80c);
    // Water temp — operational
    RUN_TEST(test_water_temp_normal_running);
    RUN_TEST(test_water_temp_open_circuit_returns_zero);
    RUN_TEST(test_water_temp_short_circuit_returns_zero);

    // Oil pressure — manual anchor points (page T-32)
    RUN_TEST(test_oil_pressure_zero_psi);
    RUN_TEST(test_oil_pressure_idle_range);
    RUN_TEST(test_oil_pressure_max_90psi);
    RUN_TEST(test_oil_pressure_below_threshold_returns_zero);
    RUN_TEST(test_oil_pressure_clamps_at_zero);

    // Payload format
    RUN_TEST(test_payload_fits_in_max_size);
    RUN_TEST(test_payload_format_basic);
    RUN_TEST(test_payload_all_zero);
    RUN_TEST(test_payload_oil_decimal_encoding);

    return UNITY_END();
}