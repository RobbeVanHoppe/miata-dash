import pytest
from src.handlers.gps_handler import GpsHandler
from src.car_state import SharedState
from src.bus.message import Message, MessageType, MessageNode


def make_message(payload: str, msg_type=MessageType.TYPE_DATA) -> Message:
    return Message(
        type=msg_type,
        source=MessageNode.NODE_GPS_ARDUINO,
        destination=MessageNode.NODE_ESP32,
        payload=payload,
        length=len(payload),
    )


@pytest.fixture
def handler():
    return GpsHandler()


@pytest.fixture
def state():
    return SharedState()


class TestGpsHandlerHappyPath:

    def test_valid_fix(self, handler, state):
        handler.handle_message(make_message("51.5074,-0.1278,60,8"), state)
        snap = state.snapshot()
        assert snap["gps_valid"] is True
        assert snap["gps_lat"] == pytest.approx(51.5074)
        assert snap["gps_lon"] == pytest.approx(-0.1278)
        assert snap["gps_speed"] == pytest.approx(60.0)
        assert snap["gps_sats"] == 8

    def test_negative_lat_lon(self, handler, state):
        handler.handle_message(make_message("-33.8688,151.2093,0,6"), state)
        snap = state.snapshot()
        assert snap["gps_lat"] == pytest.approx(-33.8688)
        assert snap["gps_lon"] == pytest.approx(151.2093)

    def test_zero_speed(self, handler, state):
        handler.handle_message(make_message("51.5074,-0.1278,0,4"), state)
        assert state.snapshot()["gps_speed"] == pytest.approx(0.0)


class TestGpsHandlerEdgeCases:

    def test_error_message_sets_invalid(self, handler, state):
        # First get a valid fix
        handler.handle_message(make_message("51.5074,-0.1278,60,8"), state)
        assert state.snapshot()["gps_valid"] is True
        # Then receive an error
        handler.handle_message(make_message("", MessageType.TYPE_ERROR), state)
        assert state.snapshot()["gps_valid"] is False

    def test_missing_fields_sets_invalid(self, handler, state):
        handler.handle_message(make_message("51.5074,-0.1278"), state)
        assert state.snapshot()["gps_valid"] is False

    def test_empty_payload_sets_invalid(self, handler, state):
        handler.handle_message(make_message(""), state)
        assert state.snapshot()["gps_valid"] is False

    def test_non_numeric_sets_invalid(self, handler, state):
        handler.handle_message(make_message("invalid,data,here,x"), state)
        assert state.snapshot()["gps_valid"] is False

    def test_non_data_message_ignored(self, handler, state):
        msg = make_message("51.5074,-0.1278,60,8", MessageType.TYPE_INFO)
        handler.handle_message(msg, state)
        assert state.snapshot()["gps_valid"] is False  # state unchanged