import pytest
import time
from src.bus.i2c_master import I2CMaster
from src.bus.mock_i2c import MockBus
from src.bus.message import MessageNode
from src.car_state import SharedState
from src.handlers.sensor_handler import SensorHandler
from src.handlers.gps_handler import GpsHandler
from src.handlers.imu_handler import ImuHandler


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


class TestI2CMasterPolling:

    def test_sensor_handler_called(self, master, state):
        master.start()
        time.sleep(0.25)  # allow at least 2 poll cycles
        master.stop()
        assert state.snapshot()["rpm"] == 1500

    def test_gps_handler_called(self, master, state):
        master.start()
        time.sleep(0.25)
        master.stop()
        snap = state.snapshot()
        assert snap["gps_valid"] is True
        assert snap["gps_lat"] == pytest.approx(51.5074)

    def test_imu_handler_called(self, master, state):
        master.start()
        time.sleep(0.25)
        master.stop()
        snap = state.snapshot()
        assert snap["imu_x"] == pytest.approx(0.02)
        assert snap["imu_y"] == pytest.approx(0.98)

    def test_stop_actually_stops(self, master, state):
        master.start()
        time.sleep(0.25)
        master.stop()
        rpm_after_stop = state.snapshot()["rpm"]
        time.sleep(0.25)
        assert state.snapshot()["rpm"] == rpm_after_stop  # no more updates

    def test_unregistered_address_does_not_crash(self, state):
        # Master with no handlers registered
        bus = MockBus()
        i2c = I2CMaster(state=state, bus=bus)
        i2c.start()
        time.sleep(0.25)
        i2c.stop()
        assert state.snapshot()["rpm"] == 0  # nothing updated, no crash


class TestFlaskIntegration:

    def test_api_state_returns_json(self, state):
        from src.web.app import app, init_app
        init_app(state)
        client = app.test_client()
        response = client.get("/api/state")
        assert response.status_code == 200
        data = response.get_json()
        assert "rpm" in data
        assert "gps_valid" in data
        assert "imu_x" in data

    def test_api_state_reflects_updates(self, state):
        from src.web.app import app, init_app
        init_app(state)
        state.update(rpm=7000, water_temp_c=102)
        client = app.test_client()
        response = client.get("/api/state")
        data = response.get_json()
        assert data["rpm"] == 7000
        assert data["water_temp_c"] == 102

    def test_dashboard_returns_html(self, state):
        from src.web.app import app, init_app
        init_app(state)
        client = app.test_client()
        response = client.get("/")
        assert response.status_code == 200
        assert b"Miata" in response.data