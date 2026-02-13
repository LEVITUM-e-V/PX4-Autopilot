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

/**
 * @file PcuSerial.hpp
 *
 * Driver for reading PCU (Power Control Unit) telemetry over serial (UART).
 * The PCU sends CSV lines in the format:
 *   FC:<stack_voltage>,<load_current>,<power>,<energy>,<battery_voltage>,
 *      <battery_current>,<load_voltage>,<stack_temp_1>,<stack_temp_2>,
 *      <stack_temp_3>,<stack_temp_4>,<target_stack_temp>,<board_temp>,
 *      <h2_supply_pressure>,<tank_pressure>,<fan_speed>,<operation_state>\r\n
 *
 * Publishes to the pcu_telemetry uORB topic.
 */

#pragma once

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>
#include <drivers/drv_hrt.h>
#include <lib/perf/perf_counter.h>
#include <uORB/Publication.hpp>
#include <uORB/topics/pcu_telemetry.h>

class PcuSerial : public px4::ScheduledWorkItem
{
public:
	PcuSerial(const char *port);
	~PcuSerial() override;

	int			init();
	void			print_info();

private:
	void			start();
	void			stop();
	void			Run() override;

	int			open_serial_port();
	int			collect();

	/** Maximum expected CSV line length (including "FC:" prefix and \r\n) */
	static constexpr unsigned LINE_BUF_SIZE = 512;

	/** Number of CSV fields expected (16 floats + 1 uint) */
	static constexpr unsigned NUM_FIELDS = 17;

	char			_line_buf[LINE_BUF_SIZE] {};
	unsigned		_line_pos{0};

	void			parse_line();

	char			_port[20] {};
	int			_fd{-1};

	uORB::Publication<pcu_telemetry_s> _pcu_telemetry_pub{ORB_ID(pcu_telemetry)};

	perf_counter_t		_sample_perf;
	perf_counter_t		_comms_errors;
};
