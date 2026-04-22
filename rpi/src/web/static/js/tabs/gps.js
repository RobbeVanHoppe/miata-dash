/**
 * tabs/gps.js — GPS tab
 *
 * Displays: speed, lat/lon/alt, satellite count, fix status, compass heading.
 * Session distance is tracked locally (resets on page reload).
 *
 * To extend: wire up a map tile (e.g. Leaflet) by calling _updateMap(lat, lon)
 * once the GPS has a fix. A <div id="map"> placeholder is in dashboard.html.
 */

import { setText, setPill, fmt } from '../ui.js';

// Session distance tracking
let _prevLat = null;
let _prevLon = null;
let _sessionDistKm = 0;

export function init() {
    // Nothing to set up yet.
    // Future: initialise Leaflet map here.
}

/**
 * @param {object} state
 */
export function update(state) {
    const ok  = state.gps_valid;
    const lat = state.gps_lat  ?? 0;
    const lon = state.gps_lon  ?? 0;
    const spd = state.gps_speed ?? 0;
    const alt = state.gps_alt  ?? 0;
    const sat = state.gps_sats ?? 0;

    // Speed
    setText('gps-spd-big', ok ? Math.round(spd) : '0');

    // Coordinates
    setText('gps-lat2', ok ? fmt(lat, 6) + '°' : '--');
    setText('gps-lon2', ok ? fmt(lon, 6) + '°' : '--');
    setText('gps-alt2', ok ? fmt(alt, 0) + ' m' : '-- m');
    setText('gps-sats2', sat);

    // Fix pill
    setPill('gps-fix-pill', 'gps-fix-t', ok, 'Fixed', 'No Fix', ok ? 'on' : 'off');

    // Last update time
    if (ok) {
        setText('gps-time', new Date().toLocaleTimeString());
        _updateSessionDistance(lat, lon);
    }

    // Compass (heading is estimated from movement direction when speed > 5 km/h)
    // TinyGPS++ doesn't expose course directly in our payload yet — placeholder.
    // When you add course to the GPS payload, replace this with state.gps_course.
    setText('gps-track', '---°');

    setText('gps-dist', fmt(_sessionDistKm, 1, true) + ' km');
}

// ── Private ───────────────────────────────────────────────────────────────────

function _updateSessionDistance(lat, lon) {
    if (_prevLat !== null && _prevLon !== null) {
        _sessionDistKm += _haversineKm(_prevLat, _prevLon, lat, lon);
    }
    _prevLat = lat;
    _prevLon = lon;
}

/** Haversine great-circle distance in km */
function _haversineKm(lat1, lon1, lat2, lon2) {
    const R = 6371;
    const dLat = _deg2rad(lat2 - lat1);
    const dLon = _deg2rad(lon2 - lon1);
    const a = Math.sin(dLat / 2) ** 2
        + Math.cos(_deg2rad(lat1)) * Math.cos(_deg2rad(lat2)) * Math.sin(dLon / 2) ** 2;
    return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

function _deg2rad(deg) { return deg * (Math.PI / 180); }