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

	return PX4_OK;
}

int PcuSerial::parse_floats(const char *line, debug_array_s &msg)
{
	unsigned count = 0;
	const char *ptr = line;

	while (*ptr != '\0' && count < debug_array_s::ARRAY_SIZE) {
		char *end = nullptr;
		float val = strtof(ptr, &end);

		if (end == ptr) {
			/* no conversion performed, skip this character */
			ptr++;
			continue;
		}

		msg.data[count] = val;
		count++;

		ptr = end;

		if (*ptr == ',') {
			ptr++;
		}
	}

	return (int)count;
}

int PcuSerial::collect()
{
	char readbuf[128];
	int ret = ::read(_fd, readbuf, sizeof(readbuf));

	if (ret < 0) {
		perf_count(_comms_errors);
		return -EAGAIN;

	} else if (ret == 0) {
		return -EAGAIN;
	}

	for (int i = 0; i < ret; i++) {
		char c = readbuf[i];

		if (c == '\n' || c == '\r') {
			if (_linebuf_index > 0) {
				_linebuf[_linebuf_index] = '\0';

				perf_begin(_sample_perf);

				debug_array_s msg{};
				msg.timestamp = hrt_absolute_time();
				msg.id = 0;
				strncpy(msg.name, "pcu", sizeof(msg.name));

				int num_values = parse_floats(_linebuf, msg);

				if (num_values > 0) {
					_debug_array_pub.publish(msg);
				}

				perf_end(_sample_perf);

				_linebuf_index = 0;
			}

		} else {
			if (_linebuf_index < LINE_BUFFER_SIZE - 1) {
				_linebuf[_linebuf_index++] = c;

			} else {
				/* overflow, discard line */
				_linebuf_index = 0;
				perf_count(_comms_errors);
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
