import pytest
import struct
from src.bus.message import Message, MessageType, MessageNode, MESSAGE_SIZE, MESSAGE_FORMAT


def make_raw(type_, source, dest, payload: str) -> bytes:
    payload_bytes = payload.encode("ascii")[:24].ljust(24, b"\x00")
    return struct.pack(MESSAGE_FORMAT, type_, source, dest, payload_bytes, len(payload))


class TestMessageFromBytes:

    def test_basic_deserialize(self):
        raw = make_raw(0, 3, 1, "1,0,1,1500,88,32")
        msg = Message.from_bytes(raw)
        assert msg.type == MessageType.TYPE_DATA
        assert msg.source == MessageNode.NODE_SENSOR_ARDUINO
        assert msg.destination == MessageNode.NODE_ESP32
        assert msg.payload == "1,0,1,1500,88,32"

    def test_correct_size(self):
        assert MESSAGE_SIZE == 28

    def test_payload_truncated_to_length(self):
        # length field = 5, rest should be stripped
        payload_bytes = b"hello\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        raw = struct.pack(MESSAGE_FORMAT, 0, 1, 1, payload_bytes, 5)
        msg = Message.from_bytes(raw)
        assert msg.payload == "hello"

    def test_wrong_size_raises(self):
        with pytest.raises(ValueError):
            Message.from_bytes(b"\x00" * 10)

    def test_all_message_types(self):
        for msg_type in MessageType:
            raw = make_raw(int(msg_type), 1, 1, "test")
            msg = Message.from_bytes(raw)
            assert msg.type == msg_type

    def test_all_node_types(self):
        for node in MessageNode:
            raw = make_raw(0, int(node), int(node), "test")
            msg = Message.from_bytes(raw)
            assert msg.source == node
            assert msg.destination == node


class TestMessageToBytes:

    def test_roundtrip(self):
        original = Message(
            type=MessageType.TYPE_DATA,
            source=MessageNode.NODE_GPS_ARDUINO,
            destination=MessageNode.NODE_ESP32,
            payload="51.5074,-0.1278,60,8",
            length=20,
        )
        raw = original.to_bytes()
        restored = Message.from_bytes(raw)
        assert restored.type == original.type
        assert restored.source == original.source
        assert restored.destination == original.destination
        assert restored.payload == original.payload

    def test_to_bytes_correct_length(self):
        msg = Message(
            type=MessageType.TYPE_DATA,
            source=MessageNode.NODE_ESP32,
            destination=MessageNode.NODE_ESP32,
            payload="hello",
            length=5,
        )
        assert len(msg.to_bytes()) == MESSAGE_SIZE

    def test_payload_over_24_chars_truncated(self):
        long_payload = "A" * 30
        msg = Message(
            type=MessageType.TYPE_DATA,
            source=MessageNode.NODE_ESP32,
            destination=MessageNode.NODE_ESP32,
            payload=long_payload,
            length=len(long_payload),
        )
        raw = msg.to_bytes()
        assert len(raw) == MESSAGE_SIZE