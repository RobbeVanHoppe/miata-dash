#include "unity.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "common/message.h"

// ─── Arduino mock ────────────────────────────────────────────────────────────

static uint8_t pinModes[20]   = {};
static uint8_t pinStates[20]  = {};
static uint32_t mockMillis    = 0;

void pinMode(uint8_t pin, uint8_t mode)       { pinModes[pin]  = mode;  }
void digitalWrite(uint8_t pin, uint8_t state) { pinStates[pin] = state; }
uint8_t digitalRead(uint8_t pin)              { return pinStates[pin];  }
uint32_t millis()                             { return mockMillis;       }

#define HIGH 1
#define LOW  0
#define OUTPUT 1
#define INPUT_PULLUP 2

// ─── Pin definitions (mirror header) ─────────────────────────────────────────

enum sensorPins {
    RelayAccPin         = 6,
    RelayRetractUpPin   = 7,
    RelayRetractDownPin = 8,
    RelayBeamPin        = 9,
    RelayLightsPin      = 10,
    RelayInteriorPin    = 11,
};

// ─── Mirror production constants ─────────────────────────────────────────────

#define WAT_T_RESISTOR_VALUE 330.00f
#define OIL_P_RESISTOR_VALUE 180.62f
#define VCC      5.0f
#define ADC_MAX  1023.0f

// ─── Mirror production functions ─────────────────────────────────────────────

static float adcToRSensor(int raw, float rRef) {
    float voltage = raw * (VCC / ADC_MAX);
    return (rRef * voltage) / (VCC - voltage);
}

static int rawToWaterTemp(int raw) {
    if (raw < 10 || raw > 1010) return 0;
    float rSensor = adcToRSensor(raw, WAT_T_RESISTOR_VALUE);
    float lnR = logf(rSensor);
    float tempK = 1.0f / (0.0011934494f + (0.0002837733f * lnR) +
                          (0.000000006840434f * lnR * lnR * lnR));
    if (tempK < 233.15f || tempK > 393.15f) return 0;
    return (int)(tempK - 273.15f);
}

static float rawToOilPressure(int raw) {
    if (raw < 10) return 0.0f;
    float rSensor = adcToRSensor(raw, OIL_P_RESISTOR_VALUE);
    float psi = (rSensor - 154.0f) / -1.4f;
    if (psi < 0) psi = 0;
    return psi * 0.0689476f;
}

static int rSensorToRaw(float rSensor, float rRef) {
    float voltage = VCC * rSensor / (rRef + rSensor);
    return (int)(voltage * ADC_MAX / VCC);
}

// ─── Minimal command dispatcher (mirrors processCommand logic) ────────────────

static bool  retractorRunning = false;
static bool  lightsUp         = false;
static uint32_t retractorTimer = 0;

static void resetRelays() {
    for (uint8_t pin = 6; pin <= 11; pin++)
        pinStates[pin] = HIGH;
    retractorRunning = false;
    lightsUp         = false;
    retractorTimer   = 0;
    mockMillis       = 0;
}

static void dispatchCommand(const char* cmd) {
    if      (strcmp(cmd, "LIGHTS_ON")  == 0) {
        pinStates[RelayLightsPin] = LOW;
    }
    else if (strcmp(cmd, "LIGHTS_OFF") == 0) {
        pinStates[RelayLightsPin] = HIGH;
        pinStates[RelayBeamPin]   = HIGH;
    }
    else if (strcmp(cmd, "BEAM_ON")    == 0) pinStates[RelayBeamPin] = LOW;
    else if (strcmp(cmd, "BEAM_OFF")   == 0) pinStates[RelayBeamPin] = HIGH;
    else if (strcmp(cmd, "RETRACT_UP") == 0) {
        pinStates[RelayRetractDownPin] = HIGH;
        pinStates[RelayRetractUpPin]   = LOW;
        retractorTimer   = mockMillis;
        retractorRunning = true;
        lightsUp         = true;
    }
    else if (strcmp(cmd, "RETRACT_DOWN") == 0) {
        pinStates[RelayRetractUpPin]   = HIGH;
        pinStates[RelayRetractDownPin] = LOW;
        retractorTimer   = mockMillis;
        retractorRunning = true;
        lightsUp         = false;
    }
    else if (strcmp(cmd, "ACC_ON")       == 0) pinStates[RelayAccPin]      = LOW;
    else if (strcmp(cmd, "ACC_OFF")      == 0) pinStates[RelayAccPin]      = HIGH;
    else if (strcmp(cmd, "INTERIOR_ON")  == 0) pinStates[RelayInteriorPin] = LOW;
    else if (strcmp(cmd, "INTERIOR_OFF") == 0) pinStates[RelayInteriorPin] = HIGH;
}

// Mirrors the stall-protection check in update()
static void tickUpdate() {
    if (retractorRunning && (mockMillis - retractorTimer > 1500)) {
        pinStates[RelayRetractUpPin]   = HIGH;
        pinStates[RelayRetractDownPin] = HIGH;
        retractorRunning = false;
    }
}

// ─── Lighting tests ───────────────────────────────────────────────────────────

void test_lights_on_energises_relay() {
    resetRelays();
    dispatchCommand("LIGHTS_ON");
    TEST_ASSERT_EQUAL(LOW, pinStates[RelayLightsPin]);
}

void test_lights_off_de_energises_relay() {
    resetRelays();
    dispatchCommand("LIGHTS_ON");
    dispatchCommand("LIGHTS_OFF");
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayLightsPin]);
}

void test_lights_off_also_kills_beam() {
    resetRelays();
    dispatchCommand("LIGHTS_ON");
    dispatchCommand("BEAM_ON");
    TEST_ASSERT_EQUAL(LOW, pinStates[RelayBeamPin]);
    dispatchCommand("LIGHTS_OFF");
    // Both must be off — beam cannot survive without lights
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayLightsPin]);
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayBeamPin]);
}

void test_beam_on_independent_of_lights_relay() {
    resetRelays();
    dispatchCommand("BEAM_ON");
    TEST_ASSERT_EQUAL(LOW, pinStates[RelayBeamPin]);
}

void test_beam_off() {
    resetRelays();
    dispatchCommand("BEAM_ON");
    dispatchCommand("BEAM_OFF");
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayBeamPin]);
}

// ─── Retractor tests ──────────────────────────────────────────────────────────

void test_retract_up_energises_correct_relay() {
    resetRelays();
    dispatchCommand("RETRACT_UP");
    TEST_ASSERT_EQUAL(LOW,  pinStates[RelayRetractUpPin]);
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayRetractDownPin]);
}

void test_retract_down_energises_correct_relay() {
    resetRelays();
    dispatchCommand("RETRACT_DOWN");
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayRetractUpPin]);
    TEST_ASSERT_EQUAL(LOW,  pinStates[RelayRetractDownPin]);
}

void test_retract_up_sets_lightsup_true() {
    resetRelays();
    dispatchCommand("RETRACT_UP");
    TEST_ASSERT_TRUE(lightsUp);
}

void test_retract_down_sets_lightsup_false() {
    resetRelays();
    dispatchCommand("RETRACT_DOWN");
    TEST_ASSERT_FALSE(lightsUp);
}

void test_retract_direction_change_turns_off_previous_relay() {
    // Going up then immediately down must not leave both relays on
    resetRelays();
    dispatchCommand("RETRACT_UP");
    dispatchCommand("RETRACT_DOWN");
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayRetractUpPin]);
    TEST_ASSERT_EQUAL(LOW,  pinStates[RelayRetractDownPin]);
}

void test_retractor_stall_protection_cuts_motor_after_1500ms() {
    resetRelays();
    mockMillis = 0;
    dispatchCommand("RETRACT_UP");
    TEST_ASSERT_TRUE(retractorRunning);

    mockMillis = 1499;
    tickUpdate();
    TEST_ASSERT_EQUAL(LOW, pinStates[RelayRetractUpPin]);  // still running
    TEST_ASSERT_TRUE(retractorRunning);

    mockMillis = 1501;
    tickUpdate();
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayRetractUpPin]);
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayRetractDownPin]);
    TEST_ASSERT_FALSE(retractorRunning);
}

void test_retractor_stall_protection_does_not_trigger_early() {
    resetRelays();
    mockMillis = 1000;
    dispatchCommand("RETRACT_DOWN");

    mockMillis = 2499;  // 1499ms elapsed, not yet
    tickUpdate();
    TEST_ASSERT_EQUAL(LOW, pinStates[RelayRetractDownPin]);
    TEST_ASSERT_TRUE(retractorRunning);
}

// ─── ACC / interior tests ─────────────────────────────────────────────────────

void test_acc_on() {
    resetRelays();
    dispatchCommand("ACC_ON");
    TEST_ASSERT_EQUAL(LOW, pinStates[RelayAccPin]);
}

void test_acc_off() {
    resetRelays();
    dispatchCommand("ACC_ON");
    dispatchCommand("ACC_OFF");
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayAccPin]);
}

void test_interior_on() {
    resetRelays();
    dispatchCommand("INTERIOR_ON");
    TEST_ASSERT_EQUAL(LOW, pinStates[RelayInteriorPin]);
}

void test_interior_off() {
    resetRelays();
    dispatchCommand("INTERIOR_ON");
    dispatchCommand("INTERIOR_OFF");
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayInteriorPin]);
}

// ─── Relay init state tests ───────────────────────────────────────────────────

void test_all_relays_start_high() {
    resetRelays();
    // After reset (mirrors begin()), all relay pins must be HIGH (off)
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayLightsPin]);
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayBeamPin]);
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayRetractUpPin]);
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayRetractDownPin]);
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayAccPin]);
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayInteriorPin]);
}

void test_commands_do_not_cross_contaminate() {
    // Triggering one relay must not affect unrelated relays
    resetRelays();
    dispatchCommand("ACC_ON");
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayLightsPin]);
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayBeamPin]);
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayRetractUpPin]);
    TEST_ASSERT_EQUAL(HIGH, pinStates[RelayInteriorPin]);
}

// ─── Sensor math tests (carried over, updated constants) ─────────────────────

void test_water_temp_minus20c() {
    TEST_ASSERT_INT_WITHIN(2, -20, rawToWaterTemp(rSensorToRaw(16200.0f, WAT_T_RESISTOR_VALUE)));
}

void test_water_temp_20c() {
    TEST_ASSERT_INT_WITHIN(2, 20, rawToWaterTemp(rSensorToRaw(2450.0f, WAT_T_RESISTOR_VALUE)));
}

void test_water_temp_80c() {
    TEST_ASSERT_INT_WITHIN(2, 80, rawToWaterTemp(rSensorToRaw(320.0f, WAT_T_RESISTOR_VALUE)));
}

void test_water_temp_open_circuit_returns_zero() {
    TEST_ASSERT_EQUAL_INT(0, rawToWaterTemp(0));
    TEST_ASSERT_EQUAL_INT(0, rawToWaterTemp(9));
}

void test_water_temp_short_circuit_returns_zero() {
    TEST_ASSERT_EQUAL_INT(0, rawToWaterTemp(1011));
    TEST_ASSERT_EQUAL_INT(0, rawToWaterTemp(1023));
}

void test_oil_pressure_zero_psi() {
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, rawToOilPressure(rSensorToRaw(154.0f, OIL_P_RESISTOR_VALUE)));
}

void test_oil_pressure_max_90psi() {
    TEST_ASSERT_FLOAT_WITHIN(0.15f, 6.21f, rawToOilPressure(rSensorToRaw(28.0f, OIL_P_RESISTOR_VALUE)));
}

void test_oil_pressure_clamps_at_zero() {
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, rawToOilPressure(rSensorToRaw(200.0f, OIL_P_RESISTOR_VALUE)));
}

// ─── Runner ───────────────────────────────────────────────────────────────────

void setUp(void)    { resetRelays(); }
void tearDown(void) {}

int main() {
    UNITY_BEGIN();

    // Relay init
    RUN_TEST(test_all_relays_start_high);
    RUN_TEST(test_commands_do_not_cross_contaminate);

    // Lighting
    RUN_TEST(test_lights_on_energises_relay);
    RUN_TEST(test_lights_off_de_energises_relay);
    RUN_TEST(test_lights_off_also_kills_beam);
    RUN_TEST(test_beam_on_independent_of_lights_relay);
    RUN_TEST(test_beam_off);

    // Retractor
    RUN_TEST(test_retract_up_energises_correct_relay);
    RUN_TEST(test_retract_down_energises_correct_relay);
    RUN_TEST(test_retract_up_sets_lightsup_true);
    RUN_TEST(test_retract_down_sets_lightsup_false);
    RUN_TEST(test_retract_direction_change_turns_off_previous_relay);
    RUN_TEST(test_retractor_stall_protection_cuts_motor_after_1500ms);
    RUN_TEST(test_retractor_stall_protection_does_not_trigger_early);

    // ACC / interior
    RUN_TEST(test_acc_on);
    RUN_TEST(test_acc_off);
    RUN_TEST(test_interior_on);
    RUN_TEST(test_interior_off);

    // Sensor math
    RUN_TEST(test_water_temp_minus20c);
    RUN_TEST(test_water_temp_20c);
    RUN_TEST(test_water_temp_80c);
    RUN_TEST(test_water_temp_open_circuit_returns_zero);
    RUN_TEST(test_water_temp_short_circuit_returns_zero);
    RUN_TEST(test_oil_pressure_zero_psi);
    RUN_TEST(test_oil_pressure_max_90psi);
    RUN_TEST(test_oil_pressure_clamps_at_zero);

    return UNITY_END();
}