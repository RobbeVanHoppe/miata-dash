from car_state import SharedState
from bus.i2c_master import I2CMaster
from bus.message import MessageNode
from handlers.sensor_handler import SensorHandler
from handlers.gps_handler import GpsHandler
from handlers.imu_handler import ImuHandler
from web.app import app, init_app


def main():
    # 1. Shared state
    state = SharedState()

    # 2. I2C master with handlers registered
    i2c = I2CMaster(state=state)
    i2c.register(MessageNode.NODE_SENSOR_ARDUINO, SensorHandler())
    i2c.register(MessageNode.NODE_GPS_ARDUINO, GpsHandler())
    i2c.register(MessageNode.NODE_MPU9250_ARDUINO, ImuHandler())
    i2c.start()

    # 3. Flask app
    init_app(state)
    print("Miata Dash - RPi Node Online")
    print("Dashboard available at http://localhost:5000")
    app.run(host="0.0.0.0", port=5000)


if __name__ == "__main__":
    main()