# PCU DroneCAN Bridge

The `UavcanPcuBridge` is a UAVCAN v0 (DroneCAN) sensor bridge that subscribes to `levitum::equipment::pcu::PcuStatus` messages (DTID 20200) from the hydrogen fuel cell PCU and publishes to the `pcu_telemetry` uORB topic.

This replaces the serial path (`pcu_serial` driver) with CAN bus communication. Both drivers publish to the same `pcu_telemetry` uORB topic, so downstream consumers (MAVLink stream, logging, etc.) work unchanged.

## How It Works

```
[PCU STM32U575]                      [PX4 Flight Controller]
     |                                      |
     | FDCAN @ 1 Mbps                       | FDCAN
     |     PcuStatus (DTID 20200)           |
     |     BatteryInfo (DTID 1092)          |
     |     NodeStatus (DTID 341)            |
     |                                      |
     +--- CAN bus (2-wire) ----------------+
                                            |
                                     uavcan driver
                                            |
                                   UavcanPcuBridge
                                            |
                                   pcu_telemetry uORB
                                            |
                              +---------+--------+
                              |                  |
                        PCU_TELEMETRY      flight logging
                        MAVLink stream
```

1. The PCU broadcasts `PcuStatus` over DroneCAN whenever new fuel cell data arrives (~10 Hz max).
2. PX4's UAVCAN driver receives the message via `UavcanPcuBridge::pcu_sub_cb()`.
3. The callback maps all 17 DSDL fields to the `pcu_telemetry_s` struct and publishes via uORB.
4. Additionally, the PCU broadcasts standard `uavcan.equipment.power.BatteryInfo` (DTID 1092) at 1 Hz, which PX4's built-in battery bridge picks up automatically.

## DSDL Message

**Path:** `src/drivers/uavcan/dsdl/levitum/equipment/pcu/20200.PcuStatus.uavcan`

This file must be byte-identical to the copy on the PCU side (`PCU/DSDL/levitum/equipment/pcu/20200.PcuStatus.uavcan`). Any difference causes a DSDL signature mismatch and silent message rejection.

### Fields

| Field | DSDL Type | uORB Field | Unit |
|-------|-----------|------------|------|
| stack_voltage | float16 | stack_voltage | V |
| load_current | float16 | load_current | A |
| power | float16 | power | W |
| energy | float32 | energy | Wh |
| battery_voltage | float16 | battery_voltage | V |
| battery_current | float16 | battery_current | A |
| load_voltage | float16 | load_voltage | V |
| stack_temp[0..3] | float16[4] | stack_temp_1..4 | deg C |
| target_stack_temp | float16 | target_stack_temp | deg C |
| board_temp | float16 | board_temp | deg C |
| h2_supply_pressure | float16 | h2_supply_pressure | bar |
| tank_pressure | float16 | tank_pressure | bar |
| fan_speed | float16 | fan_speed | RPM |
| operation_state | uint8 | operation_state | enum |

Operation state values: `0` = standby, `1` = warmup, `2` = running, `3` = fault.

## Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `UAVCAN_ENABLE` | 0 | Set to `1` to enable the DroneCAN stack |
| `UAVCAN_SUB_PCU` | 0 | Set to `1` to enable the PCU bridge |
| `UAVCAN_SUB_BAT` | 0 | Set to `1` to also receive BatteryInfo from the PCU |

All require a reboot after changing.

### Quick Setup

```
param set UAVCAN_ENABLE 1
param set UAVCAN_SUB_PCU 1
param set UAVCAN_SUB_BAT 1
reboot
```

### Verification

```
listener pcu_telemetry
listener battery_status
```

## PCU Node Details

| Property | Value |
|----------|-------|
| Node ID | 125 |
| Node name | `levitum.pcu` |
| Bitrate | 1 Mbps |
| Broadcast: PcuStatus | DTID 20200, on new data |
| Broadcast: BatteryInfo | DTID 1092, 1 Hz |
| Broadcast: NodeStatus | DTID 341, 1 Hz |

The PCU node ID (125) is hardcoded in the PCU firmware (`PCU/Core/Inc/pcu_dronecan_config.h`). PX4 defaults to node ID 1. These must not conflict.

## Files

### Source Files

| File | Purpose |
|------|---------|
| `src/drivers/uavcan/sensors/pcu.hpp` | Bridge class definition |
| `src/drivers/uavcan/sensors/pcu.cpp` | Subscriber callback, field mapping, uORB publish |
| `src/drivers/uavcan/dsdl/levitum/equipment/pcu/20200.PcuStatus.uavcan` | DSDL message definition |

### Modified Files

| File | Change |
|------|--------|
| `src/drivers/uavcan/CMakeLists.txt` | Added `levitum` to DSDLC_INPUTS, `sensors/pcu.cpp` to SRCS |
| `src/drivers/uavcan/Kconfig` | Added `UAVCAN_SENSOR_PCU` config entry |
| `src/drivers/uavcan/sensors/sensor_bridge.cpp` | Added PCU include and `make_all()` registration |
| `src/drivers/uavcan/uavcan_params.c` | Added `UAVCAN_SUB_PCU` parameter |

### Shared Files (unchanged)

| File | Role |
|------|------|
| `msg/PcuTelemetry.msg` | uORB message definition (shared with `pcu_serial` driver) |
| `src/modules/mavlink/streams/PCU_TELEMETRY.hpp` | MAVLink stream (reads from `pcu_telemetry` uORB) |

## DroneCAN vs Serial: When to Use Which

| | DroneCAN (`UavcanPcuBridge`) | Serial (`pcu_serial`) |
|---|---|---|
| Interface | CAN bus (2-wire differential) | UART (TX/RX) |
| Requires | CAN transceiver (SN65HVD230) | Direct UART connection |
| Bus sharing | Yes (multiple nodes on one bus) | No (point-to-point) |
| Extra battery data | Yes (BatteryInfo for free) | No |
| Wiring | 4 wires (CANH, CANL, VCC, GND) | 3 wires (TX, RX, GND) |
| Max distance | ~40 m with proper termination | ~2 m typical |

Both publish to the same `pcu_telemetry` uORB topic. Do not run both simultaneously.

## Extending the Message

When new sensors are added to the PCU:

1. **PCU side:** Append fields to `20200.PcuStatus.uavcan`, update `pcu_telemetry_t`, populate in `fcToStruct()` or new sensor code
2. **PX4 side:** Sync the DSDL file (must be byte-identical), add field assignment in `pcu_sub_cb()`, add field to `msg/PcuTelemetry.msg`

Fields must only be appended — never reorder or remove existing fields.

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| No data in `listener pcu_telemetry` | `UAVCAN_ENABLE` or `UAVCAN_SUB_PCU` not set | Set params and reboot |
| Node shows as OFFLINE in `uavcan status` | Missing NodeStatus heartbeat or bus error | Check CAN wiring and termination |
| Data appears garbled or zero | DSDL signature mismatch | Ensure DSDL files are byte-identical on both sides |
| Intermittent data loss | Missing 120 ohm termination | Add 120 ohm resistor at each physical bus end |
| `uavcan start` fails | CAN transceiver not powered or wired | Verify SN65HVD230 wiring: PF8->D, PF7->R, 3V3->VCC, GND->GND, RS->GND |
