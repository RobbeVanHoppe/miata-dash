/**
 * main.js — Entry point
 *
 * Boots the app:
 *   1. Initialises each tab module.
 *   2. Starts the state poller.
 *   3. Routes state updates to all active tab modules.
 *   4. Handles the top-bar clock and connection indicator.
 *
 * Adding a new tab:
 *   1. Create static/js/tabs/mytab.js with init() and update(state).
 *   2. import * as MyTab from './tabs/mytab.js' below.
 *   3. Add it to TABS with the page id as key.
 *   4. Add the HTML page + nav button in dashboard.html.
 */

import { onStateUpdate, onConnectionChange, startPolling } from './api.js';
import * as Sensors from './tabs/sensors.js';
import * as Lights  from './tabs/lights.js';
import * as Gps     from './tabs/gps.js';
import * as Imu     from './tabs/imu.js';
import * as Efi     from './tabs/efi.js';
import * as Sdr     from './tabs/sdr.js';

// ── Tab registry ──────────────────────────────────────────────────────────────
// Key = page-<id> suffix used in HTML.  Value = module reference.
const TABS = {
    sensors: Sensors,
    lights:  Lights,
    gps:     Gps,
    imu:     Imu,
    efi:     Efi,
    sdr:     Sdr,
};

let _activeTabKey = 'sensors';

// ── Boot ──────────────────────────────────────────────────────────────────────

document.addEventListener('DOMContentLoaded', () => {
    // Initialise every tab
    for (const mod of Object.values(TABS)) {
        mod.init?.();
    }

    // Clock
    _updateClock();
    setInterval(_updateClock, 1000);

    // Connection indicator
    onConnectionChange(ok => {
        const dot   = document.getElementById('connDot');
        const label = document.getElementById('connLbl');
        if (dot)   dot.className   = 'dot' + (ok ? '' : ' err');
        if (label) label.textContent = ok ? 'Online' : 'Fault';
    });

    // ESP32 status in top bar
    onStateUpdate(state => {
        // GPS status
        const gpsOk = state.gps_valid;
        const gpsDot = document.getElementById('gpsDot');
        const gpsLbl = document.getElementById('gpsLbl');
        if (gpsDot) gpsDot.className = 'dot' + (gpsOk ? '' : ' err');
        if (gpsLbl) gpsLbl.textContent = gpsOk ? `${state.gps_sats} Sats` : 'No Fix';

        // ESP32 connection status
        const espOk = state.esp32_connected;
        const connDot = document.getElementById('connDot');
        const connLbl = document.getElementById('connLbl');
        if (connDot) connDot.className = 'dot' + (espOk ? '' : ' err');
        if (connLbl) connLbl.textContent = espOk ? 'Online' : 'ESP32 Lost';
    });

    // Route every state update to the active tab AND sensors (always updated)
    onStateUpdate(state => {
        Sensors.update(state);                           // sensors page is always fresh
        if (_activeTabKey !== 'sensors') {
            TABS[_activeTabKey]?.update(state);            // also update the visible tab
        }
    });

    // Start fetching
    startPolling();

    // Expose tab switcher globally for onclick= attributes
    window.showTab = _showTab;
});

// ── Tab switching ─────────────────────────────────────────────────────────────

function _showTab(key, btnEl) {
    // Deactivate old tab
    TABS[_activeTabKey]?.onDeactivate?.();
    document.getElementById(`page-${_activeTabKey}`)?.classList.remove('active');
    document.querySelectorAll('.tab').forEach(b => b.classList.remove('active'));

    // Activate new tab
    _activeTabKey = key;
    document.getElementById(`page-${key}`)?.classList.add('active');
    btnEl.classList.add('active');
    TABS[key]?.onActivate?.();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

function _updateClock() {
    const d = new Date();
    const hh = String(d.getHours()).padStart(2, '0');
    const mm = String(d.getMinutes()).padStart(2, '0');
    const el = document.getElementById('clock');
    if (el) el.textContent = `${hh}:${mm}`;
}