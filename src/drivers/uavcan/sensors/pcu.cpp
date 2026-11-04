/****************************************************************************
 *
 *   Copyright (c) 2025 Levitum. All rights reserved.
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
 * 3. Neither the name Levitum nor the names of its contributors may be
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
 * @file pcu.cpp
 * @brief UAVCAN sensor bridge for the hydrogen fuel cell PCU.
 */

#include "pcu.hpp"

#include <drivers/drv_hrt.h>

const char *const UavcanPcuBridge::NAME = "pcu";

UavcanPcuBridge::UavcanPcuBridge(uavcan::INode &node) :
	UavcanSensorBridgeBase("uavcan_pcu", ORB_ID(pcu_telemetry)),
	_sub_pcu(node)
{
}

int UavcanPcuBridge::init()
{
	int res = _sub_pcu.start(PcuCbBinder(this, &UavcanPcuBridge::pcu_sub_cb));

	if (res < 0) {
		DEVICE_LOG("failed to start uavcan sub: %d", res);
		return res;
	}

	return 0;
}

void UavcanPcuBridge::pcu_sub_cb(
	const uavcan::ReceivedDataStructure<levitum::equipment::pcu::PcuStatus> &msg)
{
	pcu_telemetry_s report{};
	report.timestamp          = hrt_absolute_time();

	report.stack_voltage      = msg.stack_voltage;
	report.load_current       = msg.load_current;
	report.power              = msg.power;
	report.energy             = msg.energy;
	report.battery_voltage    = msg.battery_voltage;
	report.battery_current    = msg.battery_current;
	report.load_voltage       = msg.load_voltage;
	report.stack_temp_1       = msg.stack_temp[0];
	report.stack_temp_2       = msg.stack_temp[1];
	report.stack_temp_3       = msg.stack_temp[2];
	report.stack_temp_4       = msg.stack_temp[3];
	report.target_stack_temp  = msg.target_stack_temp;
	report.board_temp         = msg.board_temp;
	report.h2_supply_pressure = msg.h2_supply_pressure;
	report.tank_pressure      = msg.tank_pressure;
	report.fan_speed          = msg.fan_speed;
	report.operation_state    = msg.operation_state;

	publish(msg.getSrcNodeID().get(), &report);
}
