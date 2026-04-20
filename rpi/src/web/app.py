from flask import Flask, jsonify, render_template
from src.car_state import SharedState
from src.web.command_routes import commands_bp, init_commander
from src.bus.i2c_commander import I2CCommander

app = Flask(__name__)
app.register_blueprint(commands_bp)

_state: SharedState = None


def init_app(state: SharedState, commander: I2CCommander):
    global _state
    _state = state
    init_commander(commander)


@app.route("/")
def dashboard():
    return render_template("dashboard.html")


@app.route("/api/state")
def api_state():
    return jsonify(_state.snapshot())