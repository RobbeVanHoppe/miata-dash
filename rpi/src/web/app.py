from flask import Flask, jsonify, render_template
from src.car_state import SharedState

app = Flask(__name__)
_state: SharedState = None


def init_app(state: SharedState):
    global _state
    _state = state


@app.route("/")
def dashboard():
    return render_template("dashboard.html")


@app.route("/api/state")
def api_state():
    return jsonify(_state.snapshot())