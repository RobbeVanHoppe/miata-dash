"""
sdr_routes.py — Flask Blueprint for the SDR backend.

REST endpoints:
  POST /api/sdr/start
  POST /api/sdr/stop
  POST /api/sdr/tune           { freq_hz, mode, squelch }
  GET  /api/sdr/state
  GET  /api/sdr/presets        → grouped list
  POST /api/sdr/presets        { label, freq_hz, mode, squelch, band? }
  DELETE /api/sdr/presets/<id>
  PATCH  /api/sdr/presets/<id> { label }
  GET  /api/sdr/modes

WebSocket:
  WS /ws/sdr  → pushes FFT frames @ ~10 Hz
"""

import json
import time

from flask import Blueprint, request, jsonify
from flask_sock import Sock

from src.sdr.receiver import SDRReceiver
from src.sdr.preset_store import PresetStore
from src.sdr.presets import MODES

sdr_bp = Blueprint("sdr", __name__)
_sock  = Sock()

_rcv:   SDRReceiver | None = None
_store: PresetStore | None = None

SPECTRUM_HZ = 10


def init_sdr(app) -> SDRReceiver:
    """Wire up Sock + create receiver + preset store. Call once from main.py."""
    global _rcv, _store
    _sock.init_app(app)
    _rcv   = SDRReceiver()
    _store = PresetStore()
    return _rcv


# ── Receiver lifecycle ────────────────────────────────────────────────────────

@sdr_bp.route("/api/sdr/start", methods=["POST"])
def sdr_start():
    if _rcv is None:
        return jsonify({"error": "not initialised"}), 500
    _rcv.start()
    return jsonify(_rcv.state())


@sdr_bp.route("/api/sdr/stop", methods=["POST"])
def sdr_stop():
    if _rcv is None:
        return jsonify({"error": "not initialised"}), 500
    _rcv.stop()
    return jsonify({"ok": True})


@sdr_bp.route("/api/sdr/tune", methods=["POST"])
def sdr_tune():
    if _rcv is None:
        return jsonify({"error": "not initialised"}), 500
    body = request.get_json(silent=True) or {}
    try:
        state = _rcv.retune(
            freq_hz = body.get("freq_hz"),
            mode    = body.get("mode"),
            squelch = body.get("squelch"),
        )
        return jsonify(state)
    except ValueError as e:
        return jsonify({"error": str(e)}), 400


@sdr_bp.route("/api/sdr/state", methods=["GET"])
def sdr_state():
    if _rcv is None:
        return jsonify({"error": "not initialised"}), 500
    return jsonify(_rcv.state())


@sdr_bp.route("/api/sdr/modes", methods=["GET"])
def sdr_modes():
    return jsonify(MODES)


# ── Preset CRUD ───────────────────────────────────────────────────────────────

@sdr_bp.route("/api/sdr/presets", methods=["GET"])
def presets_list():
    """Returns presets grouped by band — ready for the UI to render."""
    if _store is None:
        return jsonify({"error": "not initialised"}), 500
    return jsonify(_store.grouped())


@sdr_bp.route("/api/sdr/presets", methods=["POST"])
def presets_add():
    """
    Body: { "label": str, "freq_hz": int, "mode": str,
            "squelch": float (opt), "band": str (opt) }
    """
    if _store is None:
        return jsonify({"error": "not initialised"}), 500
    body = request.get_json(silent=True) or {}

    label   = body.get("label", "").strip()
    freq_hz = body.get("freq_hz")
    mode    = body.get("mode")

    if not label:
        return jsonify({"error": "label is required"}), 400
    if freq_hz is None:
        return jsonify({"error": "freq_hz is required"}), 400
    if mode not in ("wfm", "am", "nfm"):
        return jsonify({"error": f"invalid mode '{mode}'"}), 400

    try:
        preset = _store.add(
            label   = label,
            freq_hz = int(freq_hz),
            mode    = mode,
            squelch = float(body.get("squelch", 0)),
            band    = body.get("band"),
        )
        return jsonify(preset), 201
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@sdr_bp.route("/api/sdr/presets/<preset_id>", methods=["DELETE"])
def presets_delete(preset_id: str):
    if _store is None:
        return jsonify({"error": "not initialised"}), 500
    deleted = _store.delete(preset_id)
    if not deleted:
        return jsonify({"error": "preset not found or is a factory preset"}), 404
    return jsonify({"ok": True, "deleted": preset_id})


@sdr_bp.route("/api/sdr/presets/<preset_id>", methods=["PATCH"])
def presets_update(preset_id: str):
    if _store is None:
        return jsonify({"error": "not initialised"}), 500
    body  = request.get_json(silent=True) or {}
    label = body.get("label", "").strip()
    if not label:
        return jsonify({"error": "label is required"}), 400
    updated = _store.update_label(preset_id, label)
    if updated is None:
        return jsonify({"error": "preset not found or is a factory preset"}), 404
    return jsonify(updated)


# ── WebSocket spectrum stream ─────────────────────────────────────────────────

@_sock.route("/ws/sdr")
def ws_sdr(ws):
    if _rcv is None:
        ws.close()
        return

    interval = 1.0 / SPECTRUM_HZ
    print("[ws/sdr] Client connected")
    try:
        while True:
            t0    = time.monotonic()
            bins  = _rcv.fft.compute()
            state = _rcv.state()

            ws.send(json.dumps({
                "bins":    bins,
                "freq_hz": state["freq_hz"],
                "mode":    state["mode"],
                "squelch": state["squelch"],
            }))

            elapsed = time.monotonic() - t0
            time.sleep(max(0.0, interval - elapsed))
    except Exception:
        pass
    finally:
        print("[ws/sdr] Client disconnected")