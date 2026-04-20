import struct
from src.bus.message import MESSAGE_FORMAT, MessageType, MessageNode


class MockBus:
    """Simulates Arduino I2C responses for local development and testing."""

    def read_i2c_block_data(self, addr: int, register: int, length: int) -> bytes:
        # Return a fake sensor payload for each known address
        payloads = {
            0x11: "L:1,B:0,U:1,R:1500,W:88,O:3.2",  # Sensor Arduino
            0x10: "51.5074,-0.1278,60.0,10.0,8",      # GPS Arduino
            0x13: "0.02,0.98,0.15",                    # IMU Arduino
        }
        payload = payloads.get(addr, "IDLE")
        payload_bytes = payload.encode("ascii")[:24].ljust(24, b"\x00")
        raw = struct.pack(
            MESSAGE_FORMAT,
            int(MessageType.TYPE_DATA),
            int(MessageNode.NODE_SENSOR_ARDUINO),
            int(MessageNode.NODE_ESP32),
            payload_bytes,
            len(payload),
        )
        return list(raw)