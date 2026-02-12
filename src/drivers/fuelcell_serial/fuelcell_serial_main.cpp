/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include "FuelCellSerial.hpp"

#include <px4_platform_common/getopt.h>
#include <px4_platform_common/module.h>

namespace fuelcell_serial
{

FuelCellSerial *g_dev{nullptr};

static int start(const char *port)
{
	if (g_dev != nullptr) {
		PX4_WARN("already started");
		return -1;
	}

	if (port == nullptr) {
		PX4_ERR("no device specified");
		return -1;
	}

	g_dev = new FuelCellSerial(port);

	if (g_dev == nullptr) {
		return -1;
	}

	if (g_dev->init() != PX4_OK) {
		delete g_dev;
		g_dev = nullptr;
		return -1;
	}

	return 0;
}

static int stop()
{
	if (g_dev != nullptr) {
		delete g_dev;
		g_dev = nullptr;

	} else {
		return -1;
	}

	return 0;
}

static int status()
{
	if (g_dev == nullptr) {
		PX4_ERR("driver not running");
		return -1;
	}

	g_dev->print_info();

	return 0;
}

static int usage()
{
	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description

Serial driver for reading fuel cell telemetry data over UART.

Expects ASCII comma-separated float values, newline-terminated.
Publishes data to the debug_array uORB topic, which is bridged to
the ground station via MAVLink DEBUG_FLOAT_ARRAY messages.

The serial port is configured via the FCEL_SER_CFG parameter.

### Examples

Start driver on a specified serial device:
$ fuelcell_serial start -d /dev/ttyS3

Stop driver:
$ fuelcell_serial stop
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("fuelcell_serial", "driver");
	PRINT_MODULE_USAGE_SUBCATEGORY("fuelcell");
	PRINT_MODULE_USAGE_COMMAND_DESCR("start", "Start driver");
	PRINT_MODULE_USAGE_PARAM_STRING('d', nullptr, nullptr, "Serial device", false);
	PRINT_MODULE_USAGE_COMMAND_DESCR("stop", "Stop driver");
	PRINT_MODULE_USAGE_COMMAND_DESCR("status", "Print driver status");
	return PX4_OK;
}

} // namespace fuelcell_serial

extern "C" __EXPORT int fuelcell_serial_main(int argc, char *argv[])
{
	const char *device_path = nullptr;
	int ch;
	int myoptind = 1;
	const char *myoptarg = nullptr;

	while ((ch = px4_getopt(argc, argv, "d:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'd':
			device_path = myoptarg;
			break;

		default:
			fuelcell_serial::usage();
			return -1;
		}
	}

	if (myoptind >= argc) {
		fuelcell_serial::usage();
		return -1;
	}

	if (!strcmp(argv[myoptind], "start")) {
		return fuelcell_serial::start(device_path);

	} else if (!strcmp(argv[myoptind], "stop")) {
		return fuelcell_serial::stop();

	} else if (!strcmp(argv[myoptind], "status")) {
		return fuelcell_serial::status();
	}

	fuelcell_serial::usage();
	return -1;
}
