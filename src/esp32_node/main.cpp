#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* SSID     = "miotspot";
const char* PASSWORD = "miata";
const char* RPI_URL  = "http://192.168.4.1:5000/api/esp32/data";

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ESP32 Miata Node ===");

    WiFi.begin(SSID, PASSWORD);
    Serial.print("Connecting to miotspot");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost, reconnecting...");
        WiFi.reconnect();
        delay(1000);
        return;
    }

    HTTPClient http;
    http.begin(RPI_URL);
    http.addHeader("Content-Type", "application/json");

    // Dummy payload for now — real sensors come later
    String payload = "{\"rpm\":0,\"water_temp_c\":0,\"oil_pressure\":0.0,"
                     "\"lights_on\":false,\"beam_on\":false,\"lights_up\":false,"
                     "\"parking_brake_on\":false,\"gps_valid\":false,"
                     "\"gps_lat\":0.0,\"gps_lon\":0.0,\"gps_speed\":0.0,"
                     "\"gps_alt\":0.0,\"gps_sats\":0,"
                     "\"imu_x\":0.0,\"imu_y\":0.0,\"imu_z\":0.0}";

    int code = http.POST(payload);
    if (code == 200) {
        Serial.println("POST ok");
    } else {
        Serial.println("POST failed: " + String(code));
    }
    http.end();

    delay(100);  // 10Hz
}