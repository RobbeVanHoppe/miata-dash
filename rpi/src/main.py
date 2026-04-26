from waitress import serve
from src.car_state import SharedState
from src.web.app import app, init_app
from src.web.sdr_routes import sdr_bp, init_sdr


def main():
    state = SharedState()

    app.register_blueprint(sdr_bp)
    init_sdr(app)
    init_app(state)

    print("Miata Dash - Online")
    print("Dashboard: http://192.168.4.1:5000")
    serve(app, host="0.0.0.0", port=5000, threads=4)


if __name__ == "__main__":
    main()