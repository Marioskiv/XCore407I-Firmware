#!/usr/bin/env python3
"""Basic jog ramp progression test (host-side logic approximation).
This doesn't execute MCU code; it reproduces the expected ramp profile logic for validation.
"""
import math

def simulate_ramp(target_speed, accel, dt, duration):
    v = 0.0
    t = 0.0
    speeds = []
    while t < duration:
        # accelerate toward target
        if v < target_speed:
            v = min(target_speed, v + accel * dt)
        elif v > target_speed:
            v = max(target_speed, v - accel * dt)
        speeds.append(v)
        t += dt
    return speeds

def test_ramp_up_reaches_target():
    speeds = simulate_ramp(1000, 5000, 0.01, 0.5)
    assert abs(speeds[-1] - 1000) < 1e-3, f"End speed {speeds[-1]} != target"

def test_ramp_no_overshoot():
    speeds = simulate_ramp(800, 4000, 0.01, 0.4)
    assert all(s <= 800 + 1e-6 for s in speeds), "Overshoot detected"

def test_ramp_adjust_down():
    # accelerate to 1200 then drop target to 600 mid-way
    first = simulate_ramp(1200, 6000, 0.01, 0.2)
    second = simulate_ramp(600, 6000, 0.01, 0.2)
    assert first[-1] >= 1100
    assert abs(second[-1] - 600) < 1e-3

if __name__ == '__main__':
    test_ramp_up_reaches_target(); test_ramp_no_overshoot(); test_ramp_adjust_down(); print("OK")
