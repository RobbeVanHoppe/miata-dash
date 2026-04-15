//
// Created by robbe on 22/03/2026.
//
#pragma once

//#include <stdint.h>
//#include <stddef.h>
//#include <string.h>


enum MessageType : uint8_t {
    TYPE_DATA,
    TYPE_ERROR,
    TYPE_INFO,
    TYPE_PING,
    TYPE_DEBUG,
    TYPE_EVENT,
    TYPE_COMMAND,
    TYPE_UNKNOWN
};

enum MessageNode : uint8_t {
    NODE_UNKNOWN,
    NODE_ESP32,
    NODE_GPS_ARDUINO,
    NODE_SENSOR_ARDUINO,
    NODE_TM1638_ARDUINO,
    NODE_MPU9250_ARDUINO
};

inline const char *messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::TYPE_DATA: return "DATA";
        case MessageType::TYPE_INFO: return "INFO";
        case MessageType::TYPE_ERROR: return "ERROR";
        case MessageType::TYPE_PING: return "PING";
        case MessageType::TYPE_DEBUG: return "DEBUG";
        case MessageType::TYPE_EVENT: return "EVENT";
        case MessageType::TYPE_COMMAND: return "COMMAND";
        case MessageType::TYPE_UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

inline const char *messageNodeToString(MessageNode node) {
    switch (node) {
        case MessageNode::NODE_ESP32: return "ESP32";
        case MessageNode::NODE_GPS_ARDUINO: return "GPS";
        case MessageNode::NODE_SENSOR_ARDUINO: return "SENS";
        case MessageNode::NODE_TM1638_ARDUINO: return "TM16";
        case MessageNode::NODE_UNKNOWN:
        default:
            return "UNK";
    }
}

inline MessageNode parseMessageNode(const char *rawNode) {
    if (!rawNode || rawNode[0] == '\0') {
        return MessageNode::NODE_UNKNOWN;
    }

    if (strcmp(rawNode, "ESP32") == 0) {
        return MessageNode::NODE_ESP32;
    }
    if (strcmp(rawNode, "GPS") == 0) {
        return MessageNode::NODE_GPS_ARDUINO;
    }
    if (strcmp(rawNode, "SENS") == 0) {
        return MessageNode::NODE_SENSOR_ARDUINO;
    }
    if (strcmp(rawNode, "TM16") == 0) {
        return MessageNode::NODE_TM1638_ARDUINO;
    }

    return MessageNode::NODE_UNKNOWN;
}

struct __attribute__((packed)) Message {
    static constexpr size_t MaxPayloadSize = 24;   // fits in standard 32-bit I2C buffers

    MessageType type;
    MessageNode source;
    MessageNode destination;
    char payload[MaxPayloadSize];
    uint8_t length;  // how many bytes in payload are actually used

    Message(MessageType type = TYPE_UNKNOWN, MessageNode source = NODE_UNKNOWN, MessageNode destination = NODE_UNKNOWN)
    : type(type), source(source), destination(destination), payload{0}, length(0) {}

    Message(MessageType type, MessageNode source, MessageNode destination, const char *textPayload)
    : Message(type, source, destination) {
        setPayload(textPayload);
    }

    Message(MessageType type, MessageNode source, MessageNode destination, const uint8_t *data, size_t dataLength)
    : Message(type, source, destination) {
        setPayload(data, dataLength);
    }

    void setPayload(const char *textPayload) {
        if (!textPayload) {
            clearPayload();
            return;
        }

        const size_t copyLength = strnlen(textPayload, MaxPayloadSize - 1);
        memcpy(payload, textPayload, copyLength);
        payload[copyLength] = '\0';
        length = static_cast<uint8_t>(copyLength);
    }

    void setPayload(const uint8_t *data, size_t dataLength) {
        if (!data || dataLength == 0) {
            clearPayload();
            return;
        }

        const size_t copyLength = dataLength < (MaxPayloadSize - 1) ? dataLength : (MaxPayloadSize - 1);
        memcpy(payload, data, copyLength);
        payload[copyLength] = '\0';
        length = static_cast<uint8_t>(copyLength);
    }

    void clearPayload() {
        payload[0] = '\0';
        length = 0;
    }
};

static inline uint8_t nodeToAddress(MessageNode node) {
    switch (node) {
        case NODE_ESP32:          return 0x08; // Controller usually doesn't need an address, but good for logic
        case NODE_GPS_ARDUINO:    return 0x10;
        case NODE_SENSOR_ARDUINO: return 0x11;
        case NODE_TM1638_ARDUINO: return 0x12;
        case NODE_MPU9250_ARDUINO: return 0x13;

        default:                  return 0x00;
    }
}
