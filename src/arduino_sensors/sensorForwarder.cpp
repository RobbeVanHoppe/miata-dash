#include "arduino_sensors/sensorForwarder.h"
#include "common/carState.h"

#define WAT_T_RESISTOR_VALUE 330.00
#define OIL_P_RESISTOR_VALUE 180.62

static SensorForwarder* instance = nullptr;
static CarState sensorNodeState;

volatile unsigned long pulseCount = 0;

void tachoISR() {
    pulseCount++;
}

// --- ANALOG SENSOR MATH ---
int readWaterTemp() {
    int raw = analogRead(ADCWaterPin);
    if (raw < 10 || raw > 1010) return 0;

    float voltage = raw * (5.0 / 1023.0);
    float rSensor = (voltage * WAT_T_RESISTOR_VALUE) / (5.0 - voltage);

    // Steinhart-Hart coefficients derived from Mazda B6 coolant sensor
    // Source: 1990 MX-5 Workshop Manual p.F-136
    // Anchor points: -20°C=16200Ω, 20°C=2450Ω, 80°C=320Ω
    float lnR = log(rSensor);
    float tempK = 1.0 / (0.0011934494 + (0.0002837733 * lnR) + (0.000000006840434 * lnR * lnR * lnR));

    if (tempK < 233.15 || tempK > 393.15) return 0;
    else return (int)(tempK - 273.15);
}

float readOilPressure() {
    int raw = analogRead(ADCOilPin);
    if (raw < 10) return 0.0;

    float voltage = raw * (5.0 / 1023.0);
    float rSensor = (voltage * OIL_P_RESISTOR_VALUE) / (5.0 - voltage);

    // Miata NA Sender: ~154R (0 PSI) to ~28R (90 PSI)
    float psi = (rSensor - 154.0) / -1.4;
    if (psi < 0) psi = 0;

    return psi * 0.0689476; // PSI to BAR
}

int calculateRPMs() {
    static uint32_t lastRpmCalc = 0;
    static int currentRPM = 0;
    uint32_t now = millis();

    if (now - lastRpmCalc >= 250) {
        uint32_t elapsed = now - lastRpmCalc;
        noInterrupts();
        unsigned long pulses = pulseCount;
        pulseCount = 0;
        interrupts();

        float deltaSeconds = elapsed / 1000.0;
        float rpmScopePeriod = 1000.0 / 32.0; // Scope reads 32Hz @ 1000RPM
        if (pulses > 0) {
            currentRPM = (int)((pulses / deltaSeconds) * rpmScopePeriod);
        } else {
            currentRPM = 0;
        }
        lastRpmCalc = now;
    }
    return currentRPM;
}

void SensorForwarder::begin() {
    instance = this;
    Serial.begin(115200);

    analogReference(DEFAULT);

    // ── Relay outputs — all start HIGH (relay off, active low) ──
    const uint8_t relayPins[] = {
            RelayLightsPin, RelayBeamPin,
            RelayRetractUpPin, RelayRetractDownPin,
            RelayAccPin, RelayInteriorPin
    };
    for (uint8_t pin : relayPins) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }

    // ── Opto inputs ──────────────────────────────────────────────
    const uint8_t optoPins[] = {
            OptoLightPin, OptoBeamPin, OptoBrakePin
            // OptoTachoPin handled separately below (interrupt)
    };
    for (uint8_t pin : optoPins) {
        pinMode(pin, INPUT_PULLUP);
    }

    // ── Tacho interrupt ──────────────────────────────────────────
    pinMode(OptoTachoPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(OptoTachoPin), tachoISR, FALLING);

    // ── I2C ──────────────────────────────────────────────────────
    Wire.begin(nodeToAddress(nodeType_));
    Wire.onRequest([](){ if (instance) instance->onI2CRequest(); });
    Wire.onReceive([](int len){ if (instance) instance->handleI2CReceive(len); });
}

void SensorForwarder::update() {
    // 1. Digital/Pulse Updates
    sensorNodeState.RPM = calculateRPMs();
    sensorNodeState.lightsOn  = (digitalRead(OptoLightPin) == LOW);
    sensorNodeState.beamOn    = (digitalRead(OptoBeamPin)  == LOW);
    sensorNodeState.parkingBrakeOn = (digitalRead(OptoBrakePin) == LOW);
    sensorNodeState.lightsUp = lightsUp_;

    // 2. Analog Updates
    sensorNodeState.OilPressure = readOilPressure();
    sensorNodeState.WaterTempC  = readWaterTemp();

    // 3. Pack I2C Message — positional CSV, oil*10 as int to save bytes
    // Format: "lights,beam,up,rpm,water,oil*10"
    // Example: "1,0,1,1500,88,32"  (3.2 bar -> 32)
    readyMsg.type   = TYPE_DATA;
    readyMsg.source = nodeType_;
    snprintf(readyMsg.payload, Message::MaxPayloadSize, "%d,%d,%d,%d,%d,%d",
             sensorNodeState.lightsOn,
             sensorNodeState.beamOn,
             sensorNodeState.lightsUp,
             sensorNodeState.RPM,
             sensorNodeState.WaterTempC,
             (int)(sensorNodeState.OilPressure * 10));

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 1000) {
        Serial.print("RPM: ");   Serial.print(sensorNodeState.RPM);
        Serial.print(" | Oil: "); Serial.print(sensorNodeState.OilPressure);
        Serial.print(" | Water: "); Serial.println(sensorNodeState.WaterTempC);
        Serial.print("Payload: "); Serial.println(readyMsg.payload);
        lastPrint = millis();
    }

    // Retractor stall protection — cut motor after 1500ms
    if (retractorRunning_ && (millis() - retractorTimer_ > 1500)) {
        digitalWrite(RelayRetractUpPin,   HIGH);
        digitalWrite(RelayRetractDownPin, HIGH);
        retractorRunning_ = false;
    }
}

void SensorForwarder::handleI2CReceive(int len) {
    Message msg;
    if (receiveToStruct(msg)) {
        if (msg.type == TYPE_COMMAND) {
            processCommand(msg);
        }
    }
}

void SensorForwarder::processCommand(const Message& msg) {
    // ── Lighting ──────────────────────────────────────────────
    if      (strcmp(msg.payload, "LIGHTS_ON")  == 0) {
        digitalWrite(RelayLightsPin, LOW);
    }
    else if (strcmp(msg.payload, "LIGHTS_OFF") == 0) {
        digitalWrite(RelayLightsPin, HIGH);
        digitalWrite(RelayBeamPin,   HIGH);   // beam can't stay on without lights
    }
    else if (strcmp(msg.payload, "BEAM_ON")    == 0) digitalWrite(RelayBeamPin, LOW);
    else if (strcmp(msg.payload, "BEAM_OFF")   == 0) digitalWrite(RelayBeamPin, HIGH);

        // ── Retractor (H-bridge: two relay pins, mutually exclusive) ──
    else if (strcmp(msg.payload, "RETRACT_UP") == 0) {
        digitalWrite(RelayRetractDownPin, HIGH);  // ensure reverse is off first
        digitalWrite(RelayRetractUpPin,   LOW);
        retractorTimer_ = millis();               // start stall-protection timer
        retractorRunning_ = true;
        lightsUp_         = true;
    }
    else if (strcmp(msg.payload, "RETRACT_DOWN") == 0) {
        digitalWrite(RelayRetractUpPin,   HIGH);
        digitalWrite(RelayRetractDownPin, LOW);
        retractorTimer_ = millis();
        retractorRunning_ = true;
        lightsUp_         = false;
    }

        // ── ACC bus ───────────────────────────────────────────────
    else if (strcmp(msg.payload, "ACC_ON")  == 0) digitalWrite(RelayAccPin, LOW);
    else if (strcmp(msg.payload, "ACC_OFF") == 0) digitalWrite(RelayAccPin, HIGH);

        // ── Interior light ────────────────────────────────────────
    else if (strcmp(msg.payload, "INTERIOR_ON")  == 0) digitalWrite(RelayInteriorPin, LOW);
    else if (strcmp(msg.payload, "INTERIOR_OFF") == 0) digitalWrite(RelayInteriorPin, HIGH);
}
