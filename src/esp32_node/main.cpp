#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* SSID    = "miotspot";
const char* RPI_URL = "http://192.168.4.1:5000/api/esp32/data";

// Mock state
float mockRpm       = 0;
float mockWater     = 20;
float mockOil       = 0;
bool  lightsOn      = false;
bool  lightsUp      = false;

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

    // RPM: ramps from 800 to 7000 over 10 seconds, then drops back
    float cycle = (t % 10000) / 10000.0f;
    mockRpm = 800 + (6200 * cycle);

    // Water temp: slowly climbs from 20 to 95°C over 60 seconds
    mockWater = 20 + (75 * min((float)t / 60000.0f, 1.0f));

    // Oil pressure: follows RPM loosely (higher RPM = higher pressure)
    mockOil = 0.5f + (mockRpm / 7000.0f) * 4.5f;

    // Lights toggle every 5 seconds
    lightsOn = (t / 5000) % 2 == 0;

    // Pop-ups toggle every 8 seconds
    lightsUp = (t / 8000) % 2 == 0;
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost, reconnecting...");
        WiFi.reconnect();
        delay(1000);
        return;
    }

    updateMockValues();

    // Build JSON payload
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
             mockRpm / 100.0f,               // speed loosely follows RPM
             sinf(millis() / 3000.0f) * 0.4f,  // gentle lateral G sway
             cosf(millis() / 5000.0f) * 0.2f   // gentle longitudinal G
    );

    HTTPClient http;
    http.begin(RPI_URL);
    http.addHeader("Content-Type", "application/json");

    int code = http.POST(payload);
    if (code == 200) {
        Serial.printf("POST ok — RPM:%d Water:%d Oil:%.1f\n",
                      (int)mockRpm, (int)mockWater, mockOil);
    } else {
        Serial.println("POST failed: " + String(code));
    }
    http.end();

    delay(100);  // 10Hz
}