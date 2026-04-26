@app.route('/api/esp32/data', methods=['POST'])
def api_esp32_data():
    data = request.get_json(silent=True)
    if not data:
        return jsonify({'error': 'no data'}), 400

    _state.update(
        rpm=data.get('rpm', 0),
        water_temp_c=data.get('water_temp_c', 0),
        oil_pressure=data.get('oil_pressure', 0.0),
        lights_on=data.get('lights_on', False),
        beam_on=data.get('beam_on', False),
        lights_up=data.get('lights_up', False),
        parking_brake_on=data.get('parking_brake_on', False),
        gps_valid=data.get('gps_valid', False),
        gps_lat=data.get('gps_lat', 0.0),
        gps_lon=data.get('gps_lon', 0.0),
        gps_speed=data.get('gps_speed', 0.0),
        gps_alt=data.get('gps_alt', 0.0),
        gps_sats=data.get('gps_sats', 0),
        imu_x=data.get('imu_x', 0.0),
        imu_y=data.get('imu_y', 0.0),
        imu_z=data.get('imu_z', 0.0),
    )
    return jsonify({'ok': True})