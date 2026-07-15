/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDSTLS Security protocol
 *
 * Copyright 2023 Joan Torres <joan.torres@suse.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include "settings.h"

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/settings.h>

#include <winpr/assert.h>
#include <winpr/stream.h>
#include <winpr/wlog.h>

#include "rdstls.h"
#include "transport.h"
#include "utils.h"

#define RDSTLS_VERSION_1 0x01u
#define RDSTLS_VERSION_2 0x02u

#define RDSTLS_TYPE_CAPABILITIES 0x01
#define RDSTLS_TYPE_AUTHREQ 0x02
#define RDSTLS_TYPE_AUTHRSP 0x04

#define RDSTLS_DATA_CAPABILITIES 0x01
#define RDSTLS_DATA_PASSWORD_CREDS 0x01
#define RDSTLS_DATA_AUTORECONNECT_COOKIE 0x02
#define RDSTLS_DATA_FEDAUTH_TOKEN 0x03
#define RDSTLS_DATA_RESULT_CODE 0x01

typedef enum
{
	RDSTLS_STATE_INITIAL,
	RDSTLS_STATE_CAPABILITIES,
	RDSTLS_STATE_AUTH_REQ,
	RDSTLS_STATE_AUTH_RSP,
	RDSTLS_STATE_FINAL,
} RDSTLS_STATE;

typedef enum
{

	RDSTLS_RESULT_SUCCESS = 0x00000000,
	RDSTLS_RESULT_ACCESS_DENIED = 0x00000005,
	RDSTLS_RESULT_LOGON_FAILURE = 0x0000052e,
	RDSTLS_RESULT_INVALID_LOGON_HOURS = 0x00000530,
	RDSTLS_RESULT_PASSWORD_EXPIRED = 0x00000532,
	RDSTLS_RESULT_ACCOUNT_DISABLED = 0x00000533,
	RDSTLS_RESULT_PASSWORD_MUST_CHANGE = 0x00000773,
	RDSTLS_RESULT_ACCOUNT_LOCKED_OUT = 0x00000775
} RDSTLS_RESULT_CODE;

struct rdp_rdstls
{
	BOOL server;
	RDSTLS_STATE state;
	rdpContext* context;
	rdpTransport* transport;

	RDSTLS_RESULT_CODE resultCode;
	wLog* log;
	uint16_t supportedVersions;
};

static const uint16_t RDSTLS_VERSION_MASK = RDSTLS_VERSION_1 | RDSTLS_VERSION_2;

WINPR_ATTR_NODISCARD
static const char* rdstls_result_code_str(UINT32 resultCode)
{
	switch (resultCode)
	{
		case RDSTLS_RESULT_SUCCESS:
			return "RDSTLS_RESULT_SUCCESS";
		case RDSTLS_RESULT_ACCESS_DENIED:
			return "RDSTLS_RESULT_ACCESS_DENIED";
		case RDSTLS_RESULT_LOGON_FAILURE:
			return "RDSTLS_RESULT_LOGON_FAILURE";
		case RDSTLS_RESULT_INVALID_LOGON_HOURS:
			return "RDSTLS_RESULT_INVALID_LOGON_HOURS";
		case RDSTLS_RESULT_PASSWORD_EXPIRED:
			return "RDSTLS_RESULT_PASSWORD_EXPIRED";
		case RDSTLS_RESULT_ACCOUNT_DISABLED:
			return "RDSTLS_RESULT_ACCOUNT_DISABLED";
		case RDSTLS_RESULT_PASSWORD_MUST_CHANGE:
			return "RDSTLS_RESULT_PASSWORD_MUST_CHANGE";
		case RDSTLS_RESULT_ACCOUNT_LOCKED_OUT:
			return "RDSTLS_RESULT_ACCOUNT_LOCKED_OUT";
		default:
			return "RDSTLS_RESULT_UNKNOWN";
	}
}

#define rdstls_required_role_is_server(rdstls, isServer) \
	rdstls_required_role_is_server_((rdstls), (isServer), __FILE__, __func__, __LINE__)

WINPR_ATTR_NODISCARD
static BOOL rdstls_required_role_is_server_(const rdpRdstls* rdstls, BOOL isServer,
                                            const char* file, const char* fkt, size_t line)
{
	WINPR_ASSERT(rdstls);
	const BOOL rc = rdstls->server == isServer;
	if (!rc)
	{
		const DWORD level = WLOG_ERROR;
		if (WLog_IsLevelActive(rdstls->log, level))
			WLog_PrintTextMessage(rdstls->log, level, line, file, fkt,
			                      "Message not allowed in current role '%s'",
			                      rdstls->server ? "server" : "client");
	}
	return rc;
}

/**
 * Create new RDSTLS state machine.
 *
 * @param context A pointer to the rdp context to use
 *
 * @return new RDSTLS state machine.
 */

rdpRdstls* rdstls_new(rdpContext* context, rdpTransport* transport)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(transport);

	rdpSettings* settings = context->settings;
	WINPR_ASSERT(settings);

	rdpRdstls* rdstls = (rdpRdstls*)calloc(1, sizeof(rdpRdstls));

	if (!rdstls)
		return nullptr;
	rdstls->log = WLog_Get(FREERDP_TAG("core.rdstls"));
	rdstls->supportedVersions = RDSTLS_VERSION_MASK;
	rdstls->context = context;
	rdstls->transport = transport;
	rdstls->server = settings->ServerMode;

	rdstls->resultCode = RDSTLS_RESULT_ACCESS_DENIED;
	rdstls->state = RDSTLS_STATE_INITIAL;

	return rdstls;
}

/**
 * Free RDSTLS state machine.
 * @param rdstls The RDSTLS instance to free
 */

void rdstls_free(rdpRdstls* rdstls)
{
	free(rdstls);
}

WINPR_ATTR_NODISCARD
static const char* rdstls_get_state_str(RDSTLS_STATE state)
{
	switch (state)
	{
		case RDSTLS_STATE_INITIAL:
			return "RDSTLS_STATE_INITIAL";
		case RDSTLS_STATE_CAPABILITIES:
			return "RDSTLS_STATE_CAPABILITIES";
		case RDSTLS_STATE_AUTH_REQ:
			return "RDSTLS_STATE_AUTH_REQ";
		case RDSTLS_STATE_AUTH_RSP:
			return "RDSTLS_STATE_AUTH_RSP";
		case RDSTLS_STATE_FINAL:
			return "RDSTLS_STATE_FINAL";
		default:
			return "UNKNOWN";
	}
}

WINPR_ATTR_NODISCARD
static RDSTLS_STATE rdstls_get_state(rdpRdstls* rdstls)
{
	WINPR_ASSERT(rdstls);
	return rdstls->state;
}

WINPR_ATTR_NODISCARD
static BOOL check_transition(wLog* log, RDSTLS_STATE current, RDSTLS_STATE expected,
                             RDSTLS_STATE requested)
{
	if (requested != expected)
	{
		WLog_Print(log, WLOG_ERROR,
		           "Unexpected rdstls state transition from %s [%u] to %s [%u], expected %s [%u]",
		           rdstls_get_state_str(current), current, rdstls_get_state_str(requested),
		           requested, rdstls_get_state_str(expected), expected);
		return FALSE;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_set_state(rdpRdstls* rdstls, RDSTLS_STATE state)
{
	BOOL rc = FALSE;
	WINPR_ASSERT(rdstls);

	WLog_Print(rdstls->log, WLOG_DEBUG, "-- %s\t--> %s", rdstls_get_state_str(rdstls->state),
	           rdstls_get_state_str(state));

	switch (rdstls->state)
	{
		case RDSTLS_STATE_INITIAL:
			rc = check_transition(rdstls->log, rdstls->state, RDSTLS_STATE_CAPABILITIES, state);
			break;
		case RDSTLS_STATE_CAPABILITIES:
			rc = check_transition(rdstls->log, rdstls->state, RDSTLS_STATE_AUTH_REQ, state);
			break;
		case RDSTLS_STATE_AUTH_REQ:
			rc = check_transition(rdstls->log, rdstls->state, RDSTLS_STATE_AUTH_RSP, state);
			break;
		case RDSTLS_STATE_AUTH_RSP:
			rc = check_transition(rdstls->log, rdstls->state, RDSTLS_STATE_FINAL, state);
			break;
		case RDSTLS_STATE_FINAL:
			rc = check_transition(rdstls->log, rdstls->state, RDSTLS_STATE_CAPABILITIES, state);
			break;
		default:
			WLog_Print(rdstls->log, WLOG_ERROR,
			           "Invalid rdstls state %s [%u], requested transition to %s [%u]",
			           rdstls_get_state_str(rdstls->state), rdstls->state,
			           rdstls_get_state_str(state), state);
			break;
	}
	if (rc)
		rdstls->state = state;

	return rc;
}

#define rdstls_check_state_requirements(rdstls, expected) \
	rdstls_check_state_requirements_((rdstls), (expected), __FILE__, __func__, __LINE__)

WINPR_ATTR_NODISCARD
static BOOL rdstls_check_state_requirements_(rdpRdstls* rdstls, RDSTLS_STATE expected,
                                             const char* file, const char* fkt, size_t line)
{
	const RDSTLS_STATE current = rdstls_get_state(rdstls);
	if (current == expected)
		return TRUE;

	WINPR_ASSERT(rdstls);

	const DWORD log_level = WLOG_ERROR;
	if (WLog_IsLevelActive(rdstls->log, log_level))
		WLog_PrintTextMessage(rdstls->log, log_level, line, file, fkt,
		                      "Unexpected rdstls state %s [%u], expected %s [%u]",
		                      rdstls_get_state_str(current), current,
		                      rdstls_get_state_str(expected), expected);

	return FALSE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_write_capabilities(WINPR_ATTR_UNUSED rdpRdstls* rdstls, wStream* s)
{
	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, RDSTLS_VERSION_1);
	Stream_Write_UINT16(s, RDSTLS_TYPE_CAPABILITIES);
	Stream_Write_UINT16(s, RDSTLS_DATA_CAPABILITIES);
	Stream_Write_UINT16(s, rdstls->supportedVersions);

	return TRUE;
}

WINPR_ATTR_NODISCARD
static SSIZE_T rdstls_write_string(wStream* s, const char* str)
{
	const size_t pos = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, 2))
		return -1;

	if (!str)
	{
		/* Write unicode null */
		Stream_Write_UINT16(s, 2);
		if (!Stream_EnsureRemainingCapacity(s, 2))
			return -1;

		Stream_Write_UINT16(s, 0);
		return (SSIZE_T)(Stream_GetPosition(s) - pos);
	}

	const SSIZE_T devNameWLen = ConvertUtf8ToWChar(str, nullptr, 0);
	if (devNameWLen < 0)
		return -1;
	const size_t length = WINPR_ASSERTING_INT_CAST(size_t, devNameWLen) + 1;
	const size_t slen = strlen(str);

	Stream_Write_UINT16(s, (UINT16)length * sizeof(WCHAR));

	if (!Stream_EnsureRemainingCapacity(s, length * sizeof(WCHAR)))
		return -1;

	if (Stream_Write_UTF16_String_From_UTF8(s, length, str, slen, TRUE) < 0)
		return -1;

	return (SSIZE_T)(Stream_GetPosition(s) - pos);
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_write_data(wStream* s, UINT32 length, const BYTE* data)
{
	WINPR_ASSERT(data || (length == 0));

	if (!Stream_EnsureRemainingCapacity(s, 2) || (length > UINT16_MAX))
		return FALSE;

	Stream_Write_UINT16(s, (UINT16)length);

	if (!Stream_EnsureRemainingCapacity(s, length))
		return FALSE;

	Stream_Write(s, data, length);

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_write_cookie(wStream* s, const ARC_SC_PRIVATE_PACKET* cookie)
{
	WINPR_ASSERT(cookie);
	const uint16_t length = sizeof(ARC_SC_PRIVATE_PACKET);
	WINPR_STATIC_ASSERT(sizeof(ARC_SC_PRIVATE_PACKET) == 28);

	if (!Stream_EnsureRemainingCapacity(s, 2))
		return FALSE;

	Stream_Write_UINT16(s, length);

	if (!Stream_EnsureRemainingCapacity(s, length))
		return FALSE;

	Stream_Write_UINT32(s, cookie->cbLen);
	Stream_Write_UINT32(s, cookie->version);
	Stream_Write_UINT32(s, cookie->logonId);
	Stream_Write(s, cookie->arcRandomBits, sizeof(cookie->arcRandomBits));
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_read_cookie(wLog* log, wStream* s, ARC_SC_PRIVATE_PACKET* cookie)
{
	WINPR_ASSERT(cookie);
	const uint16_t length = sizeof(ARC_SC_PRIVATE_PACKET);
	WINPR_STATIC_ASSERT(sizeof(ARC_SC_PRIVATE_PACKET) == 28);

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, length + 2ull))
		return FALSE;

	const uint16_t len = Stream_Get_UINT16(s);
	if (len != length)
	{
		WLog_Print(log, WLOG_ERROR,
		           "RDSTLS Cookie: Unexpected length %" PRIu16 ",  expected %" PRIu16, len, length);
		return FALSE;
	}

	cookie->cbLen = Stream_Get_UINT32(s);
	cookie->version = Stream_Get_UINT32(s);
	cookie->logonId = Stream_Get_UINT32(s);
	Stream_Read(s, cookie->arcRandomBits, sizeof(cookie->arcRandomBits));
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_write_authentication_request_with_password(rdpRdstls* rdstls, wStream* s)
{
	WINPR_ASSERT(rdstls);
	WINPR_ASSERT(rdstls->context);

	if (!rdstls_required_role_is_server(rdstls, FALSE))
		return FALSE;
	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_REQ))
		return FALSE;

	WLog_Print(rdstls->log, WLOG_DEBUG, "Writing RDSTLS password authentication message");

	rdpSettings* settings = rdstls->context->settings;
	WINPR_ASSERT(settings);

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return FALSE;

	Stream_Write_UINT16(s, RDSTLS_TYPE_AUTHREQ);
	Stream_Write_UINT16(s, RDSTLS_DATA_PASSWORD_CREDS);

	if (!rdstls_write_data(s, settings->RedirectionGuidLength, settings->RedirectionGuid))
		return FALSE;

	if (rdstls_write_string(s, settings->Username) < 0)
		return FALSE;

	if (rdstls_write_string(s, settings->Domain) < 0)
		return FALSE;

	if (!rdstls_write_data(s, settings->RedirectionPasswordLength, settings->RedirectionPassword))
		return FALSE;

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_write_authentication_request_with_cookie(WINPR_ATTR_UNUSED rdpRdstls* rdstls,
                                                            WINPR_ATTR_UNUSED wStream* s)
{
	WINPR_ASSERT(rdstls);
	WINPR_ASSERT(rdstls->context);

	WLog_Print(rdstls->log, WLOG_DEBUG, "Writing RDSTLS cookie authentication message");

	if (!rdstls_required_role_is_server(rdstls, FALSE))
		return FALSE;
	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_REQ))
		return FALSE;

	rdpSettings* settings = rdstls->context->settings;
	WINPR_ASSERT(settings);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, RDSTLS_TYPE_AUTHREQ);
	Stream_Write_UINT16(s, RDSTLS_DATA_AUTORECONNECT_COOKIE);
	Stream_Write_UINT32(s, settings->RedirectedSessionId);

	return (rdstls_write_cookie(s, settings->ServerAutoReconnectCookie));
}

/*
 * Warn if the endpoint FedAuth token targets a different virtual machine
 * than the VM identifier passed via the .rdp `pcb` field / /pcb command
 * line switch. The token payload starts with "VMID=<guid>&..."; a
 * mismatch would be silently rejected by the server later on. This is a
 * best-effort local sanity check.
 */
static void rdstls_check_fedauth_vmid(rdpRdstls* rdstls, const char* token, const char* selectedVm)
{
	WINPR_ASSERT(rdstls);
	WINPR_ASSERT(token);

	if (!selectedVm || !*selectedVm)
		return;

	const char* vmidField = strstr(token, "VMID=");
	if (!vmidField)
		return;
	vmidField += 5;

	const size_t vmLen = strlen(selectedVm);
	const BOOL matches = (_strnicmp(vmidField, selectedVm, vmLen) == 0) &&
	                     (vmidField[vmLen] == '\0' || vmidField[vmLen] == '&');
	if (!matches)
	{
		WLog_Print(rdstls->log, WLOG_WARN,
		           "endpoint FedAuth token is issued for a different virtual machine "
		           "than the one selected for connection");
	}
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_write_authentication_request_with_fedauth_token(rdpRdstls* rdstls, wStream* s)
{
	WINPR_ASSERT(rdstls);
	WINPR_ASSERT(rdstls->context);

	WLog_Print(rdstls->log, WLOG_DEBUG, "Writing RDSTLS FedAuth token authentication message");

	if (!rdstls_required_role_is_server(rdstls, FALSE))
		return FALSE;
	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_REQ))
		return FALSE;

	const rdpSettings* settings = rdstls->context->settings;
	WINPR_ASSERT(settings);

	const char* token = freerdp_settings_get_string(settings, FreeRDP_EndpointFedAuthToken);
	if (!token || !*token)
	{
		WLog_Print(rdstls->log, WLOG_ERROR, "EndpointFedAuthToken not set");
		return FALSE;
	}

	rdstls_check_fedauth_vmid(rdstls, token,
	                          freerdp_settings_get_string(settings, FreeRDP_PreconnectionBlob));

	const size_t utf8Length = strlen(token);
	/* The wire length prefix is a UINT16 counting the token in UTF-16LE
	 * including a terminating NUL character. */
	if (utf8Length >= UINT16_MAX / sizeof(WCHAR))
	{
		WLog_Print(rdstls->log, WLOG_ERROR,
		           "EndpointFedAuthToken length %" PRIuz " exceeds RDSTLS wire limit", utf8Length);
		return FALSE;
	}

	const size_t wideLength = utf8Length + 1;
	const size_t wideBytes = wideLength * sizeof(WCHAR);

	if (!Stream_EnsureRemainingCapacity(s, 6 + wideBytes))
		return FALSE;

	Stream_Write_UINT16(s, RDSTLS_TYPE_AUTHREQ);
	Stream_Write_UINT16(s, RDSTLS_DATA_FEDAUTH_TOKEN);
	Stream_Write_UINT16(s, (UINT16)wideBytes);

	return Stream_Write_UTF16_String_From_UTF8(s, wideLength, token, utf8Length, TRUE) >= 0;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_write_authentication_response(rdpRdstls* rdstls, wStream* s)
{
	WINPR_ASSERT(rdstls);

	if (!rdstls_required_role_is_server(rdstls, TRUE))
		return FALSE;
	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_RSP))
		return FALSE;
	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, RDSTLS_TYPE_AUTHRSP);
	Stream_Write_UINT16(s, RDSTLS_DATA_RESULT_CODE);
	Stream_Write_UINT32(s, rdstls->resultCode);

	return TRUE;
}

#define rdstls_version_required(log, expected, actual) \
	rdstls_version_required_((log), (expected), (actual), __FILE__, __func__, __LINE__)
WINPR_ATTR_NODISCARD
static BOOL rdstls_version_required_(wLog* log, uint16_t expected, uint16_t actual,
                                     const char* file, const char* fkt, size_t line)
{
	if (actual < expected)
	{
		const DWORD level = WLOG_ERROR;
		if (WLog_IsLevelActive(log, level))
		{
			WLog_PrintTextMessage(log, WLOG_ERROR, line, file, fkt,
			                      "version=0x%04" PRIx16 ", expected at least 0x%04" PRIx16, actual,
			                      expected);
		}
		return FALSE;
	}
	return TRUE;
}

#define rdstls_are_some_versions_supported(log, version, mask) \
	rdstls_are_some_versions_supported_((log), (version), (mask), __FILE__, __func__, __LINE__)
WINPR_ATTR_NODISCARD
static BOOL rdstls_are_some_versions_supported_(wLog* log, uint16_t version, BOOL isMask,
                                                const char* file, const char* fkt, size_t line)
{
	if (!isMask)
	{
		size_t cnt = 0;
		for (size_t x = 0; x < 16; x++)
		{
			const unsigned val = 1 << x;
			if ((version & val) != 0)
				cnt++;
		}
		if (cnt != 1)
		{
			WLog_PrintTextMessage(log, WLOG_ERROR, line, file, fkt,
			                      "received invalid version mask=0x%04" PRIx16
			                      ", expected { 0x%04" PRIx32 ", 0x%04" PRIx32 "}",
			                      version, RDSTLS_VERSION_1, RDSTLS_VERSION_2);
			return FALSE;
		}
	}

	if ((version & RDSTLS_VERSION_MASK) == 0)
	{
		const DWORD level = WLOG_ERROR;
		if (WLog_IsLevelActive(log, level))
		{
			WLog_PrintTextMessage(log, WLOG_ERROR, line, file, fkt,
			                      "received invalid version mask=0x%04" PRIx16
			                      ", expected { 0x%04" PRIx32 ", 0x%04" PRIx32 "}",
			                      version, RDSTLS_VERSION_1, RDSTLS_VERSION_2);
		}
		return FALSE;
	}
	return TRUE;
}

#define rdstls_is_version_supported(rdstls, versions) \
	rdstls_is_version_supported_((rdstls), (version), __FILE__, __func__, __LINE__)
WINPR_ATTR_NODISCARD
static BOOL rdstls_is_version_supported_(rdpRdstls* rdstls, uint16_t version, const char* file,
                                         const char* fkt, size_t line)
{
	WINPR_ASSERT(rdstls);

	if ((rdstls->supportedVersions & version) == 0)
	{
		const DWORD level = WLOG_ERROR;
		if (WLog_IsLevelActive(rdstls->log, level))
		{
			WLog_PrintTextMessage(rdstls->log, WLOG_ERROR, line, file, fkt,
			                      "received invalid version=0x%04" PRIx16
			                      ", expected { 0x%04" PRIx32 ", 0x%04" PRIx32 "}",
			                      version, RDSTLS_VERSION_1, RDSTLS_VERSION_2);
		}
		return FALSE;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_process_capabilities(rdpRdstls* rdstls, wStream* s)
{
	WINPR_ASSERT(rdstls);
	if (!rdstls_required_role_is_server(rdstls, FALSE))
		return FALSE;
	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_CAPABILITIES))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, 4))
		return FALSE;

	const UINT16 dataType = Stream_Get_UINT16(s);
	if (dataType != RDSTLS_DATA_CAPABILITIES)
	{
		WLog_Print(rdstls->log, WLOG_ERROR,
		           "received invalid DataType=0x%04" PRIX16 ", expected 0x%04" PRIX32, dataType,
		           WINPR_CXX_COMPAT_CAST(UINT32, RDSTLS_DATA_CAPABILITIES));
		return FALSE;
	}

	const UINT16 supportedVersions = Stream_Get_UINT16(s);
	if (!rdstls_are_some_versions_supported(rdstls->log, supportedVersions, TRUE))
		return FALSE;
	rdstls->supportedVersions = supportedVersions & RDSTLS_VERSION_MASK;

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_read_unicode_string(WINPR_ATTR_UNUSED wLog* log, wStream* s, char** str)
{
	WINPR_ASSERT(str);

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 2))
		return FALSE;

	const UINT16 length = Stream_Get_UINT16(s);

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, length))
		return FALSE;

	if (length <= 2)
	{
		*str = nullptr;
		Stream_Seek(s, length);
		return TRUE;
	}

	*str = Stream_Read_UTF16_String_As_UTF8(s, length / sizeof(WCHAR), nullptr);
	return (*str) != nullptr;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_read_data(WINPR_ATTR_UNUSED wLog* log, wStream* s, UINT16* pLength,
                             const BYTE** pData)
{
	WINPR_ASSERT(pLength);
	WINPR_ASSERT(pData);

	*pData = nullptr;
	*pLength = 0;
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 2))
		return FALSE;

	const UINT16 length = Stream_Get_UINT16(s);

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, length))
		return FALSE;

	if (length <= 2)
	{
		Stream_Seek(s, length);
		return TRUE;
	}

	*pData = Stream_ConstPointer(s);
	*pLength = length;
	Stream_Seek(s, length);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_cmp_data(wLog* log, const char* field, const BYTE* serverData,
                            const UINT32 serverDataLength, const BYTE* clientData,
                            const UINT16 clientDataLength)
{
	if (serverDataLength > 0)
	{
		if (clientDataLength == 0)
		{
			WLog_Print(log, WLOG_ERROR, "expected %s", field);
			return FALSE;
		}

		if (serverDataLength > UINT16_MAX || serverDataLength != clientDataLength ||
		    memcmp(serverData, clientData, serverDataLength) != 0)
		{
			WLog_Print(log, WLOG_ERROR, "%s verification failed", field);
			return FALSE;
		}
	}

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_cmp_str(wLog* log, const char* field, const char* serverStr,
                           const char* clientStr)
{
	if (!utils_str_is_empty(serverStr))
	{
		if (utils_str_is_empty(clientStr))
		{
			WLog_Print(log, WLOG_ERROR, "expected %s", field);
			return FALSE;
		}

		WINPR_ASSERT(serverStr);
		WINPR_ASSERT(clientStr);
		if (strcmp(serverStr, clientStr) != 0)
		{
			WLog_Print(log, WLOG_ERROR, "%s verification failed", field);
			return FALSE;
		}
	}

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_process_authentication_request_with_password(rdpRdstls* rdstls, wStream* s,
                                                                uint16_t version)
{
	WINPR_ASSERT(rdstls);
	WINPR_ASSERT(rdstls->context);

	if (!rdstls_version_required(rdstls->log, RDSTLS_VERSION_1, version))
		return FALSE;
	if (!rdstls_required_role_is_server(rdstls, TRUE))
		return FALSE;
	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_REQ))
		return FALSE;

	BOOL rc = FALSE;

	const BYTE* clientRedirectionGuid = nullptr;
	UINT16 clientRedirectionGuidLength = 0;
	char* clientPassword = nullptr;
	char* clientUsername = nullptr;
	char* clientDomain = nullptr;

	const rdpSettings* settings = rdstls->context->settings;
	WINPR_ASSERT(settings);

	if (!rdstls_read_data(rdstls->log, s, &clientRedirectionGuidLength, &clientRedirectionGuid))
		goto fail;

	if (!rdstls_read_unicode_string(rdstls->log, s, &clientUsername))
		goto fail;

	if (!rdstls_read_unicode_string(rdstls->log, s, &clientDomain))
		goto fail;

	if (!rdstls_read_unicode_string(rdstls->log, s, &clientPassword))
		goto fail;

	{
		const BYTE* serverRedirectionGuid =
		    freerdp_settings_get_pointer(settings, FreeRDP_RedirectionGuid);
		const UINT32 serverRedirectionGuidLength =
		    freerdp_settings_get_uint32(settings, FreeRDP_RedirectionGuidLength);
		const char* serverUsername = freerdp_settings_get_string(settings, FreeRDP_Username);
		const char* serverDomain = freerdp_settings_get_string(settings, FreeRDP_Domain);
		const char* serverPassword = freerdp_settings_get_string(settings, FreeRDP_Password);

		if (!rdstls_cmp_data(rdstls->log, "RedirectionGuid", serverRedirectionGuid,
		                     serverRedirectionGuidLength, clientRedirectionGuid,
		                     clientRedirectionGuidLength))
			rdstls->resultCode = RDSTLS_RESULT_ACCESS_DENIED;
		else if (!rdstls_cmp_str(rdstls->log, "UserName", serverUsername, clientUsername))
			rdstls->resultCode = RDSTLS_RESULT_LOGON_FAILURE;
		else if (!rdstls_cmp_str(rdstls->log, "Domain", serverDomain, clientDomain))
			rdstls->resultCode = RDSTLS_RESULT_LOGON_FAILURE;
		else if (!rdstls_cmp_str(rdstls->log, "Password", serverPassword, clientPassword))
			rdstls->resultCode = RDSTLS_RESULT_LOGON_FAILURE;
		else
			rdstls->resultCode = RDSTLS_RESULT_SUCCESS;
	}
	rc = TRUE;
fail:
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_process_authentication_request_with_cookie(rdpRdstls* rdstls, wStream* s,
                                                              uint16_t version)
{
	if (!rdstls_version_required(rdstls->log, RDSTLS_VERSION_1, version))
		return FALSE;

	if (!rdstls_required_role_is_server(rdstls, TRUE))
		return FALSE;
	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_REQ))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, 2))
		return FALSE;

	const rdpSettings* settings = rdstls->context->settings;
	WINPR_ASSERT(settings);

	const uint32_t id = Stream_Get_UINT32(s);
	const uint32_t expected = freerdp_settings_get_uint32(settings, FreeRDP_RedirectedSessionId);
	if (id != expected)
	{
		WLog_Print(rdstls->log, WLOG_ERROR,
		           "RDSTLS Cookie SessionId does not match RedirectedSessionId. Deny access.");
		return FALSE;
	}

	ARC_SC_PRIVATE_PACKET cookie = WINPR_C_ARRAY_INIT;
	if (!rdstls_read_cookie(rdstls->log, s, &cookie))
		return FALSE;

	const ARC_SC_PRIVATE_PACKET* expect =
	    freerdp_settings_get_pointer(settings, FreeRDP_ServerAutoReconnectCookie);
	if (!expect)
	{
		WLog_Print(rdstls->log, WLOG_ERROR, "No RDSTLS Cookie provided by server. Deny access.");
		return FALSE;
	}

	if (memcmp(expect, &cookie, sizeof(ARC_SC_PRIVATE_PACKET)) != 0)
	{
		WLog_Print(rdstls->log, WLOG_ERROR, "RDSTLS Cookie does not match. Deny access.");
		return FALSE;
	}

	WLog_Print(rdstls->log, WLOG_DEBUG, "RDSTLS Cookie matches. Grant access.");
	return FALSE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_process_authentication_request_with_fedauth_token(rdpRdstls* rdstls, wStream* s,
                                                                     uint16_t version)
{
	WINPR_ASSERT(rdstls);

	if (!rdstls_version_required(rdstls->log, RDSTLS_VERSION_2, version))
		return FALSE;
	if (!rdstls_required_role_is_server(rdstls, TRUE))
		return FALSE;
	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_REQ))
		return FALSE;
	if ((rdstls->supportedVersions & RDSTLS_VERSION_2) == 0)
	{
		WLog_Print(rdstls->log, WLOG_ERROR, "FedAuth token only supported with RDSTLS_VERSION_2");
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, 2))
		return FALSE;
	const uint16_t wbytes = Stream_Get_UINT16(s);
	if (wbytes == 0)
	{
		WLog_Print(rdstls->log, WLOG_ERROR, "Empty FedAuth token given by client. Deny access");
		return FALSE;
	}
	if ((wbytes % sizeof(WCHAR)) != 0)
	{
		WLog_Print(rdstls->log, WLOG_ERROR,
		           "Invalid FedAuth token length %" PRIu16 "given by client. Must be even", wbytes);
		return FALSE;
	}
	const size_t wcharlen = wbytes / sizeof(WCHAR);
	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, wbytes))
		return FALSE;

	const rdpSettings* settings = rdstls->context->settings;
	WINPR_ASSERT(settings);

	size_t len = 0;
	WCHAR* token =
	    freerdp_settings_get_string_as_utf16(settings, FreeRDP_EndpointFedAuthToken, &len);
	if (!token || (len == 0))
	{
		free(token);
		WLog_Print(rdstls->log, WLOG_ERROR,
		           "No FedAuth token provided by server to compare. Deny access");
		return FALSE;
	}

	if (len != wcharlen)
	{
		WLog_Print(rdstls->log, WLOG_ERROR, "FedAuth token length does not match. Deny access");
		free(token);
		return FALSE;
	}

	const int rc = memcmp(token, Stream_Pointer(s), len * sizeof(WCHAR));
	free(token);
	if (rc != 0)
	{
		WLog_Print(rdstls->log, WLOG_ERROR, "FedAuth token does not match. Deny access");
		return FALSE;
	}

	WLog_Print(rdstls->log, WLOG_INFO, "FedAuth token does match. Grant access");
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_process_authentication_request(rdpRdstls* rdstls, wStream* s, uint16_t version)
{
	if (!rdstls_required_role_is_server(rdstls, TRUE))
		return FALSE;

	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_REQ))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, 2))
		return FALSE;

	const UINT16 dataType = Stream_Get_UINT16(s);
	switch (dataType)
	{
		case RDSTLS_DATA_PASSWORD_CREDS:
			if (!rdstls_process_authentication_request_with_password(rdstls, s, version))
				return FALSE;
			break;
		case RDSTLS_DATA_AUTORECONNECT_COOKIE:
			if (!rdstls_process_authentication_request_with_cookie(rdstls, s, version))
				return FALSE;
			break;
		case RDSTLS_DATA_FEDAUTH_TOKEN:
			if (!rdstls_process_authentication_request_with_fedauth_token(rdstls, s, version))
				return FALSE;
			break;
		default:
			WLog_Print(rdstls->log, WLOG_ERROR,
			           "received invalid DataType=0x%04" PRIX16 ", expected 0x%04" PRIX32
			           " or 0x%04" PRIX32,
			           dataType, WINPR_CXX_COMPAT_CAST(UINT32, RDSTLS_DATA_PASSWORD_CREDS),
			           WINPR_CXX_COMPAT_CAST(UINT32, RDSTLS_DATA_AUTORECONNECT_COOKIE));
			return FALSE;
	}

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_process_authentication_response(rdpRdstls* rdstls, wStream* s)
{
	if (!rdstls_required_role_is_server(rdstls, FALSE))
		return FALSE;
	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_RSP))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, 6))
		return FALSE;

	const UINT16 dataType = Stream_Get_UINT16(s);
	if (dataType != RDSTLS_DATA_RESULT_CODE)
	{
		WLog_Print(rdstls->log, WLOG_ERROR,
		           "received invalid DataType=0x%04" PRIX16 ", expected 0x%04" PRIX32, dataType,
		           WINPR_CXX_COMPAT_CAST(UINT32, RDSTLS_DATA_RESULT_CODE));
		return FALSE;
	}

	const UINT32 resultCode = Stream_Get_UINT32(s);
	if (resultCode != RDSTLS_RESULT_SUCCESS)
	{
		WLog_Print(rdstls->log, WLOG_ERROR, "resultCode: %s [0x%08" PRIX32 "]",
		           rdstls_result_code_str(resultCode), resultCode);

		UINT32 error = FREERDP_ERROR_CONNECT_UNDEFINED;
		switch (resultCode)
		{
			case RDSTLS_RESULT_ACCESS_DENIED:
				error = FREERDP_ERROR_CONNECT_ACCESS_DENIED;
				break;
			case RDSTLS_RESULT_ACCOUNT_DISABLED:
				error = FREERDP_ERROR_CONNECT_ACCOUNT_DISABLED;
				break;
			case RDSTLS_RESULT_ACCOUNT_LOCKED_OUT:
				error = FREERDP_ERROR_CONNECT_ACCOUNT_LOCKED_OUT;
				break;
			case RDSTLS_RESULT_LOGON_FAILURE:
				error = FREERDP_ERROR_CONNECT_LOGON_FAILURE;
				break;
			case RDSTLS_RESULT_INVALID_LOGON_HOURS:
				error = FREERDP_ERROR_CONNECT_ACCOUNT_RESTRICTION;
				break;
			case RDSTLS_RESULT_PASSWORD_EXPIRED:
				error = FREERDP_ERROR_CONNECT_PASSWORD_EXPIRED;
				break;
			case RDSTLS_RESULT_PASSWORD_MUST_CHANGE:
				error = FREERDP_ERROR_CONNECT_PASSWORD_MUST_CHANGE;
				break;
			default:
				WLog_Print(rdstls->log, WLOG_ERROR,
				           "Unexpected resultCode: [0x%08" PRIX32 "], NTSTATUS=%s, Win32Error=%s",
				           resultCode, GetSecurityStatusString((SECURITY_STATUS)resultCode),
				           Win32ErrorCode2Tag(resultCode & 0xFFFF));
				error = FREERDP_ERROR_CONNECT_UNDEFINED;
				break;
		}

		freerdp_set_last_error_if_not(rdstls->context, error);
		return FALSE;
	}

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_send_capabilities(rdpRdstls* rdstls)
{
	BOOL rc = FALSE;

	if (!rdstls_required_role_is_server(rdstls, TRUE))
		return FALSE;

	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_CAPABILITIES))
		return FALSE;

	wStream* s = Stream_New(nullptr, 512);
	if (!s)
		goto fail;

	if (!rdstls_write_capabilities(rdstls, s))
		goto fail;
	if (transport_write(rdstls->transport, s) < 0)
		goto fail;

	rc = rdstls_set_state(rdstls, RDSTLS_STATE_AUTH_REQ);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_recv_authentication_request(rdpRdstls* rdstls, uint16_t* pVersion)
{
	BOOL rc = FALSE;
	WINPR_ASSERT(pVersion);

	if (!rdstls_required_role_is_server(rdstls, TRUE))
		return FALSE;
	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_REQ))
		return FALSE;

	wStream* s = Stream_New(nullptr, 4096);
	if (!s)
		goto fail;

	WINPR_ASSERT(rdstls);

	{
		const int res = transport_read_pdu(rdstls->transport, s);
		if (res < 0)
			goto fail;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, 4))
		goto fail;

	const UINT16 version = Stream_Get_UINT16(s);
	if (!rdstls_is_version_supported(rdstls, version))
		goto fail;
	*pVersion = version;

	const UINT16 pduType = Stream_Get_UINT16(s);
	switch (pduType)
	{
		case RDSTLS_TYPE_AUTHREQ:
			if (!rdstls_process_authentication_request(rdstls, s, version))
				goto fail;
			break;
		default:
			WLog_Print(rdstls->log, WLOG_ERROR,
			           "Invalid RDSTLS PDU type [0x%04" PRIx16 "] while reading AUTHREQ", pduType);
			goto fail;
	}

	rc = rdstls_set_state(rdstls, RDSTLS_STATE_AUTH_RSP);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_send_authentication_response(rdpRdstls* rdstls, uint16_t version)
{
	BOOL rc = FALSE;

	if (!rdstls_required_role_is_server(rdstls, TRUE))
		return FALSE;

	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_RSP))
		return FALSE;

	wStream* s = Stream_New(nullptr, 512);
	if (!s)
		goto fail;

	if (!Stream_EnsureRemainingCapacity(s, 2))
		goto fail;

	Stream_Write_UINT16(s, version);

	if (!rdstls_write_authentication_response(rdstls, s))
		goto fail;

	if (transport_write(rdstls->transport, s) < 0)
		goto fail;

	rc = rdstls_set_state(rdstls, RDSTLS_STATE_FINAL);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_recv_capabilities(rdpRdstls* rdstls)
{
	BOOL rc = FALSE;

	if (!rdstls_required_role_is_server(rdstls, FALSE))
		return FALSE;

	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_CAPABILITIES))
		return FALSE;

	wStream* s = Stream_New(nullptr, 512);
	if (!s)
		goto fail;

	WINPR_ASSERT(rdstls);

	{
		const int res = transport_read_pdu(rdstls->transport, s);
		if (res < 0)
			goto fail;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, 4))
		goto fail;

	const UINT16 version = Stream_Get_UINT16(s);
	if (!rdstls_is_version_supported(rdstls, version))
		goto fail;

	const UINT16 pduType = Stream_Get_UINT16(s);
	switch (pduType)
	{
		case RDSTLS_TYPE_CAPABILITIES:
			if (!rdstls_process_capabilities(rdstls, s))
				goto fail;
			break;
		default:
			WLog_Print(rdstls->log, WLOG_ERROR,
			           "Invalid pduType 0x%04" PRIx16 " while reading capability", pduType);
			goto fail;
	}

	rc = rdstls_set_state(rdstls, RDSTLS_STATE_AUTH_REQ);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_send_authentication_request(rdpRdstls* rdstls, uint16_t* pVersion)
{
	WINPR_ASSERT(pVersion);

	BOOL rc = FALSE;

	if (!rdstls_required_role_is_server(rdstls, FALSE))
		return FALSE;

	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_REQ))
		return FALSE;

	wStream* s = Stream_New(nullptr, 4096);
	if (!s)
		goto fail;

	WINPR_ASSERT(rdstls->context);

	const rdpSettings* settings = rdstls->context->settings;
	WINPR_ASSERT(settings);

	if (!Stream_EnsureRemainingCapacity(s, 2))
		goto fail;

	const RDSTLS_STATE state = rdstls_get_state(rdstls);
	const char* fedAuthToken = freerdp_settings_get_string(settings, FreeRDP_EndpointFedAuthToken);
	BOOL useFedAuth = (state == RDSTLS_STATE_AUTH_REQ) && !utils_str_is_empty(fedAuthToken);
	if ((rdstls->supportedVersions & RDSTLS_VERSION_2) == 0)
	{
		useFedAuth = FALSE;
		WLog_Print(rdstls->log, WLOG_WARN,
		           "Client has FedAuthToken ready, but server did not announce RDSTLS_VERSION_2.");
	}

	*pVersion = useFedAuth ? RDSTLS_VERSION_2 : RDSTLS_VERSION_1;
	Stream_Write_UINT16(s, *pVersion);

	if (useFedAuth)
	{
		if (!rdstls_write_authentication_request_with_fedauth_token(rdstls, s))
			goto fail;
	}
	else if (settings->RedirectionFlags & LB_PASSWORD_IS_PK_ENCRYPTED)
	{
		if (!rdstls_write_authentication_request_with_password(rdstls, s))
			goto fail;
	}
	else if (settings->ServerAutoReconnectCookie != nullptr)
	{
		if (!rdstls_write_authentication_request_with_cookie(rdstls, s))
			goto fail;
	}
	else
	{
		WLog_Print(rdstls->log, WLOG_ERROR,
		           "cannot authenticate with FedAuth token, password or "
		           "auto-reconnect cookie");
		goto fail;
	}

	WINPR_ASSERT(rdstls);
	if (transport_write(rdstls->transport, s) < 0)
		goto fail;

	rc = rdstls_set_state(rdstls, RDSTLS_STATE_AUTH_RSP);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL rdstls_recv_authentication_response(rdpRdstls* rdstls, uint16_t expected)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(rdstls);

	if (!rdstls_required_role_is_server(rdstls, FALSE))
		return FALSE;

	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_RSP))
		return FALSE;

	wStream* s = Stream_New(nullptr, 512);
	if (!s)
		goto fail;

	{
		const int res = transport_read_pdu(rdstls->transport, s);
		if (res < 0)
			goto fail;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, 4))
		goto fail;

	const UINT16 version = Stream_Get_UINT16(s);
	if (!rdstls_is_version_supported(rdstls, version))
		goto fail;
	if (version != expected)
		goto fail;

	const UINT16 pduType = Stream_Get_UINT16(s);
	switch (pduType)
	{
		case RDSTLS_TYPE_AUTHRSP:
			if (!rdstls_process_authentication_response(rdstls, s))
				goto fail;
			break;
		default:
			WLog_Print(rdstls->log, WLOG_ERROR,
			           "Invalid RDSTLS PDU type [0x%04" PRIx16 "] while reading AUTHRSP", pduType);
			goto fail;
	}

	rc = rdstls_set_state(rdstls, RDSTLS_STATE_FINAL);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

WINPR_ATTR_NODISCARD
static int rdstls_server_authenticate(rdpRdstls* rdstls)
{
	WINPR_ASSERT(rdstls);
	uint16_t version = 0;

	if (!rdstls_set_state(rdstls, RDSTLS_STATE_CAPABILITIES))
		return -1;

	if (!rdstls_send_capabilities(rdstls))
		return -1;

	if (!rdstls_recv_authentication_request(rdstls, &version))
		return -1;

	if (!rdstls_send_authentication_response(rdstls, version))
		return -1;

	if (rdstls->resultCode != RDSTLS_RESULT_SUCCESS)
		return -1;

	return 1;
}

WINPR_ATTR_NODISCARD
static int rdstls_client_authenticate(rdpRdstls* rdstls)
{
	if (!rdstls_set_state(rdstls, RDSTLS_STATE_CAPABILITIES))
		return -1;

	if (!rdstls_recv_capabilities(rdstls))
		return -1;

	uint16_t version = 0;
	if (!rdstls_send_authentication_request(rdstls, &version))
		return -1;

	if (!rdstls_recv_authentication_response(rdstls, version))
		return -1;

	return 1;
}

/**
 * Authenticate using RDSTLS.
 * @param rdstls The RDSTLS instance to use
 *
 * @return 1 if authentication is successful
 */

int rdstls_authenticate(rdpRdstls* rdstls)
{
	WINPR_ASSERT(rdstls);

	if (rdstls->server)
		return rdstls_server_authenticate(rdstls);
	else
		return rdstls_client_authenticate(rdstls);
}

WINPR_ATTR_NODISCARD
static SSIZE_T rdstls_parse_pdu_data_type(wLog* log, UINT16 dataType, wStream* s)
{
	size_t pduLength = 0;

	switch (dataType)
	{
		case RDSTLS_DATA_PASSWORD_CREDS:
		{
			if (Stream_GetRemainingLength(s) < 2)
				return 0;

			const UINT16 redirGuidLength = Stream_Get_UINT16(s);

			if (Stream_GetRemainingLength(s) < redirGuidLength)
				return 0;
			Stream_Seek(s, redirGuidLength);

			if (Stream_GetRemainingLength(s) < 2)
				return 0;

			const UINT16 usernameLength = Stream_Get_UINT16(s);

			if (Stream_GetRemainingLength(s) < usernameLength)
				return 0;
			Stream_Seek(s, usernameLength);

			if (Stream_GetRemainingLength(s) < 2)
				return 0;
			const UINT16 domainLength = Stream_Get_UINT16(s);

			if (Stream_GetRemainingLength(s) < domainLength)
				return 0;
			Stream_Seek(s, domainLength);

			if (Stream_GetRemainingLength(s) < 2)
				return 0;
			const UINT16 passwordLength = Stream_Get_UINT16(s);

			if (passwordLength == 0)
			{
				WLog_Print(log, WLOG_ERROR, "invalid RDSLTS PASSWORD_CREDS: empty password");
				return -1;
			}
			else if ((redirGuidLength == 0) && (usernameLength == 0) && (domainLength == 0) &&
			         (passwordLength == 0))
			{
				WLog_Print(log, WLOG_ERROR, "invalid RDSLTS PASSWORD_CREDS: lengths 0");
				return -1;
			}
			pduLength = Stream_GetPosition(s) + passwordLength;
		}
		break;
		case RDSTLS_DATA_AUTORECONNECT_COOKIE:
		{
			if (Stream_GetRemainingLength(s) < 6)
				return 0;
			Stream_Seek(s, 4);
			const UINT16 cookieLength = Stream_Get_UINT16(s);
			if (cookieLength == 0)
			{
				WLog_Print(log, WLOG_ERROR, "invalid RDSLTS COOKIE::length");
				return -1;
			}
			pduLength = Stream_GetPosition(s) + cookieLength;
		}
		break;
		case RDSTLS_DATA_FEDAUTH_TOKEN:
		{
			if (Stream_GetRemainingLength(s) < 6)
				return 0;
			Stream_Seek(s, 4);
			const UINT16 tokenLength = Stream_Get_UINT16(s);
			if (tokenLength == 0)
			{
				WLog_Print(log, WLOG_ERROR, "invalid RDSLTS FEDAUTH_TOKEN::length");
				return -1;
			}
			pduLength = Stream_GetPosition(s) + tokenLength;
		}
		break;
		default:
			WLog_Print(log, WLOG_ERROR, "invalid RDSLTS dataType");
			return -1;
	}

	if (pduLength > SSIZE_MAX)
		return 0;
	return (SSIZE_T)pduLength;
}

SSIZE_T rdstls_parse_pdu(wLog* log, wStream* stream)
{
	SSIZE_T pduLength = -1;
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, Stream_Buffer(stream), Stream_Length(stream));

	if (Stream_GetRemainingLength(s) < 2)
		return 0;

	const UINT16 version = Stream_Get_UINT16(s);
	if (!rdstls_are_some_versions_supported(log, version, FALSE))
		return -1;

	if (Stream_GetRemainingLength(s) < 2)
		return 0;

	const UINT16 pduType = Stream_Get_UINT16(s);
	switch (pduType)
	{
		case RDSTLS_TYPE_CAPABILITIES:
			pduLength = 8;
			break;
		case RDSTLS_TYPE_AUTHREQ:
		{
			if (Stream_GetRemainingLength(s) < 2)
				return 0;

			const UINT16 dataType = Stream_Get_UINT16(s);
			pduLength = rdstls_parse_pdu_data_type(log, dataType, s);
		}
		break;
		case RDSTLS_TYPE_AUTHRSP:
			pduLength = 10;
			break;
		default:
			WLog_Print(log, WLOG_ERROR, "invalid RDSTLS PDU type");
			return -1;
	}

	return pduLength;
}
