"""
test_i2c_master.py — I2C polling and Flask integration tests.

Changes from the original:
  - init_app() now accepts an optional i2c= kwarg; tests pass it explicitly.
  - Added TestCommandForwarding class to cover the new /api/command → I2C path.
"""

import pytest
import time
from unittest.mock import MagicMock

from src.bus.i2c_master import I2CMaster
from src.bus.mock_i2c import MockBus
from src.bus.message import MessageNode, MessageType
from src.car_state import SharedState
from src.handlers.sensor_handler import SensorHandler
from src.handlers.gps_handler import GpsHandler
from src.handlers.imu_handler import ImuHandler


# ── Fixtures ──────────────────────────────────────────────────────────────────

@pytest.fixture
def state():
    return SharedState()


@pytest.fixture
def master(state):
    bus = MockBus()
    i2c = I2CMaster(state=state, bus=bus)
    i2c.register(MessageNode.NODE_SENSOR_ARDUINO, SensorHandler())
    i2c.register(MessageNode.NODE_GPS_ARDUINO, GpsHandler())
    i2c.register(MessageNode.NODE_MPU9250_ARDUINO, ImuHandler())
    return i2c


# ── Polling tests (unchanged) ─────────────────────────────────────────────────

class TestI2CMasterPolling:

    def test_sensor_handler_called(self, master, state):
        master.start()
        time.sleep(0.25)
        master.stop()
        assert state.snapshot()['rpm'] == 1500

    def test_gps_handler_called(self, master, state):
        master.start()
        time.sleep(0.25)
        master.stop()
        snap = state.snapshot()
        assert snap['gps_valid'] is True
        assert snap['gps_lat'] == pytest.approx(51.5074)

    def test_imu_handler_called(self, master, state):
        master.start()
        time.sleep(0.25)
        master.stop()
        snap = state.snapshot()
        assert snap['imu_x'] == pytest.approx(0.02)
        assert snap['imu_y'] == pytest.approx(0.98)

    def test_stop_actually_stops(self, master, state):
        master.start()
        time.sleep(0.25)
        master.stop()
        rpm_after_stop = state.snapshot()['rpm']
        time.sleep(0.25)
        assert state.snapshot()['rpm'] == rpm_after_stop

    def test_unregistered_address_does_not_crash(self, state):
        bus = MockBus()
        i2c = I2CMaster(state=state, bus=bus)
        i2c.start()
        time.sleep(0.25)
        i2c.stop()
        assert state.snapshot()['rpm'] == 0


# ── Flask integration (updated for new init_app signature) ───────────────────

class TestFlaskIntegration:

    def test_api_state_returns_json(self, state):
        from src.web.app import app, init_app
        init_app(state, i2c=None)           # ← explicit i2c=None
        client = app.test_client()
        r = client.get('/api/state')
        assert r.status_code == 200
        data = r.get_json()
        assert 'rpm' in data
        assert 'gps_valid' in data
        assert 'imu_x' in data

    def test_api_state_reflects_updates(self, state):
        from src.web.app import app, init_app
        init_app(state, i2c=None)
        state.update(rpm=7000, water_temp_c=102)
        data = app.test_client().get('/api/state').get_json()
        assert data['rpm'] == 7000
        assert data['water_temp_c'] == 102

    def test_dashboard_returns_html(self, state):
        from src.web.app import app, init_app
        init_app(state, i2c=None)
        r = app.test_client().get('/')
        assert r.status_code == 200
        assert b'Miata' in r.data


# ── Command forwarding (new) ──────────────────────────────────────────────────

class TestCommandForwarding:
    """
    Verifies that POST /api/command builds the right Message and hands it
    to the I2C master's send() method.
    """

    def _client_with_mock_i2c(self, state):
        from src.web.app import app, init_app
        mock_i2c = MagicMock()
        mock_i2c.send = MagicMock()
        init_app(state, i2c=mock_i2c)
        app.config['TESTING'] = True
        return app.test_client(), mock_i2c

    def test_command_reaches_i2c_send(self, state):
        client, mock_i2c = self._client_with_mock_i2c(state)
        r = client.post('/api/command', json={'command': 'LIGHTS_ON'})
        assert r.status_code == 200
        mock_i2c.send.assert_called_once()

    def test_command_targets_sensor_arduino(self, state):
        client, mock_i2c = self._client_with_mock_i2c(state)
        client.post('/api/command', json={'command': 'BEAM_ON'})
        node_arg = mock_i2c.send.call_args[0][0]
        assert node_arg == MessageNode.NODE_SENSOR_ARDUINO

    def test_message_type_is_command(self, state):
        client, mock_i2c = self._client_with_mock_i2c(state)
        client.post('/api/command', json={'command': 'RETRACT_UP'})
        msg = mock_i2c.send.call_args[0][1]
        assert msg.type == MessageType.TYPE_COMMAND

    def test_message_payload_matches_command(self, state):
        client, mock_i2c = self._client_with_mock_i2c(state)
        client.post('/api/command', json={'command': 'RETRACT_DOWN'})
        msg = mock_i2c.send.call_args[0][1]
        assert msg.payload == 'RETRACT_DOWN'

    def test_no_i2c_does_not_crash(self, state):
        from src.web.app import app, init_app
        init_app(state, i2c=None)
        app.config['TESTING'] = True
        r = app.test_client().post('/api/command', json={'command': 'ACC_ON'})
        assert r.status_code == 200

    def test_i2c_send_failure_returns_500(self, state):
        from src.web.app import app, init_app
        mock_i2c = MagicMock()
        mock_i2c.send.side_effect = OSError('bus busy')
        init_app(state, i2c=mock_i2c)
        app.config['TESTING'] = True
        r = app.test_client().post('/api/command', json={'command': 'LIGHTS_ON'})
        assert r.status_code == 500