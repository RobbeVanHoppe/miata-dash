/**
 * tabs/sdr.js — RTL-SDR receiver tab
 *
 * Currently renders a demo spectrum (white noise + simulated carrier).
 *
 * To wire up real data:
 *   1. On the RPi, run rtl_power or use pyrtlsdr to stream FFT bins via a
 *      WebSocket or /api/sdr/spectrum endpoint.
 *   2. Replace _drawDemoSpectrum() with _drawRealSpectrum(bins) that
 *      plots the actual power values.
 *   3. Expose /api/sdr/tune?freq=<MHz> to retune the dongle.
 */

import { setText, setPill } from '../ui.js';

const FREQ_MIN_MHZ = 24;
const FREQ_MAX_MHZ = 1800;

let _freqMHz = 88.0;
let _canvas  = null;
let _ctx     = null;
let _animId  = null;
let _active  = false;

export function init() {
    _canvas = document.getElementById('sdrCanvas');
    if (_canvas) _ctx = _canvas.getContext('2d');

    window.SdrTab = { adjustFreq, setFreq };
}

/** Start/stop the spectrum animation when the tab is shown/hidden. */
export function onActivate() {
    _active = true;
    if (!_animId) _loop();
}

export function onDeactivate() {
    _active = false;
    cancelAnimationFrame(_animId);
    _animId = null;
}

/** Called from main poller — SDR has no CarState fields yet */
export function update(_state) {
    // nothing from CarState needed here
}

export function adjustFreq(deltaMHz) {
    _freqMHz = Math.max(FREQ_MIN_MHZ, Math.min(FREQ_MAX_MHZ, _freqMHz + deltaMHz));
    _syncFreqDisplay();
    _tuneRemote();
}

export function setFreq(mhz) {
    _freqMHz = mhz;
    _syncFreqDisplay();
    _tuneRemote();
}

// ── Private ───────────────────────────────────────────────────────────────────

function _loop() {
    _drawDemoSpectrum();
    if (_active) {
        _animId = requestAnimationFrame(_loop);
    }
}

function _drawDemoSpectrum() {
    if (!_ctx) return;
    const W = _canvas.width, H = _canvas.height;
    _ctx.clearRect(0, 0, W, H);

    // Fill path
    _ctx.beginPath();
    _ctx.moveTo(0, H);

    for (let x = 0; x < W; x++) {
        const noise    = Math.random() * 7;
        const carrier  = _carrierAt(x / W);
        _ctx.lineTo(x, H - noise - carrier);
    }

    _ctx.lineTo(W, H);
    _ctx.closePath();

    const grad = _ctx.createLinearGradient(0, 0, 0, H);
    grad.addColorStop(0, 'rgba(45,212,191,0.55)');
    grad.addColorStop(1, 'rgba(45,212,191,0.04)');
    _ctx.fillStyle = grad;
    _ctx.fill();

    _ctx.strokeStyle = 'rgba(45,212,191,0.85)';
    _ctx.lineWidth = 1.5;
    _ctx.stroke();

    // Tuner centre line
    _ctx.strokeStyle  = 'rgba(168,85,247,0.65)';
    _ctx.lineWidth    = 1;
    _ctx.setLineDash([3, 3]);
    _ctx.beginPath();
    _ctx.moveTo(W / 2, 0);
    _ctx.lineTo(W / 2, H);
    _ctx.stroke();
    _ctx.setLineDash([]);
}

/** Simulated carrier bump centred at middle of visible band */
function _carrierAt(normPos) {
    const d = Math.abs(normPos - 0.5);
    return 50 * Math.exp(-d * d * 2800) * (0.7 + Math.random() * 0.3);
}

function _syncFreqDisplay() {
    setText('sdr-freq-disp', _freqMHz.toFixed(3) + ' MHz');
}

/** Tell the RPi backend to retune — no-op until the endpoint exists */
async function _tuneRemote() {
    try {
        await fetch(`/api/sdr/tune?freq=${_freqMHz}`);
    } catch {
        // Expected while endpoint not yet implemented
    }
}