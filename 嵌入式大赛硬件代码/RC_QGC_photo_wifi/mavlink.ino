/**
 * @file mavlink.ino
 * @brief MAVLink协议通信模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 实现MAVLink协议的发送与接收，支持与QGC地面站通信。
 * 主要功能：状态上报、参数管理、姿态目标控制、解锁/上锁、飞行模式切换。
 */

#if WIFI_ENABLED

#include <MAVLink.h>
#include "util.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/** 系统ID */
#define SYSTEM_ID 1
/** 低速消息发送频率(Hz) */
#define MAVLINK_RATE_SLOW 1
/** 高速消息发送频率(Hz) */
#define MAVLINK_RATE_FAST 2

/** MAVLink连接状态 */
bool mavlinkConnected = false;
/** 上次MAVLink消息接收时间 */
float lastMavlinkTime = 0;
/** MAVLink打印缓冲区 */
String mavlinkPrintBuffer;

extern float controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;
extern uint16_t channels[16];

extern SemaphoreHandle_t mutexState;
extern SemaphoreHandle_t mutexSerial;

int parametersCount();
const char *getParameterName(int index);
float getParameter(int index);
float getParameter(const char *name);
bool setParameter(const char *name, float value);

/**
 * @brief MAVLink消息处理主入口
 * 
 * 依次执行消息发送和接收。
 */
void processMavlink() {
	sendMavlink();
	receiveMavlink();
}

/**
 * @brief 发送MAVLink消息
 * 
 * 按不同频率发送心跳、系统状态、姿态、传感器和电机数据。
 */
void sendMavlink() {
	sendMavlinkPrint();

	mavlink_message_t msg;
	uint32_t time = t * 1000;

	static Rate slow(MAVLINK_RATE_SLOW), fast(MAVLINK_RATE_FAST);

	if (slow) {
		mavlink_msg_heartbeat_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC,
			(armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0) |
			((mode == STAB) ? MAV_MODE_FLAG_STABILIZE_ENABLED : 0) |
			((mode == AUTO) ? MAV_MODE_FLAG_AUTO_ENABLED : MAV_MODE_FLAG_MANUAL_INPUT_ENABLED),
			mode, MAV_STATE_STANDBY);
		sendMessage(&msg);

		if (!mavlinkConnected) return;

		mavlink_msg_extended_sys_state_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			MAV_VTOL_STATE_UNDEFINED, landed ? MAV_LANDED_STATE_ON_GROUND : MAV_LANDED_STATE_IN_AIR);
		sendMessage(&msg);
	}

	if (fast && mavlinkConnected) {
		Quaternion _attitude;
		Vector _rates, _gyro, _acc;
		float _motors[4];
		uint16_t _channels[16];
		uint32_t _controlTime;

		if (xSemaphoreTake(mutexState, pdMS_TO_TICKS(2)) == pdTRUE) {
			_attitude = attitude;
			_rates = rates;
			_gyro = gyro;
			_acc = acc;
			memcpy(_motors, motors, sizeof(motors));
			memcpy(_channels, channels, sizeof(channels));
			_controlTime = controlTime * 1000;
			xSemaphoreGive(mutexState);
		}

		const float zeroQuat[] = {0, 0, 0, 0};
		mavlink_msg_attitude_quaternion_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			time, _attitude.w, _attitude.x, -_attitude.y, -_attitude.z,
			_rates.x, -_rates.y, -_rates.z, zeroQuat);
		sendMessage(&msg);

		mavlink_msg_rc_channels_raw_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, _controlTime, 0,
			_channels[0], _channels[1], _channels[2], _channels[3],
			_channels[4], _channels[5], _channels[6], _channels[7], UINT8_MAX);
		if (_channels[0] != 0) sendMessage(&msg);

		float controls[8];
		memcpy(controls, _motors, sizeof(_motors));
		mavlink_msg_actuator_control_target_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, time, 0, controls);
		sendMessage(&msg);

		mavlink_msg_scaled_imu_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, time,
			_acc.x * 1000, -_acc.y * 1000, -_acc.z * 1000,
			_gyro.x * 1000, -_gyro.y * 1000, -_gyro.z * 1000,
			0, 0, 0, 0);
		sendMessage(&msg);
	}
}

/**
 * @brief 发送单个MAVLink消息
 * @param msg 消息指针
 */
void sendMessage(const void *msg) {
	uint8_t buf[MAVLINK_MAX_PACKET_LEN];
	int len = mavlink_msg_to_send_buffer(buf, (mavlink_message_t *)msg);
	sendWiFi(buf, len);
}

/**
 * @brief 接收MAVLink消息
 * 
 * 从UDP缓冲区读取数据并解析MAVLink消息。
 */
void receiveMavlink() {
	uint8_t buf[MAVLINK_MAX_PACKET_LEN];
	int len = receiveWiFi(buf, MAVLINK_MAX_PACKET_LEN);
	if (len) {
		mavlinkConnected = true;
		lastMavlinkTime = t;
	}

	mavlink_message_t msg;
	mavlink_status_t status;
	for (int i = 0; i < len; i++) {
		if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &msg, &status)) {
			handleMavlink(&msg);
		}
	}
}

/**
 * @brief 处理接收到的MAVLink消息
 * @param _msg MAVLink消息指针
 */
void handleMavlink(const void *_msg) {
	const mavlink_message_t& msg = *(mavlink_message_t *)_msg;

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
		mavlink_param_request_list_t m;
		mavlink_msg_param_request_list_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		mavlink_message_t msg;
		for (int i = 0; i < parametersCount(); i++) {
			mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
				getParameterName(i), getParameter(i), MAV_PARAM_TYPE_REAL32, parametersCount(), i);
			sendMessage(&msg);
		}
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_READ) {
		mavlink_param_request_read_t m;
		mavlink_msg_param_request_read_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		char name[MAVLINK_MSG_PARAM_REQUEST_READ_FIELD_PARAM_ID_LEN + 1];
		strlcpy(name, m.param_id, sizeof(name));
		float value = strlen(name) == 0 ? getParameter(m.param_index) : getParameter(name);
		if (m.param_index != -1) {
			memcpy(name, getParameterName(m.param_index), 16);
		}
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			name, value, MAV_PARAM_TYPE_REAL32, parametersCount(), m.param_index);
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_SET) {
		mavlink_param_set_t m;
		mavlink_msg_param_set_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		char name[MAVLINK_MSG_PARAM_SET_FIELD_PARAM_ID_LEN + 1];
		strlcpy(name, m.param_id, sizeof(name));
		setParameter(name, m.param_value);
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			m.param_id, m.param_value, MAV_PARAM_TYPE_REAL32, parametersCount(), 0);
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_LIST) {
		mavlink_mission_request_list_t m;
		mavlink_msg_mission_request_list_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		mavlink_message_t msg;
		mavlink_msg_mission_count_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, 0, 0, 0, MAV_MISSION_TYPE_MISSION, 0);
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_SERIAL_CONTROL) {
		mavlink_serial_control_t m;
		mavlink_msg_serial_control_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		char data[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN + 1];
		strlcpy(data, (const char *)m.data, m.count);
		doCommand(data, true);
	}

	if (msg.msgid == MAVLINK_MSG_ID_SET_ATTITUDE_TARGET) {
		if (mode != AUTO) return;

		mavlink_set_attitude_target_t m;
		mavlink_msg_set_attitude_target_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		xSemaphoreTake(mutexState, portMAX_DELAY);
		ratesTarget.x = m.body_roll_rate;
		ratesTarget.y = -m.body_pitch_rate;
		ratesTarget.z = -m.body_yaw_rate;
		attitudeTarget.w = m.q[0];
		attitudeTarget.x = m.q[1];
		attitudeTarget.y = -m.q[2];
		attitudeTarget.z = -m.q[3];
		thrustTarget = m.thrust;
		ratesExtra = Vector(0, 0, 0);

		if (m.type_mask & ATTITUDE_TARGET_TYPEMASK_ATTITUDE_IGNORE) attitudeTarget.invalidate();
		armed = m.thrust > 0;
		xSemaphoreGive(mutexState);
	}

	if (msg.msgid == MAVLINK_MSG_ID_SET_ACTUATOR_CONTROL_TARGET) {
		if (mode != AUTO) return;

		mavlink_set_actuator_control_target_t m;
		mavlink_msg_set_actuator_control_target_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		xSemaphoreTake(mutexState, portMAX_DELAY);
		attitudeTarget.invalidate();
		ratesTarget.invalidate();
		torqueTarget.invalidate();
		memcpy(motors, m.controls, sizeof(motors));
		armed = motors[0] > 0 || motors[1] > 0 || motors[2] > 0 || motors[3] > 0;
		xSemaphoreGive(mutexState);
	}

	if (msg.msgid == MAVLINK_MSG_ID_LOG_REQUEST_DATA) {
		mavlink_log_request_data_t m;
		mavlink_msg_log_request_data_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		for (int i = 0; i < sizeof(logBuffer) / sizeof(logBuffer[0]); i++) {
			mavlink_message_t msg;
			mavlink_msg_log_data_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, 0, i,
				sizeof(logBuffer[0]), (uint8_t *)logBuffer[i]);
			sendMessage(&msg);
		}
	}

	if (msg.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
		mavlink_command_long_t m;
		mavlink_msg_command_long_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;
		mavlink_message_t response;
		bool accepted = false;

		if (m.command == MAV_CMD_REQUEST_MESSAGE && m.param1 == MAVLINK_MSG_ID_AUTOPILOT_VERSION) {
			accepted = true;
			mavlink_msg_autopilot_version_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &response,
				MAV_PROTOCOL_CAPABILITY_PARAM_FLOAT | MAV_PROTOCOL_CAPABILITY_MAVLINK2, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0);
			sendMessage(&response);
		}

		if (m.command == MAV_CMD_COMPONENT_ARM_DISARM) {
			if (m.param1 && controlThrottle > 0.95) return;
			accepted = true;
			xSemaphoreTake(mutexState, portMAX_DELAY);
			armed = m.param1 == 1;
			xSemaphoreGive(mutexState);
		}

		if (m.command == MAV_CMD_DO_SET_MODE) {
			if (m.param2 < 0 || m.param2 > AUTO) return;
			accepted = true;
			xSemaphoreTake(mutexState, portMAX_DELAY);
			mode = m.param2;
			xSemaphoreGive(mutexState);
		}

		mavlink_message_t ack;
		mavlink_msg_command_ack_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &ack, m.command, accepted ? MAV_RESULT_ACCEPTED : MAV_RESULT_UNSUPPORTED, UINT8_MAX, 0, msg.sysid, msg.compid);
		sendMessage(&ack);
	}
}

/**
 * @brief 向MAVLink打印缓冲区添加消息
 * @param str 消息字符串
 */
void mavlinkPrint(const char* str) {
	mavlinkPrintBuffer += str;
}

/**
 * @brief 发送MAVLink打印缓冲区内容
 */
void sendMavlinkPrint() {
	xSemaphoreTake(mutexSerial, portMAX_DELAY);
	const char *str = mavlinkPrintBuffer.c_str();
	for (int i = 0; i < strlen(str); i += MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
		char data[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN + 1];
		strlcpy(data, str + i, sizeof(data));
		mavlink_message_t msg;
		mavlink_msg_serial_control_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			SERIAL_CONTROL_DEV_SHELL,
			i + MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN < strlen(str) ? SERIAL_CONTROL_FLAG_MULTI : 0,
			0, 0, strlen(data), (uint8_t *)data, 0, 0);
		uint8_t buf[MAVLINK_MAX_PACKET_LEN];
		int len = mavlink_msg_to_send_buffer(buf, &msg);
		sendWiFi(buf, len);
	}
	mavlinkPrintBuffer.clear();
	xSemaphoreGive(mutexSerial);
}

#endif