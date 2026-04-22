from flask import Flask, jsonify, render_template, request
from src.car_state import SharedState
from src.bus.message import MessageNode
from src.bus.i2c_master import I2CMaster

app = Flask(__name__, static_folder='./static', template_folder='./templates')

_state: SharedState = None
_i2c: I2CMaster = None


def init_app(state: SharedState, i2c: I2CMaster = None):
    """
    Wire up shared state (and optionally the I2C master for command forwarding).
    Called from main.py before app.run().
    """
    global _state, _i2c
    _state = state
    _i2c = i2c


# ── Pages ──────────────────────────────────────────────────────────────────────

@app.route('/')
def dashboard():
    return render_template('dashboard.html')


# ── Data API ───────────────────────────────────────────────────────────────────

@app.route('/api/state')
def api_state():
    return jsonify(_state.snapshot())


@app.route('/api/command', methods=['POST'])
def api_command():
    """
    Forward a relay command string to the sensor Arduino over I2C.
    Body: { "command": "LIGHTS_ON" }
    """
    data = request.get_json(silent=True)
    if not data or 'command' not in data:
        return jsonify({'error': 'missing command'}), 400

    command = data['command']

    if _i2c is None:
        # No I2C master available (e.g. running in mock/dev mode)
        print(f'[api] command (no-op, no I2C): {command}')
        return jsonify({'ok': True, 'note': 'no_i2c'})

    try:
        from src.bus.message import Message, MessageType, MessageNode
        msg = Message(
            type=MessageType.TYPE_COMMAND,
            source=MessageNode.NODE_ESP32,
            destination=MessageNode.NODE_SENSOR_ARDUINO,
            payload=command,
            length=len(command),
        )
        _i2c.send(MessageNode.NODE_SENSOR_ARDUINO, msg)
        return jsonify({'ok': True})
    except Exception as e:
        print(f'[api] command failed: {e}')
        return jsonify({'error': str(e)}), 500


# ── EFI (stub — implement when rusEFI serial is wired up) ──────────────────────

@app.route('/api/efi/connect', methods=['POST'])
def api_efi_connect():
    return jsonify({'error': 'not_implemented'}), 501


@app.route('/api/efi/state')
def api_efi_state():
    return jsonify({'error': 'not_implemented'}), 501


# ── SDR (stub — implement when pyrtlsdr is wired up) ──────────────────────────

@app.route('/api/sdr/tune')
def api_sdr_tune():
    freq = request.args.get('freq', type=float)
    return jsonify({'error': 'not_implemented', 'requested_freq': freq}), 501