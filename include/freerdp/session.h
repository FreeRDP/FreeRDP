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
#ifndef __FREERDP_SESSION_H__
#define __FREERDP_SESSION_H__

#include <winpr/wtypes.h>

/* Logon Information Types */
#define INFO_TYPE_LOGON		0x00000000
#define INFO_TYPE_LOGON_LONG		0x00000001
#define INFO_TYPE_LOGON_PLAIN_NOTIFY	0x00000002
#define INFO_TYPE_LOGON_EXTENDED_INF	0x00000003

struct rdp_logon_info {
	UINT32 sessionId;
	char *username;
	char *domain;
};
typedef struct rdp_logon_info logon_info;

struct rdp_logon_info_ex {
	BOOL haveCookie;
	UINT32 LogonId;
	BYTE ArcRandomBits[16];

	BOOL haveErrorInfo;
	UINT32 ErrorNotificationType;
	UINT32 ErrorNotificationData;
};
typedef struct rdp_logon_info_ex logon_info_ex;

#endif /* __FREERDP_SESSION_H__ */
