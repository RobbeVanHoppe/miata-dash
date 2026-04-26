from src.car_state import SharedState
from src.bus.i2c_master_esp32 import I2CMaster  # Changed import
from src.bus.i2c_commander import I2CCommander
from src.bus.message import MessageNode
from src.handlers.sensor_handler import SensorHandler
from src.handlers.gps_handler import GpsHandler
from src.handlers.imu_handler import ImuHandler
from src.web.app import app, init_app
from src.web.sdr_routes import sdr_bp, init_sdr


def main():
    # 1. Shared state
    state = SharedState()

    # 2. I2C master (polling ESP32 at 0x08) + commander (sending)
    i2c = I2CMaster(state=state)
    # Register handlers - they'll be invoked based on payload type
    i2c.register(MessageNode.NODE_SENSOR_ARDUINO, SensorHandler())
    i2c.register(MessageNode.NODE_GPS_ARDUINO, GpsHandler())
    # Note: IMU data can be added later if needed (currently not sent separately)
    i2c.start()

    commander = I2CCommander()
    commander.start(bus=i2c._bus)

    # 3. Flask app — register SDR blueprint + Sock extension
    app.register_blueprint(sdr_bp)
    init_sdr(app)          # wires up flask-sock and creates the SDRReceiver
    # (receiver is NOT started yet — user hits Start in UI)

    # 4. Wire car state + commander into the app
    init_app(state, commander)

    print("Miata Dash - RPi Node Online")
    print("Connected to: ESP32 Unified Sensor Node @ I2C 0x08")
    print("Dashboard available at http://localhost:5000")
    # Note: use_reloader=False is required when flask-sock is in use
    app.run(host="0.0.0.0", port=5000, use_reloader=False)


if __name__ == "__main__":
    main()