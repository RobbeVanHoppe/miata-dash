//
// Created by robbe on 05/04/2026.
//
#pragma once

#include <Arduino.h>
#include <Wire.h>

enum sensorPins {
    // ── Opto inputs ──────────────────────────────────────────
    OptoTachoPin    = 2,
    OptoLightPin    = 3,
    OptoBeamPin     = 4,
    OptoBrakePin    = 5,

    // ── Relay outputs ────────────────────────────────────────
    RelayAccPin         = 6,
    RelayRetractUpPin   = 7,
    RelayRetractDownPin = 8,
    RelayBeamPin        = 9,
    RelayLightsPin      = 10,
    RelayInteriorPin    = 11,

    // ── ADC inputs ───────────────────────────────────────────
    ADCOilPin   = A0,   // oil pressure sender via LM358 buffer, 180Ω ref
    ADCWaterPin = A1,   // water temp sender via LM358 buffer, 330Ω ref

    // ── Remaining free pins (for future use) ─────────────────
    // 12 (MISO), 13 (SCK) — usable as GPIO,
    // reserve for horn / hazard  if relay module expanded
};

#include "main.h"

class SensorForwarder : public BusNode {
public:
    SensorForwarder() : BusNode(NODE_SENSOR_ARDUINO) {}

    void begin() override;
    void update();

    void handleI2CRequest() { this->onI2CRequest(); }
    void handleI2CReceive(int len);

private:
    // Retractor stall protection
    uint32_t retractorTimer_   = 0;
    bool     retractorRunning_ = false;
    bool     lightsUp_         = false;

    void processCommand(const Message& msg);
};