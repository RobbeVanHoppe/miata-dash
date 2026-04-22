/**
 * tabs/sensors.js — Sensors tab
 *
 * Displays: RPM, coolant temp, oil pressure, vehicle state pills, mini G-force, mini GPS.
 * Receives real data from the state poller via update(state).
 *
 * To add a new sensor:
 *   1. Add the HTML element with an id in dashboard.html inside #page-sensors.
 *   2. Read it here in update().
 */

import { setText, setBar, setPill, setGBar, fmt } from '../ui.js';

// RPM thresholds (B6ZE)
const RPM_MAX     = 8000;
const RPM_WARN    = 6500;  // start shifting to amber
const RPM_REDLINE = 7200;  // red

// Water temp thresholds (°C)
const WATER_WARN = 100;
const WATER_CRIT = 108;

// Oil pressure thresholds (bar)
const OIL_LOW_RPM_THRESHOLD = 800;  // below this RPM, low oil is expected at idle
const OIL_CRIT = 0.5;               // below this at driving RPM → critical

export function init() {
    // Nothing to set up — this tab has no timers or canvas of its own.
    // The G cross plot lives in the IMU tab.
}

/**
 * Called on every state poll (~100ms).
 * @param {object} state  Full CarState dict from /api/state
 */
export function update(state) {
    _updateRpm(state.rpm ?? 0);
    _updateCoolant(state.water_temp_c ?? 0);
    _updateOil(state.oil_pressure ?? 0, state.rpm ?? 0);
    _updateStateFlags(state);
    _updateGMini(state);
    _updateGpsMini(state);
}

// ── Private ───────────────────────────────────────────────────────────────────

function _updateRpm(rpm) {
    setText('rpm-big', rpm.toLocaleString());

    const pct = (rpm / RPM_MAX) * 100;
    let colorClass = 'green';
    if (rpm >= RPM_REDLINE) colorClass = 'amber';
    else if (rpm >= RPM_WARN) colorClass = 'purple';
    setBar('rpmBar', pct, colorClass);
}

function _updateCoolant(wt) {
    setText('water-val', wt || '--');
    setBar('waterBar', (wt / 120) * 100, wt > WATER_WARN ? 'amber' : 'green');

    const isCrit  = wt > WATER_CRIT;
    const isWarm  = wt > WATER_WARN;
    setPill(
        'water-pill', 'water-pill-t',
        isCrit, 'Overheat!',
        isWarm ? 'Warm' : 'Normal',
        isCrit ? 'err' : isWarm ? 'warn' : 'on'
    );
}

function _updateOil(op, rpm) {
    setText('oil-val', fmt(op, 1, true));
    setBar('oilBar', (op / 6) * 100, op < OIL_CRIT && rpm > OIL_LOW_RPM_THRESHOLD ? 'amber' : 'green');

    const isLow = op < OIL_CRIT && rpm > OIL_LOW_RPM_THRESHOLD;
    setPill('oil-pill', 'oil-pill-t', !isLow, 'Good', 'Low Oil!', isLow ? 'err' : 'on');
}

function _updateStateFlags(state) {
    setPill('pl-lights', 'pl-lights-t', state.lights_on,        'Lights On',   'Lights Off');
    setPill('pl-beam',   'pl-beam-t',   state.beam_on,           'Beam On',     'Beam Off');
    setPill('pl-up',     'pl-up-t',     state.lights_up,         'Lights Up',   'Lights Down');
    setPill('pl-brake',  'pl-brake-t',  state.parking_brake_on,  'P-Brake On',  'Brake Off',
        state.parking_brake_on ? 'err' : 'off');
}

function _updateGMini(state) {
    const ix = state.imu_x ?? 0;
    const iy = state.imu_y ?? 0;
    const iz = state.imu_z ?? 0;
    setText('s-gx', fmt(ix, 2, true));
    setText('s-gy', fmt(iy, 2, true));
    setText('s-gz', fmt(iz, 2, true));
    setGBar('gBarLat', iy);
}

function _updateGpsMini(state) {
    const ok = state.gps_valid;
    setText('s-lat', ok ? fmt(state.gps_lat, 5) + '°' : '--');
    setText('s-lon', ok ? fmt(state.gps_lon, 5) + '°' : '--');
    setText('s-spd', ok ? Math.round(state.gps_speed ?? 0) + ' km/h' : '--');
    setPill('s-fix', 's-fix-t', ok, 'Fixed', 'No Fix', ok ? 'on' : 'off');
}