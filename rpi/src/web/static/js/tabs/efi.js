/**
 * tabs/efi.js — rusEFI ECU tab
 *
 * Currently a placeholder — the RPi doesn't yet have a rusEFI serial connection.
 *
 * To wire up real data:
 *   1. Add a Flask endpoint /api/efi that reads from rusEFI via pyserial/TunerStudio protocol.
 *   2. Call _pollEfi() on a separate interval here (ECU updates at ~10Hz, not 100ms).
 *   3. Replace the stub values below with real fields.
 *
 * The update(state) hook is intentionally left minimal — EFI data comes from
 * a separate endpoint, not the main CarState. RPM is mirrored from CarState
 * since the sensor Arduino also reads it.
 */

import { setText, setPill } from '../ui.js';

let _connected = false;
let _efiPollTimer = null;

export function init() {
    window.EfiTab = { connect, disconnect };
}

/**
 * Mirror the sensor-Arduino RPM into the EFI tab while the ECU is offline.
 * @param {object} state
 */
export function update(state) {
    if (!_connected) {
        // Show sensor Arduino RPM as a fallback
        setText('ef-rpm', state.rpm ?? '--');
    }
}

// ── Connection stubs — replace with real pyserial calls via /api/efi ─────────

export async function connect() {
    setPill('ecuPill', 'ecuPill-t', false, '', 'Connecting…', 'warn');

    try {
        const res = await fetch('/api/efi/connect', { method: 'POST' });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        _connected = true;
        setPill('ecuPill', 'ecuPill-t', true, 'Connected', '', 'on');
        _efiPollTimer = setInterval(_pollEfi, 200);  // 5 Hz
    } catch (e) {
        _connected = false;
        setPill('ecuPill', 'ecuPill-t', false, '', 'Not Found', 'err');
        console.warn('[efi] connect failed:', e);
    }
}

export function disconnect() {
    _connected = false;
    clearInterval(_efiPollTimer);
    setPill('ecuPill', 'ecuPill-t', false, '', 'Disconnected', 'err');
    _clearFields();
}

async function _pollEfi() {
    try {
        const res  = await fetch('/api/efi/state');
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        _renderEfi(data);
    } catch (e) {
        console.warn('[efi] poll failed:', e);
        disconnect();
    }
}

/**
 * Render real ECU fields. Shape must match what /api/efi/state returns.
 * Expected keys (all optional — '--' shown if missing):
 *   rpm, afr_target, afr_actual, ign_advance, map_kpa, tps_pct,
 *   clt_c, iat_c, vbatt, injection_mode
 */
function _renderEfi(data) {
    setText('ef-rpm',  data.rpm       ?? '--');
    setText('ef-afr',  _pair(data.afr_target, data.afr_actual));
    setText('ef-ign',  data.ign_advance != null ? data.ign_advance + '°' : '--°');
    setText('ef-map',  _pair(data.map_kpa, data.tps_pct, ' kPa', '%'));
    setText('ef-clt',  _pair(data.clt_c, data.iat_c, '°C', '°C'));
    setText('ef-vbat', data.vbatt != null ? data.vbatt.toFixed(1) + 'V' : '--V');
    setText('ef-mode', data.injection_mode ?? '--');
}

function _clearFields() {
    for (const id of ['ef-rpm','ef-afr','ef-ign','ef-map','ef-clt','ef-vbat','ef-mode']) {
        setText(id, '--');
    }
}

function _pair(a, b, unitA = '', unitB = '') {
    const fmtA = a != null ? a + unitA : '--';
    const fmtB = b != null ? b + unitB : '--';
    return `${fmtA} / ${fmtB}`;
}