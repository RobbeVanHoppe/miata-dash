/**
 * tabs/imu.js — IMU / G-force tab
 *
 * Displays: live X/Y/Z G-values, centred G-bars, G-cross scatter plot, peak tracking.
 *
 * The G-cross canvas is only redrawn when the IMU tab is active (perf).
 * History trails are kept in a circular buffer (_gHistory).
 */

import { setText, setGBar, setValueColor, fmt } from '../ui.js';

const HISTORY_LEN = 80;   // points kept in the scatter trail
const G_MAX       = 3.0;  // full-scale on bars and plot

// G warn/crit thresholds for colour coding
const G_WARN = 1.0;
const G_CRIT = 2.0;

let _gHistory = [];
let _peakLat  = 0;
let _peakLong = 0;
let _canvas   = null;
let _ctx      = null;

export function init() {
    _canvas = document.getElementById('gCross');
    if (_canvas) _ctx = _canvas.getContext('2d');

    // Expose resetPeaks for the button onclick
    window.ImuTab = { resetPeaks };
}

/**
 * @param {object} state
 */
export function update(state) {
    const ix = state.imu_x ?? 0;
    const iy = state.imu_y ?? 0;
    const iz = state.imu_z ?? 0;

    // Big readouts
    _setGValue('imu-x-big', ix);
    _setGValue('imu-y-big', iy);
    _setGValue('imu-z-big', iz);

    // Centred progress bars
    setGBar('ib-x', ix, G_MAX);
    setGBar('ib-y', iy, G_MAX);
    setGBar('ib-z', iz, G_MAX);

    // Peak tracking
    if (Math.abs(iy) > _peakLat)  { _peakLat  = Math.abs(iy); setText('peak-lat',  fmt(_peakLat,  2, true)); }
    if (Math.abs(ix) > _peakLong) { _peakLong = Math.abs(ix); setText('peak-long', fmt(_peakLong, 2, true)); }

    // G-cross scatter — only draw when tab is visible
    _gHistory.push({ x: iy, y: -ix });  // lateral = X axis, longitudinal = Y axis
    if (_gHistory.length > HISTORY_LEN) _gHistory.shift();

    if (document.getElementById('page-imu')?.classList.contains('active')) {
        _drawGCross(ix, iy);
    }
}

export function resetPeaks() {
    _peakLat = 0; _peakLong = 0; _gHistory = [];
    setText('peak-lat', '0.00');
    setText('peak-long', '0.00');
}

// ── Private ───────────────────────────────────────────────────────────────────

function _setGValue(id, value) {
    setText(id, fmt(value, 2, true));
    setValueColor(id, value, G_WARN, G_CRIT);
}

function _drawGCross(ix, iy) {
    if (!_ctx) return;
    const W  = _canvas.width;
    const H  = _canvas.height;
    const cx = W / 2;
    const cy = H / 2;
    const sc = Math.min(W, H) / 2 / (G_MAX + 0.3);  // slight padding

    _ctx.clearRect(0, 0, W, H);

    // ── Grid rings ──
    for (const g of [1, 2, 3]) {
        _ctx.beginPath();
        _ctx.arc(cx, cy, g * sc, 0, Math.PI * 2);
        _ctx.strokeStyle = `rgba(168,85,247,${g === 1 ? 0.18 : 0.08})`;
        _ctx.lineWidth = 1;
        _ctx.stroke();
    }

    // ── Axes ──
    _ctx.strokeStyle = 'rgba(255,255,255,0.1)';
    _ctx.lineWidth = 1;
    _ctx.beginPath(); _ctx.moveTo(cx, 0);   _ctx.lineTo(cx, H); _ctx.stroke();
    _ctx.beginPath(); _ctx.moveTo(0, cy);   _ctx.lineTo(W, cy); _ctx.stroke();

    // ── History trail ──
    _gHistory.forEach((pt, i) => {
        const alpha = (i / _gHistory.length) * 0.6;
        _ctx.beginPath();
        _ctx.arc(cx + pt.x * sc, cy + pt.y * sc, 2, 0, Math.PI * 2);
        _ctx.fillStyle = `rgba(168,85,247,${alpha})`;
        _ctx.fill();
    });

    // ── Current dot ──
    const dotX = cx + iy * sc;   // lateral → horizontal
    const dotY = cy - ix * sc;   // longitudinal → vertical (invert so brake = up)
    _ctx.beginPath();
    _ctx.arc(dotX, dotY, 5, 0, Math.PI * 2);
    _ctx.fillStyle = '#a855f7';
    _ctx.shadowColor = '#a855f7';
    _ctx.shadowBlur = 12;
    _ctx.fill();
    _ctx.shadowBlur = 0;
}