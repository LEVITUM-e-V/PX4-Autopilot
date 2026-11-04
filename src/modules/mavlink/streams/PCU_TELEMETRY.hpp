#ifndef PCU_TELEMETRY_HPP
#define PCU_TELEMETRY_HPP

#include <uORB/topics/pcu_telemetry.h>

class MavlinkStreamPcuTelemetry : public MavlinkStream
{
public:
	static MavlinkStream *new_instance(Mavlink *mavlink) { return new MavlinkStreamPcuTelemetry(mavlink); }

	static constexpr const char *get_name_static() { return "PCU_TELEMETRY"; }
	static constexpr uint16_t get_id_static() { return MAVLINK_MSG_ID_PCU_TELEMETRY; }

	const char *get_name() const override { return get_name_static(); }
	uint16_t get_id() override { return get_id_static(); }

	unsigned get_size() override
	{
		return _pcu_telemetry_sub.advertised() ? MAVLINK_MSG_ID_PCU_TELEMETRY_LEN + MAVLINK_NUM_NON_PAYLOAD_BYTES : 0;
	}

private:
	explicit MavlinkStreamPcuTelemetry(Mavlink *mavlink) : MavlinkStream(mavlink) {}

	uORB::Subscription _pcu_telemetry_sub{ORB_ID(pcu_telemetry)};

	bool send() override
	{
		pcu_telemetry_s pcu;

		if (_pcu_telemetry_sub.update(&pcu)) {
			mavlink_pcu_telemetry_t msg{};

			msg.time_usec         = pcu.timestamp;
			msg.stack_voltage     = pcu.stack_voltage;
			msg.load_current      = pcu.load_current;
			msg.power             = pcu.power;
			msg.energy            = pcu.energy;
			msg.battery_voltage   = pcu.battery_voltage;
			msg.battery_current   = pcu.battery_current;
			msg.load_voltage      = pcu.load_voltage;
			msg.stack_temp_1      = pcu.stack_temp_1;
			msg.stack_temp_2      = pcu.stack_temp_2;
			msg.stack_temp_3      = pcu.stack_temp_3;
			msg.stack_temp_4      = pcu.stack_temp_4;
			msg.target_stack_temp = pcu.target_stack_temp;
			msg.board_temp        = pcu.board_temp;
			msg.h2_supply_pressure = pcu.h2_supply_pressure;
			msg.tank_pressure     = pcu.tank_pressure;
			msg.fan_speed         = pcu.fan_speed;
			msg.operation_state   = pcu.operation_state;

			mavlink_msg_pcu_telemetry_send_struct(_mavlink->get_channel(), &msg);

			return true;
		}

		return false;
	}
};

#endif // PCU_TELEMETRY_HPP
