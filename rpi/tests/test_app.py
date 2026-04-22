"""
test_app.py — Flask route tests for the updated app.py

Covers:
  - GET  /               → 200, HTML
  - GET  /api/state      → 200, correct JSON shape and values
  - POST /api/command    → all branches:
      - missing body / missing key  → 400
      - no I2C master registered    → 200 ok + note
      - I2C master present, send ok → 200 ok
      - I2C master present, send raises → 500
  - GET  /api/efi/connect  (POST 501)
  - GET  /api/efi/state    → 501
  - GET  /api/sdr/tune     → 501, echoes freq param
"""

import pytest
from unittest.mock import MagicMock, patch

from src.car_state import SharedState
from src.web.app import app, init_app


# ── Fixtures ──────────────────────────────────────────────────────────────────

@pytest.fixture(autouse=True)
def reset_app_globals():
    """
    Ensure each test starts with a clean app state.
    init_app() writes to module-level globals; we reset them after every test.
    """
    yield
    init_app(SharedState(), i2c=None)


@pytest.fixture
def state():
    s = SharedState()
    s.update(
        rpm=3000, water_temp_c=88, oil_pressure=3.2,
        lights_on=True, beam_on=False, lights_up=True,
        parking_brake_on=False,
        gps_valid=True, gps_lat=51.5074, gps_lon=-0.1278,
        gps_speed=60.0, gps_sats=8,
        imu_x=0.02, imu_y=0.98, imu_z=0.15,
    )
    return s


@pytest.fixture
def client(state):
    init_app(state, i2c=None)
    app.config['TESTING'] = True
    with app.test_client() as c:
        yield c


@pytest.fixture
def mock_i2c():
    """A mock I2CMaster with a send() method."""
    m = MagicMock()
    m.send = MagicMock()
    return m


# ── Dashboard page ────────────────────────────────────────────────────────────

class TestDashboardPage:

    def test_returns_200(self, client):
        r = client.get('/')
        assert r.status_code == 200

    def test_returns_html(self, client):
        r = client.get('/')
        assert b'<!DOCTYPE html>' in r.data or b'<html' in r.data

    def test_contains_miata(self, client):
        r = client.get('/')
        assert b'Miata' in r.data


# ── GET /api/state ────────────────────────────────────────────────────────────

class TestApiState:

    def test_returns_200(self, client):
        assert client.get('/api/state').status_code == 200

    def test_returns_json(self, client):
        r = client.get('/api/state')
        assert r.content_type == 'application/json'

    def test_contains_all_carstate_keys(self, client):
        data = client.get('/api/state').get_json()
        expected_keys = {
            'rpm', 'water_temp_c', 'oil_pressure',
            'lights_on', 'beam_on', 'lights_up', 'parking_brake_on',
            'gps_valid', 'gps_lat', 'gps_lon', 'gps_speed', 'gps_alt', 'gps_sats',
            'imu_x', 'imu_y', 'imu_z',
        }
        assert expected_keys.issubset(data.keys())

    def test_reflects_current_state(self, client, state):
        data = client.get('/api/state').get_json()
        assert data['rpm'] == 3000
        assert data['water_temp_c'] == 88
        assert data['lights_on'] is True
        assert data['gps_valid'] is True
        assert data['gps_lat'] == pytest.approx(51.5074)
        assert data['imu_y'] == pytest.approx(0.98)

    def test_reflects_state_update(self, client, state):
        state.update(rpm=7500)
        data = client.get('/api/state').get_json()
        assert data['rpm'] == 7500

    def test_default_state_all_zeros(self):
        """Fresh SharedState should produce all-zero / all-False values."""
        init_app(SharedState(), i2c=None)
        app.config['TESTING'] = True
        with app.test_client() as c:
            data = c.get('/api/state').get_json()
        assert data['rpm'] == 0
        assert data['lights_on'] is False
        assert data['gps_valid'] is False


# ── POST /api/command ─────────────────────────────────────────────────────────

class TestApiCommand:

    # ── Validation ──

    def test_empty_body_returns_400(self, client):
        r = client.post('/api/command', content_type='application/json', data='')
        assert r.status_code == 400

    def test_missing_command_key_returns_400(self, client):
        r = client.post('/api/command', json={'not_command': 'LIGHTS_ON'})
        assert r.status_code == 400

    def test_non_json_body_returns_400(self, client):
        r = client.post('/api/command', content_type='text/plain', data='LIGHTS_ON')
        assert r.status_code == 400

    def test_400_body_contains_error_key(self, client):
        r = client.post('/api/command', json={})
        assert 'error' in r.get_json()

    # ── No I2C master (dev / mock mode) ──

    def test_no_i2c_returns_200(self, client):
        r = client.post('/api/command', json={'command': 'LIGHTS_ON'})
        assert r.status_code == 200

    def test_no_i2c_ok_true(self, client):
        data = client.post('/api/command', json={'command': 'LIGHTS_ON'}).get_json()
        assert data['ok'] is True

    def test_no_i2c_note_field(self, client):
        data = client.post('/api/command', json={'command': 'LIGHTS_ON'}).get_json()
        assert 'note' in data

    @pytest.mark.parametrize('command', [
        'LIGHTS_ON', 'LIGHTS_OFF', 'BEAM_ON', 'BEAM_OFF',
        'RETRACT_UP', 'RETRACT_DOWN', 'ACC_ON', 'ACC_OFF',
        'INTERIOR_ON', 'INTERIOR_OFF',
    ])
    def test_all_known_commands_accepted_without_i2c(self, client, command):
        r = client.post('/api/command', json={'command': command})
        assert r.status_code == 200

    # ── With I2C master ──

    def test_with_i2c_calls_send(self, state, mock_i2c):
        init_app(state, i2c=mock_i2c)
        app.config['TESTING'] = True
        with app.test_client() as c:
            r = c.post('/api/command', json={'command': 'LIGHTS_ON'})
        assert r.status_code == 200
        mock_i2c.send.assert_called_once()

    def test_with_i2c_passes_correct_node(self, state, mock_i2c):
        from src.bus.message import MessageNode
        init_app(state, i2c=mock_i2c)
        app.config['TESTING'] = True
        with app.test_client() as c:
            c.post('/api/command', json={'command': 'BEAM_ON'})
        call_args = mock_i2c.send.call_args
        assert call_args[0][0] == MessageNode.NODE_SENSOR_ARDUINO

    def test_with_i2c_message_contains_command(self, state, mock_i2c):
        from src.bus.message import MessageType
        init_app(state, i2c=mock_i2c)
        app.config['TESTING'] = True
        with app.test_client() as c:
            c.post('/api/command', json={'command': 'RETRACT_UP'})
        msg = mock_i2c.send.call_args[0][1]
        assert msg.payload == 'RETRACT_UP'
        assert msg.type == MessageType.TYPE_COMMAND

    def test_with_i2c_ok_true(self, state, mock_i2c):
        init_app(state, i2c=mock_i2c)
        app.config['TESTING'] = True
        with app.test_client() as c:
            data = c.post('/api/command', json={'command': 'LIGHTS_ON'}).get_json()
        assert data['ok'] is True

    # ── I2C send raises ──

    def test_i2c_exception_returns_500(self, state, mock_i2c):
        mock_i2c.send.side_effect = OSError('I2C bus error')
        init_app(state, i2c=mock_i2c)
        app.config['TESTING'] = True
        with app.test_client() as c:
            r = c.post('/api/command', json={'command': 'LIGHTS_ON'})
        assert r.status_code == 500

    def test_i2c_exception_body_contains_error(self, state, mock_i2c):
        mock_i2c.send.side_effect = RuntimeError('nack')
        init_app(state, i2c=mock_i2c)
        app.config['TESTING'] = True
        with app.test_client() as c:
            data = c.post('/api/command', json={'command': 'LIGHTS_ON'}).get_json()
        assert 'error' in data


# ── Stub endpoints ────────────────────────────────────────────────────────────

class TestStubEndpoints:

    def test_efi_connect_returns_501(self, client):
        r = client.post('/api/efi/connect')
        assert r.status_code == 501

    def test_efi_state_returns_501(self, client):
        r = client.get('/api/efi/state')
        assert r.status_code == 501

    def test_sdr_tune_returns_501(self, client):
        r = client.get('/api/sdr/tune?freq=88.0')
        assert r.status_code == 501

    def test_sdr_tune_echoes_freq(self, client):
        data = client.get('/api/sdr/tune?freq=121.5').get_json()
        assert data['requested_freq'] == pytest.approx(121.5)

    def test_sdr_tune_missing_freq(self, client):
        """Should still return 501, not crash."""
        r = client.get('/api/sdr/tune')
        assert r.status_code == 501

    def test_efi_state_is_json(self, client):
        r = client.get('/api/efi/state')
        assert r.content_type == 'application/json'


# ── init_app ──────────────────────────────────────────────────────────────────

class TestInitApp:

    def test_init_without_i2c(self):
        """init_app(state) should not raise when i2c is omitted."""
        init_app(SharedState())

    def test_init_with_i2c(self, mock_i2c):
        init_app(SharedState(), i2c=mock_i2c)

    def test_state_is_reflected_immediately(self):
        s = SharedState()
        s.update(rpm=1234)
        init_app(s)
        app.config['TESTING'] = True
        with app.test_client() as c:
            data = c.get('/api/state').get_json()
        assert data['rpm'] == 1234