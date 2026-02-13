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

#include "PcuSerial.hpp"

#include <fcntl.h>
#include <termios.h>
#include <cstring>
#include <cstdlib>

PcuSerial::PcuSerial(const char *port) :
	ScheduledWorkItem(MODULE_NAME, px4::serial_port_to_wq(port)),
	_sample_perf(perf_alloc(PC_ELAPSED, MODULE_NAME": read")),
	_comms_errors(perf_alloc(PC_COUNT, MODULE_NAME": com_err"))
{
	strncpy(_port, port, sizeof(_port) - 1);
	_port[sizeof(_port) - 1] = '\0';
}

PcuSerial::~PcuSerial()
{
	stop();

	if (_fd >= 0) {
		::close(_fd);
	}

	perf_free(_sample_perf);
	perf_free(_comms_errors);
}

int PcuSerial::init()
{
	start();
	return PX4_OK;
}

int PcuSerial::open_serial_port()
{
	_fd = ::open(_port, O_RDWR | O_NOCTTY | O_NONBLOCK);

	if (_fd < 0) {
		PX4_ERR("open failed (%i)", errno);
		return PX4_ERROR;
	}

	struct termios uart_config;

	tcgetattr(_fd, &uart_config);

	/* clear ONLCR flag (which appends a CR for every LF) */
	uart_config.c_oflag &= ~ONLCR;

	/* no parity, one stop bit */
	uart_config.c_cflag &= ~(CSTOPB | PARENB);

	/* set baud rate to 115200 */
	int termios_state;

	if ((termios_state = cfsetispeed(&uart_config, B115200)) < 0) {
		PX4_ERR("CFG: %d ISPD", termios_state);
	}

	if ((termios_state = cfsetospeed(&uart_config, B115200)) < 0) {
		PX4_ERR("CFG: %d OSPD", termios_state);
	}

	if ((termios_state = tcsetattr(_fd, TCSANOW, &uart_config)) < 0) {
		PX4_ERR("baud %d ATTR", termios_state);
	}

	PX4_INFO("opened %s @ 115200", _port);
	return PX4_OK;
}

/* ── Parse a complete CSV line and publish to uORB ────────────────────── */

void PcuSerial::parse_line()
{
	/* Line format: "FC:<f>,<f>,<f>,<f>,<f>,<f>,<f>,<f>,<f>,<f>,<f>,<f>,<f>,<f>,<f>,<f>,<u>\r\n"
	 * At this point _line_buf is null-terminated and \r\n has been stripped. */

	/* Verify "FC:" prefix */
	if (strncmp(_line_buf, "FC:", 3) != 0) {
		PX4_DEBUG("line missing FC: prefix");
		perf_count(_comms_errors);
		return;
	}

	perf_begin(_sample_perf);

	/* Point past the "FC:" prefix */
	char *ptr = _line_buf + 3;
	char *end;
	float fields[NUM_FIELDS - 1]; /* 16 floats */
	unsigned op_state = 0;

	/* Parse 16 float fields */
	for (unsigned i = 0; i < NUM_FIELDS - 1; i++) {
		fields[i] = strtof(ptr, &end);

		if (end == ptr) {
			PX4_DEBUG("CSV parse error at field %u", i);
			perf_count(_comms_errors);
			perf_end(_sample_perf);
			return;
		}

		ptr = end;

		if (i < NUM_FIELDS - 2) {
			/* Expect a comma separator between fields */
			if (*ptr != ',') {
				PX4_DEBUG("expected comma after field %u, got '%c'", i, *ptr);
				perf_count(_comms_errors);
				perf_end(_sample_perf);
				return;
			}

			ptr++; /* skip comma */
		}
	}

	/* Expect comma before the last (uint) field */
	if (*ptr != ',') {
		PX4_DEBUG("expected comma before operation_state, got '%c'", *ptr);
		perf_count(_comms_errors);
		perf_end(_sample_perf);
		return;
	}

	ptr++; /* skip comma */

	/* Parse the uint field */
	unsigned long val = strtoul(ptr, &end, 10);

	if (end == ptr) {
		PX4_DEBUG("CSV parse error at operation_state");
		perf_count(_comms_errors);
		perf_end(_sample_perf);
		return;
	}

	op_state = (unsigned)val;

	/* Publish to uORB */
	pcu_telemetry_s msg{};
	msg.timestamp          = hrt_absolute_time();
	msg.stack_voltage      = fields[0];
	msg.load_current       = fields[1];
	msg.power              = fields[2];
	msg.energy             = fields[3];
	msg.battery_voltage    = fields[4];
	msg.battery_current    = fields[5];
	msg.load_voltage       = fields[6];
	msg.stack_temp_1       = fields[7];
	msg.stack_temp_2       = fields[8];
	msg.stack_temp_3       = fields[9];
	msg.stack_temp_4       = fields[10];
	msg.target_stack_temp  = fields[11];
	msg.board_temp         = fields[12];
	msg.h2_supply_pressure = fields[13];
	msg.tank_pressure      = fields[14];
	msg.fan_speed          = fields[15];
	msg.operation_state    = (uint8_t)op_state;

	_pcu_telemetry_pub.publish(msg);

	perf_end(_sample_perf);
}

/* ── Collect bytes from serial port and assemble lines ────────────────── */

int PcuSerial::collect()
{
	uint8_t readbuf[256];
	int ret = ::read(_fd, readbuf, sizeof(readbuf));

	if (ret < 0) {
		if (errno != EAGAIN) {
			perf_count(_comms_errors);
		}

		return -EAGAIN;

	} else if (ret == 0) {
		return -EAGAIN;
	}

	for (int i = 0; i < ret; i++) {
		char c = (char)readbuf[i];

		if (c == '\n') {
			/* Strip trailing \r if present */
			if (_line_pos > 0 && _line_buf[_line_pos - 1] == '\r') {
				_line_pos--;
			}

			_line_buf[_line_pos] = '\0';

			if (_line_pos > 0) {
				parse_line();
			}

			_line_pos = 0;

		} else {
			if (_line_pos < LINE_BUF_SIZE - 1) {
				_line_buf[_line_pos++] = c;

			} else {
				/* Line too long, discard and reset */
				PX4_DEBUG("line buffer overflow, discarding");
				_line_pos = 0;
			}
		}
	}

	return PX4_OK;
}

void PcuSerial::start()
{
	ScheduleOnInterval(10000); /* 100 Hz */
}

void PcuSerial::stop()
{
	ScheduleClear();
}

void PcuSerial::Run()
{
	if (_fd < 0) {
		if (open_serial_port() != PX4_OK) {
			return;
		}
	}

	collect();
}

void PcuSerial::print_info()
{
	perf_print_counter(_sample_perf);
	perf_print_counter(_comms_errors);
}
