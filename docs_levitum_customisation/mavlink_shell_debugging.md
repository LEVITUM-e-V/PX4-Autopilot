# MAVLink Shell Debugging via QGroundControl

The MAVLink shell gives you an interactive NuttShell (NSH) terminal on the flight controller over the MAVLink connection — no physical debug cable required. It is the primary tool for inspecting uORB topics and driver state on a connected vehicle.

## Opening the MAVLink Shell

1. Connect the vehicle to QGroundControl (USB or telemetry radio).
2. Open **Analyze Tools** → **MAVLink Console** (or press the terminal icon in the toolbar).
3. The shell prompt `nsh>` indicates the connection is live.

The shell runs at the full console baud rate. Commands behave identically to a wired NSH session.

---

## uORB Topic Inspection

### `listener` — Print topic data

Prints the current value of a uORB topic each time it is published.

```
listener <topic_name> [--rate <hz>] [--timeout <ms>] [-n <count>]
```

| Option | Description |
|---|---|
| `--rate <hz>` | Throttle output to this rate (default: every publication) |
| `--timeout <ms>` | Exit after this many milliseconds if no message arrives |
| `-n <count>` | Exit after printing this many messages |

**Examples**

```
listener pcu_telemetry
listener sensor_combined --rate 5
listener battery_status -n 10
```

Press `Ctrl+C` to stop.

### `uorb` — Topic metadata and statistics

```
uorb status
uorb top
```

| Command | Description |
|---|---|
| `uorb status` | List all advertised topics, subscriber/publisher counts, and message sizes |
| `uorb top` | Live view of publication rates and subscriber counts (like `top` for topics) |

`uorb top` refreshes every second. Press `q` to quit.

To watch a subset of topics:

```
uorb top pcu_telemetry battery_status
```

---

## Driver Commands

Every PX4 driver module exposes a standard subcommand interface.

### `pcu_serial` — serial path debug

```
pcu_serial status
pcu_serial start -d /dev/ttyS3
pcu_serial stop
```

`pcu_serial status` prints the configured port, baud rate, number of parsed frames, and parse error counters. A non-zero error count with a low frame count usually means a wiring or baud rate mismatch.

### `mavlink` — stream and connection status

```
mavlink status
mavlink stream -d /dev/ttyS6 -s PCU_TELEMETRY -r 10
mavlink stream -u 14550 -s PCU_TELEMETRY -r 0
```

| Option | Description |
|---|---|
| `-d <device>` | Target the MAVLink instance on this serial device |
| `-u <port>` | Target the MAVLink instance on this UDP port |
| `-s <stream>` | Stream name to adjust |
| `-r <hz>` | New rate in Hz; `0` disables the stream |

`mavlink status` lists all active instances, their devices/ports, and current stream rates. Confirm the change in QGroundControl's **MAVLink Inspector** (Analyze Tools → MAVLink Inspector, filter by message ID 50001 for `PCU_TELEMETRY`).

---

## DroneCAN / UAVCAN Debug

### `uavcan status` — node table and bus health

```
uavcan status
```

Prints all known nodes on the CAN bus with their node ID, node name, health, mode, and time since last `NodeStatus` heartbeat. The PCU should appear as:

```
Node 125  levitum.pcu   HEALTH_OK   MODE_OPERATIONAL   uptime: ...
```

If the PCU node is **missing** from the list, it has never sent a `NodeStatus` heartbeat. Check CAN wiring and bus termination (120 ohm at each physical bus end).

If the node shows **OFFLINE**, the heartbeat was seen at boot but has stopped arriving. Check power to the PCU and CAN transceiver.

### `uavcan start` — manually start the UAVCAN driver

```
uavcan start
```

Normally started automatically when `UAVCAN_ENABLE` is set. If the driver is not running, start it manually to see initialisation errors. A failure here usually means the CAN transceiver is not powered or wired correctly (verify SN65HVD230: PF8→D, PF7→R, 3V3→VCC, GND→GND, RS→GND).

### Verifying the PCU bridge is receiving data

```
listener pcu_telemetry -n 10
listener battery_status -n 5
```

`pcu_telemetry` is populated by `UavcanPcuBridge` at the rate the PCU broadcasts `PcuStatus` (DTID 20200, up to 10 Hz). `battery_status` is populated from the standard `BatteryInfo` (DTID 1092, 1 Hz) that the PCU also broadcasts.

If `pcu_telemetry` is missing but `battery_status` is present, the bridge class is not registered or `UAVCAN_SUB_PCU` is not set. If neither appears, the CAN bus has no traffic — run `uavcan status` to check connectivity.

### Checking for DSDL signature mismatches

A DSDL signature mismatch causes silent message rejection: the node table shows the PCU as healthy, but `pcu_telemetry` receives no data. To confirm:

1. Check the DSDL file `src/drivers/uavcan/dsdl/levitum/equipment/pcu/20200.PcuStatus.uavcan` is byte-identical to the PCU-side copy at `PCU/DSDL/levitum/equipment/pcu/20200.PcuStatus.uavcan`.
2. Re-flash PX4 after any DSDL change — signatures are computed at compile time.

### Required parameters for DroneCAN path

```
param show UAVCAN_ENABLE
param show UAVCAN_SUB_PCU
param show UAVCAN_SUB_BAT
```

All three must be `1` and a reboot must follow any change:

```
param set UAVCAN_ENABLE 1
param set UAVCAN_SUB_PCU 1
param set UAVCAN_SUB_BAT 1
param save
reboot
```

---

## Parameter Management

```
param show <name>
param set <name> <value>
param save
```

**Examples**

```
param show PCU_UART_PORT
param set PCU_UART_PORT 401
param save
```

Parameters that configure hardware (serial port, baud rate, UAVCAN enable) require a reboot:

```
reboot
```

To show all parameters matching a prefix:

```
param show PCU_*
param show UAVCAN_*
param show SER_*
```

---

## Process and Module Management

### `ps` — List running tasks

```
ps
```

Shows all NuttX tasks with their PID, stack usage, CPU time, and name. Confirm a driver is running before blaming it for missing data.

### `top` — CPU and memory usage

```
top
```

Live view of per-task CPU usage and heap/stack utilisation. Press `q` to quit.

### Starting and stopping modules manually

```
<module> start
<module> stop
<module> status
```

If a driver crashed or was never started, `<driver> start` re-initialises it without a full reboot.

---

## System Information

```
ver all          # Firmware version, git hash, build date, board
free             # Heap and stack memory usage
df               # SD card usage
dmesg            # Kernel message ring buffer (boot messages, hardware errors)
```

`dmesg` is particularly useful for spotting UART initialisation failures, CAN transceiver errors, or sensor probe failures that happen at boot before the MAVLink connection is up.

---

## Typical Debug Workflows

### Workflow A — No data in `listener pcu_telemetry` (DroneCAN path)

```
# 1. Check parameters
param show UAVCAN_ENABLE
param show UAVCAN_SUB_PCU

# 2. Check the bus — is the PCU node visible?
uavcan status

# 3. Check topic publications
uorb status | grep pcu_telemetry
listener pcu_telemetry -n 5

# 4. Check battery_status as a sanity check (different DTID, same CAN bus)
listener battery_status -n 5
```

If `uavcan status` shows the PCU node as healthy but `pcu_telemetry` is silent, suspect a DSDL signature mismatch (see above).

### Workflow B — No data in `listener pcu_telemetry` (serial path)

```
# 1. Check the driver is running
ps | grep pcu_serial
pcu_serial status

# 2. Check for parse errors in the driver status output
# Non-zero error count with low frame count = baud rate or wiring issue

# 3. Check the uORB topic
uorb status | grep pcu_telemetry
listener pcu_telemetry -n 5
```

If the driver is not in `ps`, check `dmesg` for the initialisation error and verify `PCU_UART_PORT` points to the correct port.

### Workflow C — Driver running, topic published, but no data in QGC

```
# 1. Check the MAVLink stream is active
mavlink status

# 2. If rate is 0, enable the stream
mavlink stream -d /dev/ttyS6 -s PCU_TELEMETRY -r 1

# 3. Verify in QGC MAVLink Inspector (message ID 50001)
```

### Workflow D — Intermittent or zero data on DroneCAN

```
uavcan status        # watch for nodes going OFFLINE
uorb top pcu_telemetry   # watch publish rate; should be ~10 Hz when connected
```

Intermittent data at low rates usually means missing 120 ohm bus termination. Add a 120 ohm resistor at each physical end of the CAN bus.

---

## Notes

- The MAVLink shell shares bandwidth with telemetry. On slow radio links, verbose `listener` output can disrupt other MAVLink traffic. Use `-n` or `--rate` to limit output.
- Commands run synchronously in the shell; a blocking command (e.g., `listener` without `-n`) must be interrupted with `Ctrl+C` before entering another command.
- `param save` writes parameters to the SD card. Without it, parameter changes are lost on reboot.
- Do not run both `pcu_serial` and `UavcanPcuBridge` simultaneously — both publish to the same `pcu_telemetry` topic and will produce interleaved data.
