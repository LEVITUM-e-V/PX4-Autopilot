#!/usr/bin/env python3
"""
PCU (Power Control Unit) telemetry simulator for PX4 SITL testing.

Creates a pseudo-terminal (PTY) pair and sends periodic CSV telemetry
lines simulating PCU data, which the pcu_serial driver can consume.

CSV format:
    FC:<stack_voltage>,<load_current>,<power>,<energy>,<battery_voltage>,
       <battery_current>,<load_voltage>,<stack_temp_1>,<stack_temp_2>,
       <stack_temp_3>,<stack_temp_4>,<target_stack_temp>,<board_temp>,
       <h2_supply_pressure>,<tank_pressure>,<fan_speed>,<operation_state>\r\n

Usage:
    python3 pcu_sim.py [--rate HZ] [--profile PROFILE]

    Then start the driver in the PX4 shell:
        pcu_serial start -d /dev/pts/<N>
"""

import argparse
import os
import random
import signal
import sys
import time


# Operation state codes matching PCU firmware
OP_STATE_STANDBY = 0
OP_STATE_WARMUP = 1
OP_STATE_RUNNING = 2
OP_STATE_FAULT = 3


def build_csv_line(floats, operation_state):
    """Build a CSV line matching the driver's expected format.

    Format: FC:%0.3f,%0.3f,%0.5f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%u\\r\\n
    """
    line = "FC:%.3f,%.3f,%.5f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u\r\n" % (
        floats[0],   # stack_voltage
        floats[1],   # load_current
        floats[2],   # power
        floats[3],   # energy
        floats[4],   # battery_voltage
        floats[5],   # battery_current
        floats[6],   # load_voltage
        floats[7],   # stack_temp_1
        floats[8],   # stack_temp_2
        floats[9],   # stack_temp_3
        floats[10],  # stack_temp_4
        floats[11],  # target_stack_temp
        floats[12],  # board_temp
        floats[13],  # h2_supply_pressure
        floats[14],  # tank_pressure
        floats[15],  # fan_speed
        operation_state,
    )
    return line.encode('ascii')


def make_steady_state(t):
    """Nominal operating PCU values with minor noise."""
    floats = [
        48.0 + random.gauss(0, 0.1),        # [0]  stack voltage (V)
        12.5 + random.gauss(0, 0.2),         # [1]  load current (A)
        600.0 + random.gauss(0, 5.0),        # [2]  power (W)
        150.0 + t * 0.01,                    # [3]  energy (Wh)
        24.0 + random.gauss(0, 0.1),         # [4]  battery voltage (V)
        2.5 + random.gauss(0, 0.1),          # [5]  battery current (A)
        48.0 + random.gauss(0, 0.1),         # [6]  load voltage (V)
        62.0 + random.gauss(0, 0.3),         # [7]  stack temp sensor 1 (C)
        61.5 + random.gauss(0, 0.3),         # [8]  stack temp sensor 2 (C)
        62.5 + random.gauss(0, 0.3),         # [9]  stack temp sensor 3 (C)
        61.0 + random.gauss(0, 0.3),         # [10] stack temp sensor 4 (C)
        60.0,                                 # [11] target stack temp (C)
        45.0 + random.gauss(0, 0.2),         # [12] board temp (C)
        3500.0 + random.gauss(0, 10.0),      # [13] H2 supply pressure (mV)
        200.0 + random.gauss(0, 2.0),        # [14] tank pressure
        55.0 + random.gauss(0, 0.5),         # [15] fan speed (%)
    ]
    return floats, OP_STATE_RUNNING


def make_ramp_up(t):
    """Simulates PCU ramp-up from cold start over ~60 seconds."""
    frac = min(t / 60.0, 1.0)
    floats = [
        24.0 + 24.0 * frac + random.gauss(0, 0.1),   # stack voltage
        0.5 + 12.0 * frac + random.gauss(0, 0.1),     # load current
        12.0 + 588.0 * frac + random.gauss(0, 3.0),   # power
        t * 0.005,                                      # energy
        24.0 + random.gauss(0, 0.1),                   # battery voltage
        5.0 - 2.5 * frac + random.gauss(0, 0.1),      # battery current
        24.0 + 24.0 * frac + random.gauss(0, 0.1),    # load voltage
        25.0 + 37.0 * frac + random.gauss(0, 0.2),    # stack temp 1
        25.0 + 36.5 * frac + random.gauss(0, 0.2),    # stack temp 2
        25.0 + 37.5 * frac + random.gauss(0, 0.2),    # stack temp 3
        25.0 + 36.0 * frac + random.gauss(0, 0.2),    # stack temp 4
        60.0,                                           # target stack temp
        25.0 + 20.0 * frac + random.gauss(0, 0.2),    # board temp
        2000.0 + 1500.0 * frac + random.gauss(0, 10),  # H2 supply pressure
        100.0 + 100.0 * frac + random.gauss(0, 2.0),  # tank pressure
        20.0 + 35.0 * frac + random.gauss(0, 0.5),    # fan speed
    ]
    state = OP_STATE_WARMUP if frac < 1.0 else OP_STATE_RUNNING
    return floats, state


def make_fault(t):
    """Simulates a fault condition: voltage drops, fault code set."""
    cycle = t % 30.0
    fault_active = cycle > 20.0
    floats = [
        48.0 if not fault_active else 32.0 + random.gauss(0, 1.0),   # stack voltage
        12.5 if not fault_active else 5.0 + random.gauss(0, 0.5),    # load current
        600.0 if not fault_active else 160.0 + random.gauss(0, 10),  # power
        150.0 + t * 0.005,                                            # energy
        24.0 + random.gauss(0, 0.1),                                  # battery voltage
        2.5 if not fault_active else 8.0 + random.gauss(0, 0.5),     # battery current
        48.0 if not fault_active else 32.0 + random.gauss(0, 1.0),   # load voltage
        62.0 + (15.0 if fault_active else 0.0),                       # stack temp 1
        61.5 + (15.0 if fault_active else 0.0),                       # stack temp 2
        62.5 + (15.0 if fault_active else 0.0),                       # stack temp 3
        61.0 + (15.0 if fault_active else 0.0),                       # stack temp 4
        60.0,                                                          # target stack temp
        45.0 + (20.0 if fault_active else 0.0),                       # board temp
        3500.0 - (1000.0 if fault_active else 0.0),                   # H2 supply pressure
        200.0 - (100.0 if fault_active else 0.0),                     # tank pressure
        55.0 + (40.0 if fault_active else 0.0),                       # fan speed
    ]
    state = OP_STATE_FAULT if fault_active else OP_STATE_RUNNING
    return floats, state


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

    print(f"PCU simulator started (CSV mode)")
    print(f"  PTY path : {slave_path}")
    print(f"  Profile  : {args.profile}")
    print(f"  Rate     : {args.rate} Hz")
    print(f"  Format   : FC:<16 floats>,<uint>\\r\\n")
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

        floats, state = profile_fn(t_elapsed)
        line = build_csv_line(floats, state)

        try:
            os.write(master_fd, line)
        except OSError:
            break

        count += 1
        if count % int(args.rate * 5) == 0:
            print(f"[{t_elapsed:7.1f}s] sent {count} lines  state={state}  "
                  f"len={len(line)}", flush=True)

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
