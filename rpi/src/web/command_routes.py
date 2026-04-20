from flask import Blueprint, jsonify, request
from src.bus.i2c_commander import I2CCommander, VALID_COMMANDS

commands_bp = Blueprint("commands", __name__)
_commander: I2CCommander = None


def init_commander(commander: I2CCommander):
    global _commander
    _commander = commander


@commands_bp.route("/api/command", methods=["POST"])
def send_command():
    body = request.get_json(silent=True)
    if not body or "command" not in body:
        return jsonify({"error": "missing 'command' field"}), 400

    command = body["command"]
    if command not in VALID_COMMANDS:
        return jsonify({"error": f"unknown command '{command}'",
                        "valid": sorted(VALID_COMMANDS)}), 400

    success = _commander.send(command)
    if not success:
        return jsonify({"error": "I2C write failed"}), 502

    return jsonify({"ok": True, "command": command})


@commands_bp.route("/api/commands", methods=["GET"])
def list_commands():
    """Returns all valid command strings — useful for the dashboard UI."""
    return jsonify(sorted(VALID_COMMANDS))