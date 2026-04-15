#include "arduino_sensors/sensorForwarder.h"
#include "common/carState.h"

#define WAT_T_RESISTOR_VALUE 180.63
#define OIL_P_RESISTOR_VALUE 180.52

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

    // 1. Calculate Sensor Resistance
    float voltage = raw * (5.0 / 1023.0);
    float rSensor = (voltage * WAT_T_RESISTOR_VALUE) / (5.0 - voltage);

    // 2. Steinhart-Hart / Beta Equation for Miata Sensor
    // These constants are tuned for the standard Mazda B6/BP coolant sensor
    float temperature;
    temperature = log(rSensor);
    temperature = 1 / (0.001129148 + (0.000234125 * temperature) + (0.0000000876741 * temperature * temperature * temperature));
    temperature = temperature - 273.15; // Convert Kelvin to Celsius

    return (int)temperature;
}

float readOilPressure() {
    int raw = analogRead(ADCOilPin);
    Serial.println(raw);
    if (raw < 10) return 0.0; // Wire disconnected or shorted

    // Voltage Divider: 5V -> 150R -> A0 -> Sensor -> GND
    float voltage = raw * (5.0 / 1023.0);
    float rSensor = (voltage * OIL_P_RESISTOR_VALUE) / (5.0 - voltage);

    // Miata NA Sender: ~154R (0 PSI) to ~28R (90 PSI)
    // Formula derived from sensor curve: PSI = (Resistance - 154) / -1.4
    float psi = (rSensor - 154.0) / -1.4;
    if (psi < 0) psi = 0;

    return psi * 0.0689476; // Convert PSI to BAR
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
        // Scope reads 32Hz @ 1000RPM
        float rpmScopePeriod = 1000.0 / 32.0;
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

    // Set Analog Reference to default (5V)
    analogReference(DEFAULT);

    pinMode(RelayLightsPin, OUTPUT);
    pinMode(RelayRetractorPin, OUTPUT);
    pinMode(RelayBeamPin, OUTPUT);
    digitalWrite(RelayLightsPin, HIGH);
    digitalWrite(RelayRetractorPin, HIGH);
    digitalWrite(RelayBeamPin, HIGH);

    pinMode(OptoTachoPin, INPUT_PULLUP);
    pinMode(OptoLightPin, INPUT_PULLUP);
    pinMode(OptoBeamPin, INPUT_PULLUP);
    pinMode(OptoBrakePin, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(OptoTachoPin), tachoISR, FALLING);

    Wire.begin(nodeToAddress(nodeType_));
    Wire.onRequest([](){ if (instance) instance->onI2CRequest(); });
    Wire.onReceive([](int len){ if (instance) instance->handleI2CReceive(len); });
}

void SensorForwarder::update() {
    // 1. Digital/Pulse Updates
    sensorNodeState.RPM = calculateRPMs();
    sensorNodeState.lightsOn = (digitalRead(OptoLightPin) == LOW);
    sensorNodeState.beamOn = (digitalRead(OptoBeamPin) == LOW);
    sensorNodeState.parkingBrakeOn = (digitalRead(OptoBrakePin) == LOW);
    sensorNodeState.lightsUp = 1; // Logic for retractor relay state

    // 2. Analog Updates
    sensorNodeState.OilPressure = readOilPressure();
    sensorNodeState.WaterTempC = readWaterTemp();

    // 3. Pack I2C Message
    readyMsg.type = TYPE_DATA;
    readyMsg.source = nodeType_;
    snprintf(readyMsg.payload, Message::MaxPayloadSize, "L:%d,B:%d,U:%d,R:%d,W:%d,O:%.1f",
             sensorNodeState.lightsOn,
             sensorNodeState.beamOn,
             sensorNodeState.lightsUp,
             sensorNodeState.RPM,
             sensorNodeState.WaterTempC,
             sensorNodeState.OilPressure);

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 1000) {
        Serial.print("RPM: "); Serial.print(sensorNodeState.RPM);
        Serial.print(" | Oil: "); Serial.print(sensorNodeState.OilPressure);
        Serial.print(" | Water: "); Serial.println(sensorNodeState.WaterTempC);
        lastPrint = millis();
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
    if (strcmp(msg.payload, "RELAY_RETRACT_UP") == 0) digitalWrite(RelayRetractorPin, LOW);
    else if (strcmp(msg.payload, "RELAY_RETRACT_DOWN") == 0) digitalWrite(RelayRetractorPin, HIGH);
    else if (strcmp(msg.payload, "RELAY_LIGHTS_ON") == 0) digitalWrite(RelayLightsPin, LOW);
    else if (strcmp(msg.payload, "RELAY_BEAM_ON") == 0) digitalWrite(RelayBeamPin, LOW);
    else if (strcmp(msg.payload, "RELAY_BEAM_OFF") == 0) digitalWrite(RelayBeamPin, HIGH);
    else if (strcmp(msg.payload, "RELAY_LIGHTS_OFF") == 0) {
        digitalWrite(RelayLightsPin, HIGH);
        digitalWrite(RelayBeamPin, HIGH);
    }
}