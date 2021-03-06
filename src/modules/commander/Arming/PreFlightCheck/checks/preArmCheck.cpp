/****************************************************************************
 *
 *   Copyright (c) 2019 PX4 Development Team. All rights reserved.
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

#include "../PreFlightCheck.hpp"

#include <ArmAuthorization.h>
#include <systemlib/mavlink_log.h>
#include <uORB/topics/vehicle_command_ack.h>

bool PreFlightCheck::preArmCheck(orb_advert_t *mavlink_log_pub, const vehicle_status_flags_s &status_flags,
				 const safety_s &safety, const uint8_t arm_requirements)
{
	bool prearm_ok = true;

	// USB not connected
	if (!status_flags.circuit_breaker_engaged_usb_check && status_flags.usb_connected) {
		mavlink_log_critical(mavlink_log_pub, "Arming denied! Flying with USB is not safe");

		prearm_ok = false;
	}

	// battery and system power status
	if (!status_flags.circuit_breaker_engaged_power_check) {

		// Fail transition if power is not good
		if (!status_flags.condition_power_input_valid) {
			mavlink_log_critical(mavlink_log_pub, "Arming denied! Connect power module");

			prearm_ok = false;
		}

		// main battery level
		if (!status_flags.condition_battery_healthy) {
			if (prearm_ok) {
				mavlink_log_critical(mavlink_log_pub, "Arming denied! Check battery");
			}

			prearm_ok = false;
		}
	}

	// Arm Requirements: mission
	if (arm_requirements & ARM_REQ_MISSION_BIT) {

		if (!status_flags.condition_auto_mission_available) {
			if (prearm_ok) {
				mavlink_log_critical(mavlink_log_pub, "Arming denied! No valid mission");
			}

			prearm_ok = false;
		}

		if (!status_flags.condition_global_position_valid) {
			if (prearm_ok) {
				mavlink_log_critical(mavlink_log_pub, "Arming denied! Missions require a global position");
			}

			prearm_ok = false;
		}
	}

	// Arm Requirements: global position
	if (arm_requirements & ARM_REQ_GPS_BIT) {

		if (!status_flags.condition_global_position_valid) {
			if (prearm_ok) {
				mavlink_log_critical(mavlink_log_pub, "Arming denied! Global position required");
			}

			prearm_ok = false;
		}
	}

	// safety button
	if (safety.safety_switch_available && !safety.safety_off) {
		// Fail transition if we need safety switch press
		if (prearm_ok) {
			mavlink_log_critical(mavlink_log_pub, "Arming denied! Press safety switch first");
		}

		prearm_ok = false;
	}

	if (status_flags.avoidance_system_required && !status_flags.avoidance_system_valid) {
		if (prearm_ok) {
			mavlink_log_critical(mavlink_log_pub, "Arming denied! Avoidance system not ready");
		}

		prearm_ok = false;

	}

	if (status_flags.condition_escs_error && (arm_requirements & ARM_REQ_ESCS_CHECK_BIT)) {
		if (prearm_ok) {
			mavlink_log_critical(mavlink_log_pub, "Arming denied! One or more ESCs are offline");
			prearm_ok = false;
		}
	}

	// Arm Requirements: authorization
	// check last, and only if everything else has passed
	if ((arm_requirements & ARM_REQ_ARM_AUTH_BIT) && prearm_ok) {
		if (arm_auth_check() != vehicle_command_ack_s::VEHICLE_RESULT_ACCEPTED) {
			// feedback provided in arm_auth_check
			prearm_ok = false;
		}
	}


	return prearm_ok;
}
