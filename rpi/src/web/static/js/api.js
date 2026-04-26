/**
 * api.js — Central API layer
 *
 * All tabs talk to the backend through here. Nothing else should fetch directly.
 * Add new endpoints in one place, mock them in one place.
 */

const POLL_INTERVAL_MS = 250;

/** Registered update callbacks, one per tab */
const _listeners = [];

let _lastState = null;
let _pollTimer = null;
let _consecutiveErrors = 0;
let _lastEtag = null;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * Register a callback that receives the full state object on every poll.
 * @param {(state: CarState) => void} fn
 */
export function onStateUpdate(fn) {
    _listeners.push(fn);
}

/**
 * Send a command string to the sensor Arduino via the RPi bridge.
 * @param {string} command  e.g. "LIGHTS_ON", "RETRACT_UP"
 * @returns {Promise<boolean>} true on success
 */
export async function sendCommand(command) {
    try {
        const res = await fetch('/api/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command }),
        });
        return res.ok;
    } catch (e) {
        console.error('[api] sendCommand failed:', e);
        return false;
    }
}

/**
 * Start polling. Call once from main.js.
 */
export function startPolling() {
    _poll();
    _pollTimer = setInterval(_poll, POLL_INTERVAL_MS);
}

export function stopPolling() {
    clearInterval(_pollTimer);
}

/** Last known good state snapshot */
export function getLastState() {
    return _lastState;
}

// ── Connection status event ───────────────────────────────────────────────────

const _connListeners = [];
export function onConnectionChange(fn) { _connListeners.push(fn); }

function _setConnected(ok) {
    _connListeners.forEach(fn => fn(ok));
}

// ── Internal ──────────────────────────────────────────────────────────────────

async function _poll() {
    try {
        const headers = {};
        if (_lastEtag) headers['If-None-Match'] = _lastEtag;

        const res = await fetch('/api/state', { headers });
        if (res.status === 304) return;  // nothing changed, skip render entirely

        _lastEtag = res.headers.get('ETag');
        const state = await res.json();
        _lastState = state;
        _consecutiveErrors = 0;
        _setConnected(true);
        _listeners.forEach(fn => fn(state));
    } catch (e) {
        _consecutiveErrors++;
        if (_consecutiveErrors >= 3) _setConnected(false);
    }
}