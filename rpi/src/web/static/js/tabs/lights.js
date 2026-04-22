/**
 * tabs/lights.js — Lighting & relay control tab
 *
 * Buttons send commands via api.sendCommand().
 * State pills mirror the actual sensor feedback from the Arduino,
 * so you always see real relay state, not just "what we sent".
 *
 * To add a new relay command:
 *   1. Add a <button onclick="LightsTab.send('YOUR_CMD')"> in dashboard.html.
 *   2. Handle it in sensorForwarder.cpp → processCommand().
 *   3. Mirror the feedback state here in update() if the Arduino reports it back.
 */

import { sendCommand } from '../api.js';
import { setPill, setText } from '../ui.js';

// Log of recent commands shown under the header card
const MAX_LOG = 1;
let _lastCmd = null;
let _lastCmdTime = null;

export function init() {
    // Expose send() on window so inline onclick= attributes work.
    // If you prefer, wire up event listeners below instead.
    window.LightsTab = { send };
}

/**
 * Called on every state poll — mirrors actual Arduino-reported state.
 * @param {object} state
 */
export function update(state) {
    // Mirror actual state from Arduino (not just what we sent)
    setPill('lc-l',  'lc-l-t',  state.lights_on,  'On',       'Off');
    setPill('lc-b',  'lc-b-t',  state.beam_on,     'Beam On',  'Off');
    setPill('lc-u',  'lc-u-t',  state.lights_up,   'Up',       'Down');

    // Update log line
    if (_lastCmd) {
        const elapsed = Math.round((Date.now() - _lastCmdTime) / 1000);
        setText('cmdlog', `↑ ${_lastCmd}  ·  ${elapsed}s ago`);
    }
}

/**
 * Send a relay command. Called from button onclick or programmatically.
 * @param {string} command
 */
export async function send(command) {
    _lastCmd = command;
    _lastCmdTime = Date.now();
    setText('cmdlog', `↑ ${command}  ·  sending…`);

    const ok = await sendCommand(command);
    setText('cmdlog', ok
        ? `↑ ${command}  ·  sent at ${new Date().toLocaleTimeString()}`
        : `⚠ ${command}  ·  send failed`
    );
}