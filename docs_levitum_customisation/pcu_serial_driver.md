# PCU Serial Driver

The `pcu_serial` driver reads telemetry data from the Power Control Unit (PCU) over a serial UART connection. It expects CSV lines with the prefix `FC:`, terminated by `\r\n`, and publishes them to the `pcu_telemetry` uORB topic. A custom MAVLink stream (`PCU_TELEMETRY`, message ID 50001) forwards this data to the ground station.

## Data Format

The PCU sends CSV lines at 115200 baud in the following format:

```
FC:<stack_voltage>,<load_current>,<power>,<energy>,<battery_voltage>,<battery_current>,<load_voltage>,<stack_temp_1>,<stack_temp_2>,<stack_temp_3>,<stack_temp_4>,<target_stack_temp>,<board_temp>,<h2_supply_pressure>,<tank_pressure>,<fan_speed>,<operation_state>\r\n
```

| Index | Field              | Type   | Unit   |
|-------|--------------------|--------|--------|
| 0     | Stack voltage      | float  | V      |
| 1     | Load current       | float  | A      |
| 2     | Power              | float  | W      |
| 3     | Energy             | float  | Wh     |
| 4     | Battery voltage    | float  | V      |
| 5     | Battery current    | float  | A      |
| 6     | Load voltage       | float  | V      |
| 7     | Stack temp 1       | float  | deg C  |
| 8     | Stack temp 2       | float  | deg C  |
| 9     | Stack temp 3       | float  | deg C  |
| 10    | Stack temp 4       | float  | deg C  |
| 11    | Target stack temp  | float  | deg C  |
| 12    | Board temp         | float  | deg C  |
| 13    | H2 supply pressure | float  | mV     |
| 14    | Tank pressure      | float  | -      |
| 15    | Fan speed          | float  | %      |
| 16    | Operation state    | uint   | -      |

Operation state values: `0` = standby, `1` = warmup, `2` = running, `3` = fault.

## uORB Topic: `pcu_telemetry`

The driver publishes to the `pcu_telemetry` uORB topic, defined in `msg/PcuTelemetry.msg`. The CSV field order matches the field order in the message definition. The message must be registered in `msg/CMakeLists.txt` to be built.

To inspect the topic at runtime from the PX4 shell:

```
listener pcu_telemetry
```

## MAVLink Stream: `PCU_TELEMETRY`

A custom MAVLink message `PCU_TELEMETRY` (message ID **50001**) is defined in the LEVITUM MAVLink dialect (`src/modules/mavlink/mavlink/message_definitions/v1.0/levitum.xml`). It carries all 16 float fields plus the `operation_state` uint8 and a `time_usec` timestamp.

The `mavlink` submodule at `src/modules/mavlink/mavlink` points to the LEVITUM fork at [LEVITUM-e-V/mavlink](https://github.com/LEVITUM-e-V/mavlink), where the custom message definition lives. Any changes to the MAVLink message definition must be made in that fork and the submodule updated afterwards:

```bash
cd PX4-Autopilot
git submodule update --remote src/modules/mavlink/mavlink
git add src/modules/mavlink/mavlink
git commit -m "Update mavlink submodule"
git push
```

The MAVLink stream class is implemented in `src/modules/mavlink/streams/PCU_TELEMETRY.hpp`. It subscribes to the `pcu_telemetry` uORB topic and sends a `PCU_TELEMETRY` MAVLink message whenever new data is available.

### How It Works

1. The `pcu_serial` driver parses CSV lines from the UART and publishes a `pcu_telemetry` uORB message.
2. The MAVLink module's `PCU_TELEMETRY` stream subscribes to this uORB topic.
3. Each update is forwarded as a `PCU_TELEMETRY` MAVLink message to the GCS.

### Default Stream Rates

The `PCU_TELEMETRY` stream is configured in `src/modules/mavlink/mavlink_main.cpp` at the following default rates:

| MAVLink Mode   | Default Rate |
|----------------|-------------|
| Normal         | 1 Hz        |
| Onboard        | 10 Hz       |
| OSD            | 1 Hz        |
| Config (USB)   | 50 Hz       |
| Minimal        | 1 Hz        |

### Adjusting the Stream Rate

To change the rate at runtime from the PX4 shell:

```
mavlink stream -d /dev/ttyS6 -s PCU_TELEMETRY -r 10
```

Replace `/dev/ttyS6` with the MAVLink port device (e.g., TELEM1 on Pixhawk 6X), or use `-u 14550` for a UDP connection. Set `-r 0` to disable the stream.

### Receiving on the Ground Station

The GCS must support the custom `PCU_TELEMETRY` message (ID 50001). For custom GCS integrations, use the Levitum MAVLink dialect XML to generate the appropriate language bindings. In QGroundControl, custom messages can be viewed in the MAVLink Inspector.

## Configuration

### Setting the Serial Port (only for physical Pixhawk6X)

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

## Manual Start

The driver can also be started manually from the PX4 shell, bypassing the parameter system:

```
pcu_serial start -d /dev/ttyS3
pcu_serial status
pcu_serial stop
```

## PCU Simulator (SITL Testing)

For testing without hardware, `Tools/simulation/pcu_sim.py` creates a pseudo-terminal and sends simulated PCU CSV telemetry.

### Usage

In a separate terminal:

```bash
python3 Tools/simulation/pcu_sim.py
```

Output:

```
PCU simulator started (CSV mode)
  PTY path : /dev/pts/5
  Profile  : steady
  Rate     : 10.0 Hz
  Format   : FC:<16 floats>,<uint>\r\n

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
