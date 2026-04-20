import pytest
import threading
from src.car_state import SharedState


class TestSharedState:

    def test_default_values(self):
        state = SharedState()
        snap = state.snapshot()
        assert snap["rpm"] == 0
        assert snap["lights_on"] is False
        assert snap["gps_valid"] is False

    def test_update_single_field(self):
        state = SharedState()
        state.update(rpm=3000)
        assert state.snapshot()["rpm"] == 3000

    def test_update_multiple_fields(self):
        state = SharedState()
        state.update(rpm=5000, water_temp_c=90, lights_on=True)
        snap = state.snapshot()
        assert snap["rpm"] == 5000
        assert snap["water_temp_c"] == 90
        assert snap["lights_on"] is True

    def test_unknown_field_ignored(self):
        state = SharedState()
        state.update(nonexistent_field=999)  # should not crash
        assert "nonexistent_field" not in state.snapshot()

    def test_snapshot_is_independent_copy(self):
        state = SharedState()
        snap1 = state.snapshot()
        state.update(rpm=9000)
        snap2 = state.snapshot()
        assert snap1["rpm"] == 0
        assert snap2["rpm"] == 9000

    def test_thread_safety(self):
        state = SharedState()
        errors = []

        def writer():
            try:
                for i in range(1000):
                    state.update(rpm=i)
            except Exception as e:
                errors.append(e)

        def reader():
            try:
                for _ in range(1000):
                    state.snapshot()
            except Exception as e:
                errors.append(e)

        threads = [threading.Thread(target=writer) for _ in range(3)]
        threads += [threading.Thread(target=reader) for _ in range(3)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        assert errors == [], f"Thread safety errors: {errors}"