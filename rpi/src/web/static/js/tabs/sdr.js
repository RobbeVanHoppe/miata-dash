/**
 * tabs/sdr.js — RTL-SDR tab, 1024×760 touch-optimised
 *
 * Features:
 *   - Live spectrum + waterfall via /ws/sdr WebSocket
 *   - Grouped, touch-scrollable preset list with search
 *   - Add-preset bottom sheet (touch-friendly, no typing required while driving)
 *   - Delete custom presets with long-press or swipe-reveal delete button
 *   - REST: /api/sdr/start|stop|tune|presets
 */

import { setText, setPill } from '../ui.js';

// ── Config ────────────────────────────────────────────────────────────────────
const WS_URL      = `ws://${location.host}/ws/sdr`;
const NOISE_FLOOR = -90;
const SIGNAL_TOP  = -20;
const WATERFALL_H = 80;   // rows kept in waterfall history

// ── Module state ──────────────────────────────────────────────────────────────
let _ws        = null;
let _active    = false;
let _running   = false;
let _freqHz    = 88_000_000;
let _mode      = 'wfm';
let _squelch   = 0;
let _groups    = [];       // cached grouped presets from API
let _searchQ   = '';

// Canvas
let _specCtx   = null;
let _specW     = 0, _specH = 0;
let _wfCtx     = null;
let _wfW       = 0;
let _wfImage   = null;

// Long-press detection for delete
let _lpTimer   = null;
let _lpTarget  = null;

// ── Init ──────────────────────────────────────────────────────────────────────

export function init() {
    const sc = document.getElementById('sdrSpectrum');
    const wc = document.getElementById('sdrWaterfall');
    if (sc) { _specCtx = sc.getContext('2d'); _specW = sc.width; _specH = sc.height; }
    if (wc) {
        _wfCtx  = wc.getContext('2d');
        _wfW    = wc.width;
        _wfImage = _wfCtx.createImageData(_wfW, WATERFALL_H);
    }

    // Wire up search input
    const sq = document.getElementById('sdrSearch');
    if (sq) sq.addEventListener('input', e => { _searchQ = e.target.value.toLowerCase(); _renderPresets(); });

    // Wire up add-sheet close on backdrop tap
    const sheet = document.getElementById('sdrSheet');
    if (sheet) sheet.addEventListener('click', e => { if (e.target === sheet) _closeSheet(); });

    // Wire up add-sheet name input live preview
    const nameInput = document.getElementById('sdrSheetName');
    if (nameInput) nameInput.addEventListener('input', e => {
        document.getElementById('sdrSheetPreview').textContent = e.target.value || _suggestName();
    });

    window.SdrTab = {
        start, stop,
        adjustFreq, tuneManual,
        setMode, setSquelch,
        openAddSheet, savePreset, closeSheet: _closeSheet,
        deletePreset, tunePreset,
        clearSearch,
    };

    _loadPresets();
}

export function onActivate() {
    _active = true;
    _connectWS();
    _fetchState();
}

export function onDeactivate() {
    _active = false;
    _disconnectWS();
    _closeSheet();
}

export function update(_state) { /* SDR state comes from WS, not CarState */ }

// ── Receiver controls ─────────────────────────────────────────────────────────

export async function start() {
    _setBtn('sdrStartBtn', true);
    try {
        const res  = await fetch('/api/sdr/start', { method: 'POST' });
        _applyState(await res.json());
        _connectWS();
    } finally { _setBtn('sdrStartBtn', false); }
}

export async function stop() {
    _disconnectWS();
    await fetch('/api/sdr/stop', { method: 'POST' }).catch(() => {});
    _running = false;
    _updateRunPill();
}

export async function adjustFreq(deltaMHz) {
    const hz = Math.max(24e6, Math.min(1766e6, _freqHz + deltaMHz * 1e6));
    await _tune({ freq_hz: Math.round(hz) });
}

export async function tuneManual() {
    const inp = document.getElementById('sdrManualFreq');
    if (!inp) return;
    const mhz = parseFloat(inp.value);
    if (!isNaN(mhz)) await _tune({ freq_hz: Math.round(mhz * 1e6) });
}

export async function setMode(mode) { await _tune({ mode }); }

export async function setSquelch(val) {
    _squelch = parseFloat(val);
    setText('sdrSqVal', _squelch.toFixed(0) + ' dB');
    await _tune({ squelch: _squelch });
}

export async function tunePreset(id) {
    // Find preset in cached groups
    for (const g of _groups) {
        const p = g.presets.find(x => x.id === id);
        if (p) { await _tune({ freq_hz: p.freq_hz, mode: p.mode, squelch: p.squelch }); return; }
    }
}

async function _tune(params) {
    try {
        const res  = await fetch('/api/sdr/tune', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(params),
        });
        _applyState(await res.json());
    } catch (e) { console.error('[sdr] tune:', e); }
}

// ── Preset CRUD ───────────────────────────────────────────────────────────────

async function _loadPresets() {
    try {
        const res = await fetch('/api/sdr/presets');
        _groups   = await res.json();
        _renderPresets();
    } catch (e) { console.warn('[sdr] presets load failed:', e); }
}

export function openAddSheet() {
    const sheet = document.getElementById('sdrSheet');
    if (!sheet) return;

    // Pre-fill suggested name based on current freq+mode
    const nameInput = document.getElementById('sdrSheetName');
    const suggested = _suggestName();
    if (nameInput) nameInput.value = suggested;
    setText('sdrSheetPreview', suggested);
    setText('sdrSheetFreq', (_freqHz / 1e6).toFixed(3) + ' MHz · ' + _mode.toUpperCase());

    // Highlight the right band button
    const band = _detectBand(_freqHz);
    document.querySelectorAll('.sheet-band-btn').forEach(b => {
        b.classList.toggle('active', b.dataset.band === band);
    });

    sheet.classList.add('open');
    // Focus the name input after transition
    setTimeout(() => nameInput?.focus(), 300);
}

function _closeSheet() {
    document.getElementById('sdrSheet')?.classList.remove('open');
}

export async function savePreset() {
    const nameInput = document.getElementById('sdrSheetName');
    const label     = nameInput?.value.trim() || _suggestName();
    const band      = document.querySelector('.sheet-band-btn.active')?.dataset.band || _detectBand(_freqHz);

    try {
        const res = await fetch('/api/sdr/presets', {
            method:  'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                label, band,
                freq_hz: _freqHz,
                mode:    _mode,
                squelch: _squelch,
            }),
        });
        if (res.ok) {
            _closeSheet();
            await _loadPresets();
        }
    } catch (e) { console.error('[sdr] save preset:', e); }
}

export async function deletePreset(id) {
    try {
        const res = await fetch(`/api/sdr/presets/${id}`, { method: 'DELETE' });
        if (res.ok) await _loadPresets();
    } catch (e) { console.error('[sdr] delete preset:', e); }
}

// ── Preset list rendering ─────────────────────────────────────────────────────

function _renderPresets() {
    const container = document.getElementById('sdrPresetList');
    if (!container) return;

    const q = _searchQ.trim();
    container.innerHTML = '';

    let hasResults = false;

    for (const group of _groups) {
        // Filter within group
        const filtered = q
            ? group.presets.filter(p =>
                p.label.toLowerCase().includes(q) ||
                String(p.freq_mhz).includes(q))
            : group.presets;

        if (filtered.length === 0) continue;
        hasResults = true;

        // Band header
        const header = document.createElement('div');
        header.className = 'preset-group-header';
        header.textContent = group.label;
        container.appendChild(header);

        for (const preset of filtered) {
            container.appendChild(_makePresetRow(preset));
        }
    }

    if (!hasResults) {
        const empty = document.createElement('div');
        empty.className = 'preset-empty';
        empty.textContent = q ? `No presets matching "${q}"` : 'No presets yet';
        container.appendChild(empty);
    }
}

function _makePresetRow(preset) {
    const row = document.createElement('div');
    row.className    = 'preset-row';
    row.dataset.id   = preset.id;

    // Tune action on tap
    row.addEventListener('click', () => tunePreset(preset.id));

    // Long-press to delete (custom presets only)
    if (preset.custom) {
        row.addEventListener('pointerdown', () => {
            _lpTarget = preset.id;
            _lpTimer  = setTimeout(() => _showDeleteConfirm(preset, row), 600);
        });
        row.addEventListener('pointerup',    _cancelLp);
        row.addEventListener('pointerleave', _cancelLp);
    }

    const modeColors = { wfm: 'var(--teal)', am: 'var(--amber)', nfm: 'var(--accent)' };
    const modeColor  = modeColors[preset.mode] ?? 'var(--text3)';

    row.innerHTML = `
        <div class="preset-freq">${preset.freq_mhz.toFixed(3)}</div>
        <div class="preset-info">
            <div class="preset-label">${_esc(preset.label)}</div>
            <div class="preset-meta">
                <span style="color:${modeColor};font-size:9px;font-family:DM Mono,monospace;font-weight:600">
                    ${preset.mode.toUpperCase()}
                </span>
                ${preset.squelch > 0 ? `<span class="preset-sq">SQ ${preset.squelch.toFixed(0)}</span>` : ''}
                ${preset.custom ? '<span class="preset-custom-badge">✎</span>' : ''}
            </div>
        </div>
        <div class="preset-tune-arrow">▶</div>
    `;

    return row;
}

function _showDeleteConfirm(preset, row) {
    // Overlay a delete button on the row
    const btn = document.createElement('button');
    btn.className   = 'preset-delete-btn';
    btn.textContent = '✕ Delete';
    btn.onclick = async (e) => {
        e.stopPropagation();
        btn.textContent = '…';
        await deletePreset(preset.id);
    };
    // Prevent duplicate
    row.querySelector('.preset-delete-btn')?.remove();
    row.appendChild(btn);
    // Auto-hide after 3 s
    setTimeout(() => btn.remove(), 3000);
}

function _cancelLp() {
    if (_lpTimer) { clearTimeout(_lpTimer); _lpTimer = null; }
}

// ── Search ────────────────────────────────────────────────────────────────────

export function clearSearch() {
    const inp = document.getElementById('sdrSearch');
    if (inp) inp.value = '';
    _searchQ = '';
    _renderPresets();
}

// ── WebSocket ─────────────────────────────────────────────────────────────────

function _connectWS() {
    if (_ws && _ws.readyState <= 1) return;
    _ws = new WebSocket(WS_URL);
    _ws.onopen    = () => setPill('sdrWsPill', 'sdrWsPill-t', true,  'Live',    'Offline', 'on');
    _ws.onmessage = (ev) => {
        try {
            const f = JSON.parse(ev.data);
            _applyState({ freq_hz: f.freq_hz, mode: f.mode, squelch: f.squelch });
            if (_active) { _drawSpectrum(f.bins); _pushWaterfall(f.bins); }
        } catch (_) {}
    };
    _ws.onclose = () => {
        setPill('sdrWsPill', 'sdrWsPill-t', false, 'Live', 'Offline', 'off');
        if (_active) setTimeout(_connectWS, 3000);
    };
    _ws.onerror = () => setPill('sdrWsPill', 'sdrWsPill-t', false, 'Live', 'Error', 'err');
}

function _disconnectWS() {
    if (_ws) { _ws.onclose = null; _ws.close(); _ws = null; }
}

// ── Rendering ─────────────────────────────────────────────────────────────────

function _drawSpectrum(bins) {
    if (!_specCtx || !bins) return;
    const W = _specW, H = _specH, N = bins.length;
    _specCtx.clearRect(0, 0, W, H);

    // Grid
    _specCtx.strokeStyle = 'rgba(255,255,255,0.04)';
    _specCtx.lineWidth   = 1;
    for (let db = NOISE_FLOOR; db <= SIGNAL_TOP; db += 10) {
        const y = _dbToY(db, H);
        _specCtx.beginPath(); _specCtx.moveTo(0, y); _specCtx.lineTo(W, y); _specCtx.stroke();
    }

    // dB labels on grid
    _specCtx.fillStyle  = 'rgba(255,255,255,0.18)';
    _specCtx.font       = '9px DM Mono, monospace';
    _specCtx.textAlign  = 'left';
    for (let db = NOISE_FLOOR + 10; db < SIGNAL_TOP; db += 10) {
        _specCtx.fillText(`${db}`, 4, _dbToY(db, H) - 2);
    }

    // Fill
    _specCtx.beginPath(); _specCtx.moveTo(0, H);
    for (let i = 0; i < N; i++) {
        _specCtx.lineTo((i / N) * W, _dbToY(bins[i], H));
    }
    _specCtx.lineTo(W, H); _specCtx.closePath();
    const g = _specCtx.createLinearGradient(0, 0, 0, H);
    g.addColorStop(0,   'rgba(45,212,191,0.65)');
    g.addColorStop(0.5, 'rgba(45,212,191,0.18)');
    g.addColorStop(1,   'rgba(45,212,191,0.02)');
    _specCtx.fillStyle = g; _specCtx.fill();

    // Line
    _specCtx.beginPath();
    for (let i = 0; i < N; i++) {
        const x = (i / N) * W, y = _dbToY(bins[i], H);
        i === 0 ? _specCtx.moveTo(x, y) : _specCtx.lineTo(x, y);
    }
    _specCtx.strokeStyle = 'rgba(45,212,191,0.9)'; _specCtx.lineWidth = 1.5; _specCtx.stroke();

    // Centre line
    _specCtx.strokeStyle = 'rgba(168,85,247,0.6)'; _specCtx.lineWidth = 1;
    _specCtx.setLineDash([4, 4]);
    _specCtx.beginPath(); _specCtx.moveTo(W/2, 0); _specCtx.lineTo(W/2, H); _specCtx.stroke();
    _specCtx.setLineDash([]);
}

function _pushWaterfall(bins) {
    if (!_wfCtx || !bins || !_wfImage) return;
    const W = _wfW, N = bins.length, row = W * 4;
    const existing = _wfCtx.getImageData(0, 0, W, WATERFALL_H);
    const data = _wfImage.data;
    data.set(existing.data.subarray(0, (WATERFALL_H - 1) * row), row);
    for (let x = 0; x < W; x++) {
        const db = bins[Math.floor((x / W) * N)] ?? NOISE_FLOOR;
        const [r, g, b] = _dbToColor(db);
        const px = x * 4;
        data[px] = r; data[px+1] = g; data[px+2] = b; data[px+3] = 255;
    }
    _wfCtx.putImageData(_wfImage, 0, 0);
}

function _dbToY(db, H) {
    return ((SIGNAL_TOP - Math.max(NOISE_FLOOR, Math.min(SIGNAL_TOP, db)))
        / (SIGNAL_TOP - NOISE_FLOOR)) * H;
}

function _dbToColor(db) {
    const n = Math.max(0, Math.min(1, (db - NOISE_FLOOR) / (SIGNAL_TOP - NOISE_FLOOR)));
    const stops = [[0,0,0],[0,0,180],[0,180,200],[0,180,0],[200,200,0],[255,0,0]];
    const seg = (stops.length - 1) * n;
    const lo  = Math.floor(seg), hi = Math.min(lo + 1, stops.length - 1), t = seg - lo;
    return stops[lo].map((c, i) => Math.round(c + t * (stops[hi][i] - c)));
}

// ── State helpers ─────────────────────────────────────────────────────────────

function _applyState(s) {
    if (s.freq_hz  != null) { _freqHz  = s.freq_hz;  _syncFreqDisplay(); }
    if (s.mode     != null) { _mode    = s.mode;      _syncModeDisplay(); }
    if (s.squelch  != null) { _squelch = s.squelch;   _syncSquelchDisplay(); }
    if (s.running  != null) { _running = s.running;   _updateRunPill(); }
}

function _syncFreqDisplay() {
    setText('sdrFreqDisp', (_freqHz / 1e6).toFixed(3) + ' MHz');
}

function _syncModeDisplay() {
    const labels = { wfm: 'Wide FM', am: 'AM', nfm: 'Narrow FM' };
    setText('sdrModeDisp', labels[_mode] ?? _mode.toUpperCase());
    document.querySelectorAll('.sdr-mode-btn').forEach(b =>
        b.classList.toggle('active', b.dataset.mode === _mode));
}

function _syncSquelchDisplay() {
    const sl = document.getElementById('sdrSqSlider');
    if (sl) sl.value = _squelch;
    setText('sdrSqVal', _squelch.toFixed(0) + ' dB');
}

function _updateRunPill() {
    setPill('sdrRunPill', 'sdrRunPill-t', _running, 'Running', 'Stopped', _running ? 'on' : 'off');
}

function _setBtn(id, disabled) {
    const el = document.getElementById(id);
    if (el) el.disabled = disabled;
}

async function _fetchState() {
    try { _applyState(await (await fetch('/api/sdr/state')).json()); } catch (_) {}
}

// ── Utilities ─────────────────────────────────────────────────────────────────

function _detectBand(hz) {
    if (hz >= 88e6  && hz <= 108e6)  return 'fm';
    if (hz >= 118e6 && hz <= 137e6)  return 'airband';
    if (hz >= 156e6 && hz <= 162e6)  return 'marine';
    return 'custom';
}

function _suggestName() {
    const mhz  = (_freqHz / 1e6).toFixed(3);
    const band = _detectBand(_freqHz);
    const prefix = { fm: 'FM', airband: 'Aviation', marine: 'Marine', custom: 'SDR' };
    return `${prefix[band]} ${mhz}`;
}

function _esc(str) {
    return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}