# Adding New Airframes for Gazebo SITL Simulation

This guide documents the steps required to add a new airframe to the PX4 SITL (Software-In-The-Loop) simulation using Gazebo.

## Overview

Adding a new airframe requires changes in two main areas:

1. **Airframe definition** - a shell script with parameters and metadata
2. **Build registration** - adding the airframe to CMakeLists.txt

Additionally, a Gazebo model (SDF + meshes) must exist for the simulator to render and simulate the vehicle.

## Step-by-Step Guide

### 1. Create the Gazebo Model

Create a directory under `Tools/simulation/gz/models/<model_name>/` with:

```
Tools/simulation/gz/models/<model_name>/
├── model.config        # XML metadata (name, version, author, SDF filename)
├── model.sdf           # Gazebo SDF model definition (links, joints, sensors, plugins)
└── meshes/             # 3D mesh files (STL, DAE, OBJ) referenced by the SDF
```

**model.config** example:
```xml
<?xml version="1.0"?>
<model>
  <name>My Vehicle</name>
  <version>1.0</version>
  <sdf version="1.9">model.sdf</sdf>
</model>
```

The `model.sdf` file defines the vehicle's physical properties (mass, inertia, collisions), visual meshes, joints, sensors, and Gazebo plugins for motor/servo simulation. Refer to existing models like `Tools/simulation/gz/models/x500/` or `Tools/simulation/gz/models/standard_vtol/` as templates.

### 2. Create the Airframe Definition File

Create a new file in `ROMFS/px4fmu_common/init.d-posix/airframes/`.

**Naming convention:** `<ID>_gz_<model_name>`
- `<ID>` is a unique numeric airframe ID (check existing files to avoid conflicts)
- `gz` indicates this is a Gazebo simulation airframe
- `<model_name>` identifies the vehicle

**ID ranges used in PX4:**
- `4001-4099` - Gazebo standard vehicles (x500, cessna, standard_vtol, etc.)
- `6001-6099` - Custom/hexarotor vehicles
- `[22000-22999]` - Reserved range for custom models

**File template:**
```sh
#!/bin/sh
#
# @name My Vehicle Name
#
# @type Standard VTOL
#

. ${R}etc/init.d/rc.vtol_defaults

PX4_SIMULATOR=${PX4_SIMULATOR:=gz}
PX4_GZ_WORLD=${PX4_GZ_WORLD:=default}
PX4_SIM_MODEL=${PX4_SIM_MODEL:=<model_name>}

param set-default SIM_GZ_EN 1
param set-default SENS_EN_GPSSIM 1
param set-default SENS_EN_BAROSIM 0
param set-default SENS_EN_MAGSIM 1
param set-default SENS_EN_ARSPDSIM 1

# Vehicle-specific parameters below...
param set-default CA_AIRFRAME 2
param set-default CA_ROTOR_COUNT 4
# ... rotor positions, control allocation, PID gains, etc.
```

Key fields:
- **`@name`** - human-readable name shown in QGroundControl
- **`@type`** - vehicle type (`Quadrotor`, `Standard VTOL`, `Fixed Wing`, etc.)
- **`. ${R}etc/init.d/rc.*_defaults`** - load defaults for the vehicle type (`rc.vtol_defaults`, `rc.mc_defaults`, `rc.fw_defaults`)
- **`PX4_SIM_MODEL`** - must match the directory name under `Tools/simulation/gz/models/`
- **`SIM_GZ_EC_FUNC*`** - maps ESC channels to actuator functions (101=Motor1, 102=Motor2, etc.)
- **`SIM_GZ_SV_FUNC*`** - maps servo channels to control surface functions (201=Servo1, etc.)

### 3. Register the Airframe in CMakeLists.txt

Add the filename to the `px4_add_romfs_files()` list in:

```
ROMFS/px4fmu_common/init.d-posix/airframes/CMakeLists.txt
```

For example:
```cmake
px4_add_romfs_files(
    # ... existing entries ...
    6006_gz_p4_1
)
```

Without this entry, the airframe file won't be included in the ROMFS build and won't be available at runtime.

### 4. Build and Run

The `gz_bridge` module automatically discovers all airframes matching the `*_gz_*` pattern, so no further registration is needed. After building, you can run:

```bash
make px4_sitl gz_<model_name>
```

For example:
```bash
make px4_sitl gz_levitum_p4_1
```

## Checklist

- [ ] Gazebo model directory exists at `Tools/simulation/gz/models/<model_name>/` with `model.config`, `model.sdf`, and meshes
- [ ] Airframe file created at `ROMFS/px4fmu_common/init.d-posix/airframes/<ID>_gz_<model_name>`
- [ ] `@name` and `@type` metadata comments are correct
- [ ] `PX4_SIM_MODEL` matches the Gazebo model directory name exactly
- [ ] `PX4_SIMULATOR` line is not broken (common copy-paste issue)
- [ ] Airframe filename is added to `ROMFS/px4fmu_common/init.d-posix/airframes/CMakeLists.txt`
- [ ] Airframe ID does not conflict with existing airframes
- [ ] Control allocation parameters (rotors, servos) match the SDF model

## File Locations Reference

| Component | Path |
|-----------|------|
| Airframe scripts | `ROMFS/px4fmu_common/init.d-posix/airframes/` |
| Airframe CMakeLists | `ROMFS/px4fmu_common/init.d-posix/airframes/CMakeLists.txt` |
| Gazebo models | `Tools/simulation/gz/models/` |
| gz_bridge (auto-discovery) | `src/modules/simulation/gz_bridge/CMakeLists.txt` |
| Gazebo worlds | `Tools/simulation/gz/worlds/` |
