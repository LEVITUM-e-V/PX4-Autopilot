# PCU Serial Driver

The `pcu_serial` driver reads telemetry data from the Power Control Unit (PCU) over a serial UART connection. It expects ASCII comma-separated float values, newline-terminated, and publishes them to the `debug_array` uORB topic. On the ground station side, these appear as `DEBUG_FLOAT_ARRAY` MAVLink messages.

## Data Format

The PCU sends 10 comma-separated values per line at 115200 baud:

| Index | Field              | Unit   |
|-------|--------------------|--------|
| 0     | Stack voltage      | V      |
| 1     | Stack current      | A      |
| 2     | Stack power        | W      |
| 3     | Stack temperature  | C      |
| 4     | State of charge    | %      |
| 5     | H2 pressure        | bar*10 |
| 6     | Coolant temperature| C      |
| 7     | System state       | -      |
| 8     | Fault code         | -      |
| 9     | Cell voltage min   | V      |

System state values: `1` = running, `2` = warmup, `3` = fault.

## Configuration

### Setting the Serial Port

The port is configured via the `PCU_UART_PORT` parameter. Set it to the serial port label connected to the PCU:

```
param set PCU_UART_PORT 401
```

A reboot is required after changing the port.

### Port Label to Parameter Value Mapping

| Value | Label        |
|-------|--------------|
| 0     | Disabled     |
| 6     | UART 6       |
| 101   | TELEM 1      |
| 102   | TELEM 2      |
| 103   | TELEM 3      |
| 104   | TELEM/SERIAL 4 |
| 201   | GPS 1        |
| 202   | GPS 2        |
| 401   | EXT2         |

The default is `EXT2` (value `401`).

### Finding the Physical Connector on Pixhawk 6X

The mapping between port labels and hardware UARTs is defined in the board config (`boards/px4/fmu-v6x/default.px4board`):

| Label  | Device      | Pixhawk 6X Connector |
|--------|-------------|----------------------|
| GPS 1  | `/dev/ttyS0`| GPS1 port            |
| TELEM 3| `/dev/ttyS1`| TELEM3 port          |
| EXT2   | `/dev/ttyS3`| Debug/EXT2 port      |
| TELEM 2| `/dev/ttyS4`| TELEM2 port          |
| TELEM 1| `/dev/ttyS6`| TELEM1 port          |
| GPS 2  | `/dev/ttyS7`| GPS2 port            |

For the PCU, the default is **EXT2** which maps to `/dev/ttyS3`.

To check the mapping on any other board, look at the `CONFIG_BOARD_SERIAL_*` entries in its `.px4board` file under `boards/px4/<board>/`.

## MAVLink Telemetry Output

The driver publishes PCU data to the `debug_array` uORB topic with the name field set to `"pcu"`. PX4's MAVLink module automatically bridges this to the ground station as a `DEBUG_FLOAT_ARRAY` MAVLink message (MAVLink 2 common message set).

### How It Works

1. `pcu_serial` driver parses the serial data and publishes a `debug_array` uORB message
2. The MAVLink module subscribes to `debug_array` via its `DEBUG_FLOAT_ARRAY` stream
3. Each update is forwarded as a `DEBUG_FLOAT_ARRAY` MAVLink message to the GCS

The MAVLink message contains:
- `time_usec` — timestamp from the uORB message
- `array_id` — set to `0`
- `name` — `"pcu"` (use this to identify PCU data on the GCS side)
- `data[0..9]` — the 10 float values from the data format table above

### Default Stream Rates

The `DEBUG_FLOAT_ARRAY` stream is enabled by default in most MAVLink modes:

| MAVLink Mode | Default Rate |
|--------------|-------------|
| Normal       | 1 Hz        |
| Onboard      | 10 Hz       |
| Config (USB) | 50 Hz       |

### Adjusting the Stream Rate

To change the rate at runtime from the PX4 shell:

```
mavlink stream -d /dev/ttyS6 -s DEBUG_FLOAT_ARRAY -r 10
```

Replace `/dev/ttyS6` with the MAVLink port device (e.g., TELEM1 on Pixhawk 6X), or use `-u 14550` for a UDP connection. Set `-r 0` to disable the stream.

### Receiving on the Ground Station

In QGroundControl, `DEBUG_FLOAT_ARRAY` messages are logged in the MAVLink Inspector. For custom GCS integrations, filter for `DEBUG_FLOAT_ARRAY` messages where `name == "pcu"`.

## Manual Start

The driver can also be started manually from the PX4 shell, bypassing the parameter system:

```
pcu_serial start -d /dev/ttyS3
pcu_serial status
pcu_serial stop
```

## PCU Simulator (SITL Testing)

For testing without hardware, `Tools/simulation/pcu_sim.py` creates a pseudo-terminal and sends simulated PCU telemetry.

### Usage

In a separate terminal:

```bash
python3 Tools/simulation/pcu_sim.py
```

Output:

```
PCU simulator started
  PTY path : /dev/pts/5
  Profile  : steady
  Rate     : 10.0 Hz

Start the driver with:
  pcu_serial start -d /dev/pts/5
```

Then in the PX4 SITL shell, start the driver with the printed PTY path:

```
pcu_serial start -d /dev/pts/5
```

### Simulation Profiles

| Profile  | Description                                           |
|----------|-------------------------------------------------------|
| `steady` | Nominal operating values with minor sensor noise      |
| `ramp`   | Cold start ramp-up over 60 seconds                    |
| `fault`  | Cycles between normal operation and fault conditions   |

Select a profile with `--rate` and `--profile`:

```bash
python3 Tools/simulation/pcu_sim.py --profile fault --rate 20
```

### Why Not Use `PCU_UART_PORT` in SITL?

The SITL build has no fixed serial port mappings. The `PCU_UART_PORT` parameter exists but has no device paths to resolve against, since PTY paths (`/dev/pts/N`) are assigned dynamically by the OS. The driver must be started manually with `-d`.
