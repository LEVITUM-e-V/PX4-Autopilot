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
 * @file pcu.hpp
 * @brief UAVCAN sensor bridge for levitum::equipment::pcu::PcuStatus.
 *
 * Receives PcuStatus DroneCAN messages from the hydrogen fuel cell PCU
 * and publishes to the pcu_telemetry uORB topic.
 */

#pragma once

#include "sensor_bridge.hpp"

#include <uORB/topics/pcu_telemetry.h>

#include <levitum/equipment/pcu/PcuStatus.hpp>

class UavcanPcuBridge : public UavcanSensorBridgeBase
{
public:
	static const char *const NAME;

	UavcanPcuBridge(uavcan::INode &node);

	const char *get_name() const override { return NAME; }

	int init() override;

private:
	void pcu_sub_cb(const uavcan::ReceivedDataStructure<levitum::equipment::pcu::PcuStatus> &msg);

	typedef uavcan::MethodBinder < UavcanPcuBridge *,
		void (UavcanPcuBridge::*)
		(const uavcan::ReceivedDataStructure<levitum::equipment::pcu::PcuStatus> &) >
		PcuCbBinder;

	uavcan::Subscriber<levitum::equipment::pcu::PcuStatus, PcuCbBinder> _sub_pcu;
};
