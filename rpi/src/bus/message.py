import struct
from dataclasses import dataclass
from enum import IntEnum


class MessageType(IntEnum):
    TYPE_DATA = 0
    TYPE_ERROR = 1
    TYPE_INFO = 2
    TYPE_PING = 3
    TYPE_DEBUG = 4
    TYPE_EVENT = 5
    TYPE_COMMAND = 6
    TYPE_UNKNOWN = 7


class MessageNode(IntEnum):
    NODE_UNKNOWN = 0
    NODE_ESP32 = 1
    NODE_GPS_ARDUINO = 2
    NODE_SENSOR_ARDUINO = 3
    NODE_TM1638_ARDUINO = 4
    NODE_MPU9250_ARDUINO = 5


# Must match the C++ struct __attribute__((packed)):
# uint8_t type, uint8_t source, uint8_t destination, char payload[24], uint8_t length
# Total: 28 bytes
MESSAGE_FORMAT = "BBB24sB"
MESSAGE_SIZE = struct.calcsize(MESSAGE_FORMAT)  # 28


@dataclass
class Message:
    type: MessageType
    source: MessageNode
    destination: MessageNode
    payload: str
    length: int

    @staticmethod
    def from_bytes(raw: bytes) -> "Message":
        if len(raw) != MESSAGE_SIZE:
            raise ValueError(f"Expected {MESSAGE_SIZE} bytes, got {len(raw)}")

        type_, source, dest, payload_bytes, length = struct.unpack(MESSAGE_FORMAT, raw)
        payload = payload_bytes[:length].decode("ascii", errors="replace").rstrip("\x00")

        return Message(
            type=MessageType(type_),
            source=MessageNode(source),
            destination=MessageNode(dest),
            payload=payload,
            length=length,
        )

    def to_bytes(self) -> bytes:
        payload_bytes = self.payload.encode("ascii")[:24].ljust(24, b"\x00")
        return struct.pack(
            MESSAGE_FORMAT,
            int(self.type),
            int(self.source),
            int(self.destination),
            payload_bytes,
            len(self.payload),
        )