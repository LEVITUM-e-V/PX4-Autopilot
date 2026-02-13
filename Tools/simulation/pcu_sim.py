#!/usr/bin/env python3
"""
PCU (Power Control Unit) telemetry simulator for PX4 SITL testing.

Creates a pseudo-terminal (PTY) pair and sends periodic ASCII CSV lines
simulating PCU telemetry data, which the pcu_serial driver
can consume.

Usage:
    python3 pcu_sim.py [--rate HZ] [--profile PROFILE]

    Then start the driver in the PX4 shell:
        pcu_serial start -d /dev/pts/<N>
"""

import argparse
import math
import os
import random
import signal
import sys
import time


def make_steady_state(t):
    """Nominal operating PCU values with minor noise."""
    return [
        48.0 + random.gauss(0, 0.1),        # [0] stack voltage (V)
        12.5 + random.gauss(0, 0.2),         # [1] stack current (A)
        600.0 + random.gauss(0, 5.0),        # [2] stack power (W)
        62.0 + random.gauss(0, 0.3),         # [3] stack temperature (C)
        85.0 + random.gauss(0, 0.5),         # [4] state of charge (%)
        350.0 + random.gauss(0, 1.0),        # [5] H2 pressure (bar * 10)
        58.0 + random.gauss(0, 0.2),         # [6] coolant temperature (C)
        1.0,                                  # [7] system state (1=running)
        0.0,                                  # [8] fault code
        random.uniform(0.95, 1.0),           # [9] cell voltage min (V)
    ]


def make_ramp_up(t):
    """Simulates PCU ramp-up from cold start over ~60 seconds."""
    frac = min(t / 60.0, 1.0)
    return [
        24.0 + 24.0 * frac + random.gauss(0, 0.1),   # stack voltage
        0.5 + 12.0 * frac + random.gauss(0, 0.1),     # stack current
        12.0 + 588.0 * frac + random.gauss(0, 3.0),   # stack power
        25.0 + 37.0 * frac + random.gauss(0, 0.2),    # stack temperature
        80.0 + 5.0 * frac + random.gauss(0, 0.3),     # SOC
        200.0 + 150.0 * frac + random.gauss(0, 1.0),  # H2 pressure
        22.0 + 36.0 * frac + random.gauss(0, 0.2),    # coolant temperature
        2.0 if frac < 1.0 else 1.0,                    # state (2=warmup, 1=running)
        0.0,                                            # fault code
        0.5 + 0.5 * frac,                              # cell voltage min
    ]


def make_fault(t):
    """Simulates a fault condition: voltage drops, fault code set."""
    cycle = t % 30.0
    fault_active = cycle > 20.0
    return [
        48.0 if not fault_active else 32.0 + random.gauss(0, 1.0),   # stack voltage
        12.5 if not fault_active else 5.0 + random.gauss(0, 0.5),    # stack current
        600.0 if not fault_active else 160.0 + random.gauss(0, 10),  # stack power
        62.0 + (15.0 if fault_active else 0.0),                       # stack temperature
        85.0 - (10.0 if fault_active else 0.0),                       # SOC
        350.0 - (100.0 if fault_active else 0.0),                     # H2 pressure
        58.0 + (20.0 if fault_active else 0.0),                       # coolant temperature
        3.0 if fault_active else 1.0,                                  # state (3=fault)
        42.0 if fault_active else 0.0,                                 # fault code
        0.6 if fault_active else 0.98,                                 # cell voltage min
    ]


PROFILES = {
    "steady": make_steady_state,
    "ramp": make_ramp_up,
    "fault": make_fault,
}


def main():
    parser = argparse.ArgumentParser(description="PCU telemetry simulator for PX4 SITL")
    parser.add_argument("--rate", type=float, default=10.0,
                        help="Telemetry send rate in Hz (default: 10)")
    parser.add_argument("--profile", choices=PROFILES.keys(), default="steady",
                        help="Data profile: steady, ramp, or fault (default: steady)")
    args = parser.parse_args()

    master_fd, slave_fd = os.openpty()
    slave_path = os.ttyname(slave_fd)

    print(f"PCU simulator started")
    print(f"  PTY path : {slave_path}")
    print(f"  Profile  : {args.profile}")
    print(f"  Rate     : {args.rate} Hz")
    print(f"")
    print(f"Start the driver with:")
    print(f"  pcu_serial start -d {slave_path}")
    print(f"")
    sys.stdout.flush()

    running = True

    def handle_signal(sig, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    profile_fn = PROFILES[args.profile]
    interval = 1.0 / args.rate
    t_start = time.monotonic()
    count = 0

    while running:
        t_now = time.monotonic()
        t_elapsed = t_now - t_start

        values = profile_fn(t_elapsed)
        line = ",".join(f"{v:.4f}" for v in values) + "\n"

        try:
            os.write(master_fd, line.encode("ascii"))
        except OSError:
            break

        count += 1
        if count % int(args.rate * 5) == 0:
            print(f"[{t_elapsed:7.1f}s] sent {count} lines", flush=True)

        # Sleep until next interval
        t_next = t_start + count * interval
        sleep_time = t_next - time.monotonic()
        if sleep_time > 0:
            time.sleep(sleep_time)

    os.close(master_fd)
    os.close(slave_fd)
    print(f"\nStopped after {count} lines.")


if __name__ == "__main__":
    main()
