/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Session Info
 *
 * Copyright 2016 David FORT <contact@hardening-consulting.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef FREERDP_SESSION_H
#define FREERDP_SESSION_H

#include <winpr/wtypes.h>
#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/* Logon Information Types */
	typedef enum WINPR_C23_ENUM_TYPE(uint32_t)
	{
		INFO_TYPE_LOGON = 0x00000000,
		INFO_TYPE_LOGON_LONG = 0x00000001,
		INFO_TYPE_LOGON_PLAIN_NOTIFY = 0x00000002,
		INFO_TYPE_LOGON_EXTENDED_INF = 0x00000003
	} RDP_LOGON_INFO_TYPE;

	struct rdp_logon_info
	{
		UINT32 sessionId;
		char* username;
		char* domain;
	};
	typedef struct rdp_logon_info logon_info;

	struct rdp_logon_info_ex
	{
		BOOL haveCookie;
		UINT32 LogonId;
		BYTE ArcRandomBits[16];

		BOOL haveErrorInfo;
		UINT32 ErrorNotificationType;
		UINT32 ErrorNotificationData;
	};
	typedef struct rdp_logon_info_ex logon_info_ex;

	/** @brief return a string representation of \ref RDP_LOGON_INFO_TYPE or INFO_TYPE_UNKNOWN
	 *
	 *  @param type The type to stringify
	 *  @return A string representing \ref type
	 *  @since version 3.30.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API const char* freerdp_session_logon_type_str(uint32_t type);

	/** @brief return a string representation of \ref RDP_LOGON_INFO_TYPE details
	 *
	 *  @param type The type of \ref RDP_LOGON_INFO_TYPE extra data to decode
	 *  @param data The extra data of \ref type
	 *  @param buffer A string buffer that can hold the result
	 *  @param length The length of the string buffer in bytes
	 *
	 *  @return A string representation of \ref data
	 *  @since version 3.31.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API const char* freerdp_session_logon_type_data_str(uint32_t type, const void* data,
	                                                            char* buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SESSION_H */
