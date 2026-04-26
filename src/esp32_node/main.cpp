#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* SSID    = "miotspot";
const char* RPI_URL = "http://192.168.4.1:5000/api/esp32/data";

// ── Timing ────────────────────────────────────────────────────────────────────
// Post to RPi at 5Hz — enough for a smooth dashboard, half the previous load.
// Serial log only when values change meaningfully, or every LOG_INTERVAL_MS.
static const uint32_t POST_INTERVAL_MS = 200;   // was delay(100) = 10Hz
static const uint32_t LOG_INTERVAL_MS  = 5000;  // max one log line per 5s

// ── Mock state ────────────────────────────────────────────────────────────────
float mockRpm   = 0;
float mockWater = 20;
float mockOil   = 0;
bool  lightsOn  = false;
bool  lightsUp  = false;

// ── Last-posted values — used to detect meaningful change ─────────────────────
static int   lastPostedRpm   = -1;
static int   lastPostedWater = -1;
static float lastPostedOil   = -1.0f;

// ── Timers ────────────────────────────────────────────────────────────────────
static uint32_t lastPostMs = 0;
static uint32_t lastLogMs  = 0;

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ESP32 Miata Node ===");

    WiFi.begin(SSID);
    Serial.print("Connecting to miotspot");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
}

void updateMockValues() {
    uint32_t t = millis();

    // RPM: ramps 800→7000 over 10s, then resets
    float cycle = (t % 10000) / 10000.0f;
    mockRpm = 800 + (6200 * cycle);

    // Water temp: slowly climbs 20→95°C over 60s
    mockWater = 20 + (75 * min((float)t / 60000.0f, 1.0f));

    // Oil pressure: follows RPM loosely
    mockOil = 0.5f + (mockRpm / 7000.0f) * 4.5f;

    // Lights/pop-ups toggle on timers
    lightsOn = (t / 5000) % 2 == 0;
    lightsUp = (t / 8000) % 2 == 0;
}

// Returns true if any value has changed enough to warrant a serial log
bool valuesChangedSignificantly() {
    int rpm   = (int)mockRpm;
    int water = (int)mockWater;

    // RPM threshold: 100 RPM, water: 1°C, oil: 0.2 bar
    if (abs(rpm   - lastPostedRpm)   > 100)  return true;
    if (abs(water - lastPostedWater) > 1)    return true;
    if (fabsf(mockOil - lastPostedOil) > 0.2f) return true;
    return false;
}

void loop() {
    // ── WiFi watchdog ──────────────────────────────────────────────────────
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[wifi] lost — reconnecting...");
        WiFi.reconnect();
        delay(1000);
        return;
    }

    uint32_t now = millis();

    // ── Non-blocking rate gate: only POST every POST_INTERVAL_MS ──────────
    if (now - lastPostMs < POST_INTERVAL_MS) {
        // Nothing to do yet — yield to let the ESP32 breathe
        delay(10);
        return;
    }
    lastPostMs = now;

    updateMockValues();

    // ── Build JSON payload ─────────────────────────────────────────────────
    char payload[512];
    snprintf(payload, sizeof(payload),
             "{"
             "\"rpm\":%d,"
             "\"water_temp_c\":%d,"
             "\"oil_pressure\":%.2f,"
             "\"lights_on\":%s,"
             "\"beam_on\":false,"
             "\"lights_up\":%s,"
             "\"parking_brake_on\":false,"
             "\"gps_valid\":true,"
             "\"gps_lat\":51.4229,"
             "\"gps_lon\":4.4302,"
             "\"gps_speed\":%.1f,"
             "\"gps_alt\":12.0,"
             "\"gps_sats\":8,"
             "\"imu_x\":%.2f,"
             "\"imu_y\":%.2f,"
             "\"imu_z\":1.0"
             "}",
             (int)mockRpm,
             (int)mockWater,
             mockOil,
             lightsOn ? "true" : "false",
             lightsUp ? "true" : "false",
             mockRpm / 100.0f,
             sinf(millis() / 3000.0f) * 0.4f,
             cosf(millis() / 5000.0f) * 0.2f
    );

    // ── HTTP POST ──────────────────────────────────────────────────────────
    HTTPClient http;
    http.begin(RPI_URL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(500);  // don't hang the loop if the Pi is slow

    int code = http.POST(payload);
    http.end();

    // ── Serial logging: only when values change OR every LOG_INTERVAL_MS ──
    bool changed = valuesChangedSignificantly();
    bool logDue  = (now - lastLogMs) >= LOG_INTERVAL_MS;

    if (code == 200) {
        if (changed || logDue) {
            Serial.printf("[post] ok — RPM:%d  Water:%d°C  Oil:%.1fbar\n",
                          (int)mockRpm, (int)mockWater, mockOil);
            lastPostedRpm   = (int)mockRpm;
            lastPostedWater = (int)mockWater;
            lastPostedOil   = mockOil;
            lastLogMs       = now;
        }
    } else {
        // Always log failures — they're important
        Serial.printf("[post] FAILED code=%d\n", code);
    }
}