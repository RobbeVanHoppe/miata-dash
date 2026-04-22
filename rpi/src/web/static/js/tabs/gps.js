import { setText, setPill, fmt } from '../ui.js';

// Session distance
let _prevLat = null;
let _prevLon = null;
let _sessionDistKm = 0;

// Leaflet map
let _map = null;
let _marker = null;
let _polyline = null;
let _routePoints = [];
let _mapReady = false;

// Avg speed check
let _avgActive = false;
let _avgStartTime = null;
let _avgSpeedSamples = [];

export function init() {
    _initMap();
}

export function onActivate() {
    // Leaflet needs this when the container was hidden during init
    if (_map) setTimeout(() => _map.invalidateSize(), 50);
}

export function update(state) {
    const ok  = state.gps_valid;
    const lat = state.gps_lat   ?? 0;
    const lon = state.gps_lon   ?? 0;
    const spd = state.gps_speed ?? 0;
    const alt = state.gps_alt   ?? 0;
    const sat = state.gps_sats  ?? 0;

    // Speed
    setText('gps-spd-big', ok ? Math.round(spd) : '0');

    // Coordinates
    setText('gps-lat2',  ok ? fmt(lat, 6) + '°' : '--');
    setText('gps-lon2',  ok ? fmt(lon, 6) + '°' : '--');
    setText('gps-alt2',  ok ? fmt(alt, 0) + ' m' : '-- m');
    setText('gps-sats2', sat);

    // Fix pill
    setPill('gps-fix-pill', 'gps-fix-t', ok, 'Fixed', 'No Fix', ok ? 'on' : 'off');

    // Last update time + session distance
    if (ok) {
        setText('gps-time', new Date().toLocaleTimeString());
        _updateSessionDistance(lat, lon);
        _updateMap(lat, lon);
    }

    setText('gps-track', '---°');
    setText('gps-dist', fmt(_sessionDistKm, 1, true) + ' km');

    // Avg speed check — toggle on button event
    if (state.avg_btn_event) {
        _toggleAvgMode();
    }

    // Keep avg speed display ticking while active
    if (_avgActive && ok) {
        _avgSpeedSamples.push(spd);
        const avg = _avgSpeedSamples.reduce((a, b) => a + b, 0) / _avgSpeedSamples.length;
        setText('avg-speed-val', Math.round(avg));
        setText('avg-elapsed', _formatElapsed(Date.now() - _avgStartTime));
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

function _initMap() {
    _map = L.map('map', { zoomControl: false, attributionControl: true })
             .setView([50.85, 4.35], 13);

    L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
        attribution: '© <a href="https://carto.com/">CARTO</a>',
        subdomains: 'abcd',
        maxZoom: 19,
    }).addTo(_map);

    const icon = L.divIcon({
        className: '',
        html: '<div style="width:14px;height:14px;border-radius:50%;background:#a855f7;border:2px solid #fff;box-shadow:0 0 10px rgba(168,85,247,.9)"></div>',
        iconSize: [14, 14],
        iconAnchor: [7, 7],
    });
    _marker   = L.marker([50.85, 4.35], { icon }).addTo(_map);
    _polyline = L.polyline([], { color: '#a855f7', weight: 3, opacity: 0.75 }).addTo(_map);
}

function _updateMap(lat, lon) {
    if (!_map) return;
    _marker.setLatLng([lat, lon]);
    _routePoints.push([lat, lon]);
    _polyline.setLatLngs(_routePoints);
    if (!_mapReady) {
        _map.setView([lat, lon], 15);
        _mapReady = true;
    } else {
        _map.panTo([lat, lon]);
    }
}

function _toggleAvgMode() {
    _avgActive = !_avgActive;
    if (_avgActive) {
        _avgStartTime = Date.now();
        _avgSpeedSamples = [];
        setText('avg-speed-val', '0');
        setText('avg-elapsed', '00:00');
    }
    document.getElementById('avg-overlay').classList.toggle('active', _avgActive);
}

function _formatElapsed(ms) {
    const totalSec = Math.floor(ms / 1000);
    const m = Math.floor(totalSec / 60);
    const s = totalSec % 60;
    const h = Math.floor(m / 60);
    if (h > 0) return `${h}:${String(m % 60).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
    return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
}

function _updateSessionDistance(lat, lon) {
    if (_prevLat !== null && _prevLon !== null) {
        _sessionDistKm += _haversineKm(_prevLat, _prevLon, lat, lon);
    }
    _prevLat = lat;
    _prevLon = lon;
}

function _haversineKm(lat1, lon1, lat2, lon2) {
    const R = 6371;
    const dLat = _deg2rad(lat2 - lat1);
    const dLon = _deg2rad(lon2 - lon1);
    const a = Math.sin(dLat / 2) ** 2
        + Math.cos(_deg2rad(lat1)) * Math.cos(_deg2rad(lat2)) * Math.sin(dLon / 2) ** 2;
    return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

function _deg2rad(deg) { return deg * (Math.PI / 180); }
