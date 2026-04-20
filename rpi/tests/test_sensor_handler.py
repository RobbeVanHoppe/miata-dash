import pytest
from src.handlers.sensor_handler import SensorHandler
from src.car_state import SharedState
from src.bus.message import Message, MessageType, MessageNode


def make_message(payload: str) -> Message:
    return Message(
        type=MessageType.TYPE_DATA,
        source=MessageNode.NODE_SENSOR_ARDUINO,
        destination=MessageNode.NODE_ESP32,
        payload=payload,
        length=len(payload),
    )


@pytest.fixture
def handler():
    return SensorHandler()


@pytest.fixture
def state():
    return SharedState()


class TestSensorHandlerHappyPath:

    def test_lights_on(self, handler, state):
        handler.handle_message(make_message("1,0,0,0,0,0"), state)
        assert state.snapshot()["lights_on"] is True

    def test_lights_off(self, handler, state):
        handler.handle_message(make_message("0,0,0,0,0,0"), state)
        assert state.snapshot()["lights_on"] is False

    def test_beam_on(self, handler, state):
        handler.handle_message(make_message("0,1,0,0,0,0"), state)
        assert state.snapshot()["beam_on"] is True

    def test_lights_up(self, handler, state):
        handler.handle_message(make_message("0,0,1,0,0,0"), state)
        assert state.snapshot()["lights_up"] is True

    def test_rpm(self, handler, state):
        handler.handle_message(make_message("0,0,0,3500,0,0"), state)
        assert state.snapshot()["rpm"] == 3500

    def test_water_temp(self, handler, state):
        handler.handle_message(make_message("0,0,0,0,92,0"), state)
        assert state.snapshot()["water_temp_c"] == 92

    def test_oil_pressure_decimal(self, handler, state):
        handler.handle_message(make_message("0,0,0,0,0,32"), state)
        assert state.snapshot()["oil_pressure"] == pytest.approx(3.2)

    def test_full_payload(self, handler, state):
        handler.handle_message(make_message("1,1,1,6500,95,45"), state)
        snap = state.snapshot()
        assert snap["lights_on"] is True
        assert snap["beam_on"] is True
        assert snap["lights_up"] is True
        assert snap["rpm"] == 6500
        assert snap["water_temp_c"] == 95
        assert snap["oil_pressure"] == pytest.approx(4.5)


class TestSensorHandlerEdgeCases:

    def test_zero_rpm(self, handler, state):
        handler.handle_message(make_message("0,0,0,0,0,0"), state)
        assert state.snapshot()["rpm"] == 0

    def test_max_rpm(self, handler, state):
        handler.handle_message(make_message("0,0,0,9999,0,0"), state)
        assert state.snapshot()["rpm"] == 9999

    def test_non_data_message_ignored(self, handler, state):
        msg = Message(
            type=MessageType.TYPE_ERROR,
            source=MessageNode.NODE_SENSOR_ARDUINO,
            destination=MessageNode.NODE_ESP32,
            payload="1,0,0,3000,88,32",
            length=16,
        )
        handler.handle_message(msg, state)
        assert state.snapshot()["rpm"] == 0  # state unchanged

    def test_empty_payload_does_not_crash(self, handler, state):
        handler.handle_message(make_message(""), state)
        assert state.snapshot()["rpm"] == 0  # state unchanged

    def test_missing_fields_does_not_crash(self, handler, state):
        handler.handle_message(make_message("1,0,1"), state)
        assert state.snapshot()["rpm"] == 0  # state unchanged

    def test_non_numeric_does_not_crash(self, handler, state):
        handler.handle_message(make_message("a,b,c,d,e,f"), state)
        assert state.snapshot()["rpm"] == 0  # state unchanged