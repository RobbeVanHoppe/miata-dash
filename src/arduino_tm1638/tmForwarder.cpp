#include "arduino_tm1638/tmForwarder.h"

TM1638plus tm(4, 3, 2, false);
static TMForwarder* instance = nullptr;

// Pages
__attribute__((unused)) static Page* activePage = nullptr; // To eliminate the warning, var is used ofc
static GeneralPage generalPage;
static SensorPage sensorsPage;

void TMForwarder::begin() {
    instance = this;
    Wire.begin(nodeToAddress(nodeType_));

    Wire.onRequest([](){ if (instance) instance->onI2CRequest(); });
    Wire.onReceive([](int len) { if (instance) instance->onI2CReceive(len); });

    activePage = &generalPage;
    tm.displayBegin();

    showPageSplash("MIATA");
}

void TMForwarder::showPageSplash(const char* text) {
    tm.displayText(text);
    for (int i = 0; i < 8; i++) {
        tm.setLED(i, 3);
    }
    pageTimer = millis();
    splashActive = true;
}

void TMForwarder::renderCurrentPage() {
    if (!activePage) return;
    activePage->update(tm, tm1638State);
}

void TMForwarder::update() {
    handleInputs();
    manageDisplay();
}

void TMForwarder::handleInputs() {
    uint8_t buttons = tm.readButtons();
    if (buttons == 0) return;

    // Button 1: Page Increment & Splash trigger
    if (buttons & 0x01) {
        // 1. Determine the NEXT page name based on what's active now
        PageName nextName = (PageName)((activePage->getPageName() + 1) % PAGE_COUNT);

        // 2. Point 'activePage' to the correct object
        if (nextName == PAGE_GENERAL) activePage = &generalPage;
        else if (nextName == PAGE_SENSORS) activePage = &sensorsPage;

        // 3. Prepare the Splash Message
        char splashMsg[9];
        switch (activePage->getPageName()) {
            case PAGE_GENERAL:
                strncpy(splashMsg, "GENERAL ", 9);
                break;
            case PAGE_SENSORS:
                strncpy(splashMsg, "SENSORS ", 9);
                break;
            default:
                snprintf(splashMsg, sizeof(splashMsg), "PAGE %d  ", (int)activePage->getPageName());
                break;
        }

        // 4. Trigger the splash and call onEnter for the new page
        showPageSplash(splashMsg);
        activePage->onEnter(tm);

        delay(250); // Debounce
    }

    // If not the first button, handle per page
    else {
        handleButtonEvents(buttons);
    }
}
void TMForwarder::manageDisplay() {
    // If we are in "Splash Mode", check if the timer expired
    if (splashActive) {
        if (millis() - pageTimer > 1000) {
            splashActive = false;
            tm.setLEDs(0x0000); // Clear splash LEDs
        } else {
            return; // Don't render page content while splash is active
        }
    }

    // If we reach here, splash is over. Render the actual page content.
    renderCurrentPage();
}

void TMForwarder::handleButtonEvents(uint8_t buttons) {
    readyMsg.type = TYPE_EVENT;
    readyMsg.source = NODE_TM1638_ARDUINO;

    for (int i = 0; i < 8; i++) {
        if (buttons & (1 << i)) {
            // Get action string from the active page (e.g., "P0_B2")
            const char* action = activePage->getButtonAction(i);
            strncpy(readyMsg.payload, action, Message::MaxPayloadSize);

            // Helpful for debugging buttons
            Serial.print("Btn: ");
            Serial.println(readyMsg.payload);

            delay(200);
            break;
        }
    }
}

void TMForwarder::onI2CRequest() {
    Wire.write((uint8_t*)&readyMsg, sizeof(Message));
    // Reset message to IDLE after sending so ESP32 doesn't see the same click twice
    readyMsg.type = TYPE_INFO;
    strcpy(readyMsg.payload, "IDLE");
}

void TMForwarder::onI2CReceive(int howMany) {
    if (howMany == sizeof(Message)) {
        Message msg;
        if (receiveToStruct(msg)) { // Using your BusNode helper
            if (msg.type == TYPE_DATA) {
                // Parsing logic:
                // ESP32 sends "L%d,P%d,R%d,T%d"
                if (sscanf(msg.payload, "L:%d,B:%d,U:%d,R:%d,W:%d,O:%.1f",
                    &tm1638State.lightsOn,
                    &tm1638State.beamOn,
                    &tm1638State.lightsUp,
                    &tm1638State.RPM,
                    &tm1638State.WaterTempC,
                    &tm1638State.OilPressure) >=2) {

                    Serial.print("Data Synced: "); Serial.println(msg.payload);
                }
            }
        }
    }
}