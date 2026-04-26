from flask import Flask, jsonify, render_template, request
import hashlib
import json
import time
from src.car_state import SharedState
from src.bus.message import MessageNode
from src.bus.i2c_master import I2CMaster

app = Flask(__name__, static_folder='./static', template_folder='./templates')

_state: SharedState = None
_last_esp32_update: float = 0


def init_app(state: SharedState):
    global _state
    _state = state

# ── Pages ─────────────────────────────────────────────────────────────────────


@app.route('/')
def dashboard():
    return render_template('dashboard.html')

# ── Data API ──────────────────────────────────────────────────────────────────


@app.route('/api/esp32/data', methods=['POST'])
def api_esp32_data():
    global _last_esp32_update
    data = request.get_json(silent=True)
    if not data:
        return jsonify({'error': 'no data'}), 400
    _last_esp32_update = time.time()
    _state.update(
        rpm=data.get('rpm', 0),
        water_temp_c=data.get('water_temp_c', 0),
        oil_pressure=data.get('oil_pressure', 0.0),
        lights_on=data.get('lights_on', False),
        beam_on=data.get('beam_on', False),
        lights_up=data.get('lights_up', False),
        parking_brake_on=data.get('parking_brake_on', False),
        gps_valid=data.get('gps_valid', False),
        gps_lat=data.get('gps_lat', 0.0),
        gps_lon=data.get('gps_lon', 0.0),
        gps_speed=data.get('gps_speed', 0.0),
        gps_alt=data.get('gps_alt', 0.0),
        gps_sats=data.get('gps_sats', 0),
        imu_x=data.get('imu_x', 0.0),
        imu_y=data.get('imu_y', 0.0),
        imu_z=data.get('imu_z', 0.0),
    )
    return jsonify({'ok': True})


@app.route('/api/state')
def api_state():
    snap = _state.snapshot()
    if snap.get('avg_btn_event'):
        _state.update(avg_btn_event=False)
    snap['esp32_connected'] = (time.time() - _last_esp32_update) < 1.0

    body = json.dumps(snap, separators=(',', ':'), sort_keys=True)
    etag = hashlib.md5(body.encode()).hexdigest()[:8]

    if request.headers.get('If-None-Match') == etag:
        return '', 304

    resp = app.response_class(body, mimetype='application/json')
    resp.headers['ETag'] = etag
    return resp


@app.route('/api/command', methods=['POST'])
def api_command():
    data = request.get_json(silent=True)
    if not data or 'command' not in data:
        return jsonify({'error': 'missing command'}), 400
    print(f'[api] command received: {data["command"]}')
    return jsonify({'ok': True})