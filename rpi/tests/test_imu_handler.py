import pytest
from src.handlers.imu_handler import ImuHandler
from src.car_state import SharedState
from src.bus.message import Message, MessageType, MessageNode


def make_message(payload: str) -> Message:
    return Message(
        type=MessageType.TYPE_DATA,
        source=MessageNode.NODE_MPU9250_ARDUINO,
        destination=MessageNode.NODE_ESP32,
        payload=payload,
        length=len(payload),
    )


@pytest.fixture
def handler():
    return ImuHandler()


@pytest.fixture
def state():
    return SharedState()


class TestImuHandlerHappyPath:

    def test_basic_reading(self, handler, state):
        handler.handle_message(make_message("0.02,0.98,0.15"), state)
        snap = state.snapshot()
        assert snap["imu_x"] == pytest.approx(0.02)
        assert snap["imu_y"] == pytest.approx(0.98)
        assert snap["imu_z"] == pytest.approx(0.15)

    def test_negative_values(self, handler, state):
        handler.handle_message(make_message("-0.5,-1.0,0.0"), state)
        snap = state.snapshot()
        assert snap["imu_x"] == pytest.approx(-0.5)
        assert snap["imu_y"] == pytest.approx(-1.0)
        assert snap["imu_z"] == pytest.approx(0.0)

    def test_high_g_values(self, handler, state):
        handler.handle_message(make_message("2.5,-3.1,1.0"), state)
        snap = state.snapshot()
        assert snap["imu_x"] == pytest.approx(2.5)
        assert snap["imu_y"] == pytest.approx(-3.1)


class TestImuHandlerEdgeCases:

    def test_zero_values(self, handler, state):
        handler.handle_message(make_message("0.0,0.0,0.0"), state)
        snap = state.snapshot()
        assert snap["imu_x"] == pytest.approx(0.0)
        assert snap["imu_y"] == pytest.approx(0.0)
        assert snap["imu_z"] == pytest.approx(0.0)

    def test_non_data_message_ignored(self, handler, state):
        msg = Message(
            type=MessageType.TYPE_ERROR,
            source=MessageNode.NODE_MPU9250_ARDUINO,
            destination=MessageNode.NODE_ESP32,
            payload="1.0,2.0,3.0",
            length=11,
        )
        handler.handle_message(msg, state)
        assert state.snapshot()["imu_x"] == pytest.approx(0.0)  # state unchanged

    def test_missing_fields_does_not_crash(self, handler, state):
        handler.handle_message(make_message("0.02,0.98"), state)
        assert state.snapshot()["imu_x"] == pytest.approx(0.0)  # state unchanged

    def test_empty_payload_does_not_crash(self, handler, state):
        handler.handle_message(make_message(""), state)
        assert state.snapshot()["imu_x"] == pytest.approx(0.0)  # state unchanged

    def test_non_numeric_does_not_crash(self, handler, state):
        handler.handle_message(make_message("a,b,c"), state)
        assert state.snapshot()["imu_x"] == pytest.approx(0.0)  # state unchanged