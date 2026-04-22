/**
 * ui.js — Shared DOM helpers
 *
 * Tiny, focused utilities. No business logic here — just the repetitive
 * DOM manipulation that every tab would otherwise duplicate.
 */

/**
 * Set element text content safely.
 * @param {string} id
 * @param {string|number} value
 */
export function setText(id, value) {
    const el = document.getElementById(id);
    if (el) el.textContent = value;
}

/**
 * Set a progress bar width (0–100%).
 * @param {string} id        Element ID of the fill div
 * @param {number} pct       0–100
 * @param {'green'|'amber'|'purple'} [colorClass]
 */
export function setBar(id, pct, colorClass) {
    const el = document.getElementById(id);
    if (!el) return;
    el.style.width = Math.min(Math.max(pct, 0), 100) + '%';
    if (colorClass) {
        el.className = 'bar-fill ' + colorClass;
    }
}

/**
 * Set a pill's on/off state and label.
 * @param {string} pillId
 * @param {string} textId
 * @param {boolean} active
 * @param {string} onText
 * @param {string} offText
 * @param {'on'|'off'|'warn'|'err'} [forceClass]  override the class
 */
export function setPill(pillId, textId, active, onText, offText, forceClass) {
    const pill = document.getElementById(pillId);
    const text = document.getElementById(textId);
    if (!pill || !text) return;
    pill.className = 'pill ' + (forceClass ?? (active ? 'on' : 'off'));
    text.textContent = active ? onText : offText;
}

/**
 * Set a G-force horizontal bar (centred, extends left or right).
 * @param {string} id     Element ID of the fill div
 * @param {number} value  G value, typically ±3
 * @param {number} [max]  Absolute max for 100% (default 3)
 */
export function setGBar(id, value, max = 3) {
    const el = document.getElementById(id);
    if (!el) return;
    const pct = Math.min(Math.abs(value) / max * 50, 50);
    if (value >= 0) {
        el.style.left = '50%';
    } else {
        el.style.left = (50 - pct) + '%';
    }
    el.style.width = pct + '%';
}

/**
 * Colour a numeric readout based on thresholds.
 * @param {string} id
 * @param {number} value
 * @param {number} warnThreshold   absolute value above which → amber
 * @param {number} critThreshold   absolute value above which → red
 */
export function setValueColor(id, value, warnThreshold, critThreshold) {
    const el = document.getElementById(id);
    if (!el) return;
    if (Math.abs(value) >= critThreshold) {
        el.style.color = 'var(--red)';
    } else if (Math.abs(value) >= warnThreshold) {
        el.style.color = 'var(--amber)';
    } else {
        el.style.color = 'var(--text)';
    }
}

/**
 * Format a float to N decimal places, returning '--' if value is 0/null/undefined
 * and showZero is false.
 * @param {number|null} value
 * @param {number} [decimals=1]
 * @param {boolean} [showZero=false]
 */
export function fmt(value, decimals = 1, showZero = false) {
    if (value == null || (!showZero && value === 0)) return '--';
    return Number(value).toFixed(decimals);
}