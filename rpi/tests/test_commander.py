import pytest
import struct
from src.bus.i2c_commander import I2CCommander, VALID_COMMANDS
from src.bus.mock_i2c import MockBus
from src.bus.message import Message, MessageType, MessageNode, MESSAGE_FORMAT, MESSAGE_SIZE


@pytest.fixture
def mock_bus():
    return MockBus()


@pytest.fixture
def commander(mock_bus):
    c = I2CCommander(bus=mock_bus)
    return c


class TestI2CCommanderValidation:

    def test_valid_commands_accepted(self, commander):
        for cmd in VALID_COMMANDS:
            assert commander.send(cmd) is True

    def test_unknown_command_rejected(self, commander):
        assert commander.send("SELF_DESTRUCT") is False

    def test_empty_string_rejected(self, commander):
        assert commander.send("") is False

    def test_lowercase_rejected(self, commander):
        assert commander.send("lights_on") is False


class TestI2CCommanderWireFormat:

    def test_sends_to_correct_address(self, commander, mock_bus):
        commander.send("LIGHTS_ON")
        assert mock_bus.last_write["addr"] == 0x11

    def test_payload_is_command_string(self, commander, mock_bus):
        commander.send("LIGHTS_ON")
        raw = bytes(mock_bus.last_write["data"])
        msg = Message.from_bytes(raw)
        assert msg.payload == "LIGHTS_ON"

    def test_message_type_is_command(self, commander, mock_bus):
        commander.send("ACC_ON")
        raw = bytes(mock_bus.last_write["data"])
        msg = Message.from_bytes(raw)
        assert msg.type == MessageType.TYPE_COMMAND

    def test_source_is_rpi(self, commander, mock_bus):
        commander.send("BEAM_ON")
        raw = bytes(mock_bus.last_write["data"])
        msg = Message.from_bytes(raw)
        assert msg.source == MessageNode.NODE_ESP32

    def test_destination_is_sensor_arduino(self, commander, mock_bus):
        commander.send("RETRACT_UP")
        raw = bytes(mock_bus.last_write["data"])
        msg = Message.from_bytes(raw)
        assert msg.destination == MessageNode.NODE_SENSOR_ARDUINO

    def test_message_is_correct_size(self, commander, mock_bus):
        commander.send("LIGHTS_ON")
        assert len(mock_bus.last_write["data"]) == MESSAGE_SIZE

    def test_all_commands_produce_correct_size(self, commander, mock_bus):
        for cmd in VALID_COMMANDS:
            commander.send(cmd)
            assert len(mock_bus.last_write["data"]) == MESSAGE_SIZE


class TestI2CCommanderErrorHandling:

    def test_bus_error_returns_false(self, commander, mock_bus):
        def boom(*args, **kwargs):
            raise OSError("I2C bus error")
        mock_bus.write_i2c_block_data = boom
        assert commander.send("LIGHTS_ON") is False

    def test_bus_error_does_not_raise(self, commander, mock_bus):
        def boom(*args, **kwargs):
            raise OSError("I2C bus error")
        mock_bus.write_i2c_block_data = boom
        try:
            commander.send("LIGHTS_ON")
        except Exception as e:
            pytest.fail(f"send() raised unexpectedly: {e}")


class TestCommandRoutes:

    @pytest.fixture
    def client(self, mock_bus):
        from src.web.app import app, init_app
        from src.car_state import SharedState
        from src.bus.i2c_commander import I2CCommander
        c = I2CCommander(bus=mock_bus)
        init_app(SharedState(), c)
        app.config["TESTING"] = True
        return app.test_client()

    def test_valid_command_returns_200(self, client):
        response = client.post("/api/command",
                               json={"command": "LIGHTS_ON"},
                               content_type="application/json")
        assert response.status_code == 200
        assert response.get_json()["ok"] is True

    def test_unknown_command_returns_400(self, client):
        response = client.post("/api/command",
                               json={"command": "SELF_DESTRUCT"},
                               content_type="application/json")
        assert response.status_code == 400
        assert "valid" in response.get_json()

    def test_missing_command_field_returns_400(self, client):
        response = client.post("/api/command",
                               json={},
                               content_type="application/json")
        assert response.status_code == 400

    def test_empty_body_returns_400(self, client):
        response = client.post("/api/command", data="",
                               content_type="application/json")
        assert response.status_code == 400

    def test_list_commands_returns_all(self, client):
        response = client.get("/api/commands")
        assert response.status_code == 200
        data = response.get_json()
        assert "LIGHTS_ON" in data
        assert "RETRACT_UP" in data
        assert len(data) == len(VALID_COMMANDS)