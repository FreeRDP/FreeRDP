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

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/settings.h>

#include <winpr/assert.h>
#include <winpr/stream.h>
#include <winpr/wlog.h>

#include "rdstls.h"
#include "transport.h"
#include "utils.h"

#define RDSTLS_VERSION_1 0x01

#define RDSTLS_TYPE_CAPABILITIES 0x01
#define RDSTLS_TYPE_AUTHREQ 0x02
#define RDSTLS_TYPE_AUTHRSP 0x04

#define RDSTLS_DATA_CAPABILITIES 0x01
#define RDSTLS_DATA_PASSWORD_CREDS 0x01
#define RDSTLS_DATA_AUTORECONNECT_COOKIE 0x02
#define RDSTLS_DATA_RESULT_CODE 0x01

typedef enum
{
	RDSTLS_STATE_INITIAL,
	RDSTLS_STATE_CAPABILITIES,
	RDSTLS_STATE_AUTH_REQ,
	RDSTLS_STATE_AUTH_RSP,
	RDSTLS_STATE_FINAL,
} RDSTLS_STATE;

struct rdp_rdstls
{
	BOOL server;
	RDSTLS_STATE state;
	rdpContext* context;
	rdpTransport* transport;

	UINT32 resultCode;
	wLog* log;
};

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
		return NULL;
	rdstls->log = WLog_Get(FREERDP_TAG("core.rdstls"));
	rdstls->context = context;
	rdstls->transport = transport;
	rdstls->server = settings->ServerMode;

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

static RDSTLS_STATE rdstls_get_state(rdpRdstls* rdstls)
{
	WINPR_ASSERT(rdstls);
	return rdstls->state;
}

static BOOL check_transition(wLog* log, RDSTLS_STATE current, RDSTLS_STATE expected,
                             RDSTLS_STATE requested)
{
	if (requested != expected)
	{
		WLog_Print(log, WLOG_ERROR,
		           "Unexpected rdstls state transition from %s [%d] to %s [%d], expected %s [%d]",
		           rdstls_get_state_str(current), current, rdstls_get_state_str(requested),
		           requested, rdstls_get_state_str(expected), expected);
		return FALSE;
	}
	return TRUE;
}

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
			           "Invalid rdstls state %s [%d], requested transition to %s [%d]",
			           rdstls_get_state_str(rdstls->state), rdstls->state,
			           rdstls_get_state_str(state), state);
			break;
	}
	if (rc)
		rdstls->state = state;

	return rc;
}

static BOOL rdstls_write_capabilities(rdpRdstls* rdstls, wStream* s)
{
	if (!Stream_EnsureRemainingCapacity(s, 6))
		return FALSE;

	Stream_Write_UINT16(s, RDSTLS_TYPE_CAPABILITIES);
	Stream_Write_UINT16(s, RDSTLS_DATA_CAPABILITIES);
	Stream_Write_UINT16(s, RDSTLS_VERSION_1);

	return TRUE;
}

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

	const size_t length = (strlen(str) + 1);

	Stream_Write_UINT16(s, (UINT16)length * sizeof(WCHAR));

	if (!Stream_EnsureRemainingCapacity(s, length * sizeof(WCHAR)))
		return -1;

	if (Stream_Write_UTF16_String_From_UTF8(s, length, str, length, TRUE) < 0)
		return -1;

	return (SSIZE_T)(Stream_GetPosition(s) - pos);
}

static BOOL rdstls_write_data(wStream* s, UINT32 length, const BYTE* data)
{
	if (!data)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 2))
		return FALSE;

	Stream_Write_UINT16(s, length);

	if (!Stream_EnsureRemainingCapacity(s, length))
		return FALSE;

	Stream_Write(s, data, length);

	return TRUE;
}

static BOOL rdstls_write_authentication_request_with_password(rdpRdstls* rdstls, wStream* s)
{
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

static BOOL rdstls_write_authentication_request_with_cookie(rdpRdstls* rdstls, wStream* s)
{
	// TODO
	return FALSE;
}

static BOOL rdstls_write_authentication_response(rdpRdstls* rdstls, wStream* s)
{
	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, RDSTLS_TYPE_AUTHRSP);
	Stream_Write_UINT16(s, RDSTLS_DATA_RESULT_CODE);
	Stream_Write_UINT32(s, rdstls->resultCode);

	return TRUE;
}

static BOOL rdstls_process_capabilities(rdpRdstls* rdstls, wStream* s)
{
	UINT16 dataType;
	UINT16 supportedVersions;

	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, dataType);
	if (dataType != RDSTLS_DATA_CAPABILITIES)
	{
		WLog_Print(rdstls->log, WLOG_ERROR,
		           "received invalid DataType=0x%04" PRIX16 ", expected 0x%04" PRIX16, dataType,
		           RDSTLS_DATA_CAPABILITIES);
		return FALSE;
	}

	Stream_Read_UINT16(s, supportedVersions);
	if ((supportedVersions & RDSTLS_VERSION_1) == 0)
	{
		WLog_Print(rdstls->log, WLOG_ERROR,
		           "received invalid supportedVersions=0x%04" PRIX16 ", expected 0x%04" PRIX16,
		           supportedVersions, RDSTLS_VERSION_1);
		return FALSE;
	}

	return TRUE;
}

static BOOL rdstls_read_unicode_string(wLog* log, wStream* s, char** str)
{
	UINT16 length = 0;

	WINPR_ASSERT(str);

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, length);

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, length))
		return FALSE;

	if (length <= 2)
	{
		Stream_Seek(s, length);
		return TRUE;
	}

	*str = Stream_Read_UTF16_String_As_UTF8(s, length / sizeof(WCHAR), NULL);
	if (!*str)
		return FALSE;

	return TRUE;
}

static BOOL rdstls_read_data(wLog* log, wStream* s, UINT16* pLength, const BYTE** pData)
{
	UINT16 length = 0;

	WINPR_ASSERT(pLength);
	WINPR_ASSERT(pData);

	*pData = NULL;
	*pLength = 0;
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, length);

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, length))
		return FALSE;

	if (length <= 2)
	{
		Stream_Seek(s, length);
		return TRUE;
	}

	*pData = Stream_Pointer(s);
	*pLength = length;
	return Stream_SafeSeek(s, length);
}

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

		if (strcmp(serverStr, clientStr) != 0)
		{
			WLog_Print(log, WLOG_ERROR, "%s verification failed", field);
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL rdstls_process_authentication_request_with_password(rdpRdstls* rdstls, wStream* s)
{
	BOOL rc = FALSE;

	const BYTE* clientRedirectionGuid = NULL;
	UINT16 clientRedirectionGuidLength = 0;
	char* clientPassword = NULL;
	char* clientUsername = NULL;
	char* clientDomain = NULL;

	const BYTE* serverRedirectionGuid = NULL;
	UINT16 serverRedirectionGuidLength = 0;
	const char* serverPassword = NULL;
	const char* serverUsername = NULL;
	const char* serverDomain = NULL;

	rdpSettings* settings = rdstls->context->settings;
	WINPR_ASSERT(settings);

	if (!rdstls_read_data(rdstls->log, s, &clientRedirectionGuidLength, &clientRedirectionGuid))
		goto fail;

	if (!rdstls_read_unicode_string(rdstls->log, s, &clientUsername))
		goto fail;

	if (!rdstls_read_unicode_string(rdstls->log, s, &clientDomain))
		goto fail;

	if (!rdstls_read_unicode_string(rdstls->log, s, &clientPassword))
		goto fail;

	serverRedirectionGuid = freerdp_settings_get_pointer(settings, FreeRDP_RedirectionGuid);
	serverRedirectionGuidLength =
	    freerdp_settings_get_uint32(settings, FreeRDP_RedirectionGuidLength);
	serverUsername = freerdp_settings_get_string(settings, FreeRDP_Username);
	serverDomain = freerdp_settings_get_string(settings, FreeRDP_Domain);
	serverPassword = freerdp_settings_get_pointer(settings, FreeRDP_Password);

	rdstls->resultCode = ERROR_SUCCESS;

	if (!rdstls_cmp_data(rdstls->log, "RedirectionGuid", serverRedirectionGuid,
	                     serverRedirectionGuidLength, clientRedirectionGuid,
	                     clientRedirectionGuidLength))
		rdstls->resultCode = ERROR_LOGON_FAILURE;

	if (!rdstls_cmp_str(rdstls->log, "UserName", serverUsername, clientUsername))
		rdstls->resultCode = ERROR_LOGON_FAILURE;

	if (!rdstls_cmp_str(rdstls->log, "Domain", serverDomain, clientDomain))
		rdstls->resultCode = ERROR_LOGON_FAILURE;

	if (!rdstls_cmp_str(rdstls->log, "Password", serverPassword, clientPassword))
		rdstls->resultCode = ERROR_LOGON_FAILURE;

	rc = TRUE;
fail:
	return rc;
}

static BOOL rdstls_process_authentication_request_with_cookie(rdpRdstls* rdstls, wStream* s)
{
	// TODO
	return FALSE;
}

static BOOL rdstls_process_authentication_request(rdpRdstls* rdstls, wStream* s)
{
	UINT16 dataType;

	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, dataType);
	switch (dataType)
	{
		case RDSTLS_DATA_PASSWORD_CREDS:
			if (!rdstls_process_authentication_request_with_password(rdstls, s))
				return FALSE;
			break;
		case RDSTLS_DATA_AUTORECONNECT_COOKIE:
			if (!rdstls_process_authentication_request_with_cookie(rdstls, s))
				return FALSE;
			break;
		default:
			WLog_Print(rdstls->log, WLOG_ERROR,
			           "received invalid DataType=0x%04" PRIX16 ", expected 0x%04" PRIX16
			           " or 0x%04" PRIX16,
			           dataType, RDSTLS_DATA_PASSWORD_CREDS, RDSTLS_DATA_AUTORECONNECT_COOKIE);
			return FALSE;
	}

	return TRUE;
}

static BOOL rdstls_process_authentication_response(rdpRdstls* rdstls, wStream* s)
{
	UINT16 dataType;
	UINT32 resultCode;

	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, 6))
		return FALSE;

	Stream_Read_UINT16(s, dataType);
	if (dataType != RDSTLS_DATA_RESULT_CODE)
	{
		WLog_Print(rdstls->log, WLOG_ERROR,
		           "received invalid DataType=0x%04" PRIX16 ", expected 0x%04" PRIX16, dataType,
		           RDSTLS_DATA_RESULT_CODE);
		return FALSE;
	}

	Stream_Read_UINT32(s, resultCode);
	if (resultCode != ERROR_SUCCESS)
	{
		WLog_Print(rdstls->log, WLOG_ERROR, "resultCode: %s [0x%08" PRIX32 "] %s",
		           freerdp_get_last_error_name(resultCode), resultCode,
		           freerdp_get_last_error_string(resultCode));
		return FALSE;
	}

	return TRUE;
}

static BOOL rdstls_send(rdpTransport* transport, wStream* s, void* extra)
{
	rdpRdstls* rdstls = (rdpRdstls*)extra;
	rdpSettings* settings = NULL;

	WINPR_ASSERT(transport);
	WINPR_ASSERT(s);
	WINPR_ASSERT(rdstls);

	settings = rdstls->context->settings;
	WINPR_ASSERT(settings);

	if (!Stream_EnsureRemainingCapacity(s, 2))
		return FALSE;

	Stream_Write_UINT16(s, RDSTLS_VERSION_1);

	const RDSTLS_STATE state = rdstls_get_state(rdstls);
	switch (state)
	{
		case RDSTLS_STATE_CAPABILITIES:
			if (!rdstls_write_capabilities(rdstls, s))
				return FALSE;
			break;
		case RDSTLS_STATE_AUTH_REQ:
			if (settings->RedirectionFlags & LB_PASSWORD_IS_PK_ENCRYPTED)
			{
				if (!rdstls_write_authentication_request_with_password(rdstls, s))
					return FALSE;
			}
			else if (settings->ServerAutoReconnectCookie != NULL)
			{
				if (!rdstls_write_authentication_request_with_cookie(rdstls, s))
					return FALSE;
			}
			else
			{
				WLog_Print(rdstls->log, WLOG_ERROR,
				           "cannot authenticate with password or auto-reconnect cookie");
				return FALSE;
			}
			break;
		case RDSTLS_STATE_AUTH_RSP:
			if (!rdstls_write_authentication_response(rdstls, s))
				return FALSE;
			break;
		default:
			WLog_Print(rdstls->log, WLOG_ERROR, "Invalid rdstls state %s [%d]",
			           rdstls_get_state_str(state), state);
			return FALSE;
	}

	if (transport_write(rdstls->transport, s) < 0)
		return FALSE;

	return TRUE;
}

static int rdstls_recv(rdpTransport* transport, wStream* s, void* extra)
{
	UINT16 version;
	UINT16 pduType;
	rdpRdstls* rdstls = (rdpRdstls*)extra;

	WINPR_ASSERT(transport);
	WINPR_ASSERT(s);
	WINPR_ASSERT(rdstls);

	if (!Stream_CheckAndLogRequiredLengthWLog(rdstls->log, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, version);
	if (version != RDSTLS_VERSION_1)
	{
		WLog_Print(rdstls->log, WLOG_ERROR,
		           "received invalid RDSTLS Version=0x%04" PRIX16 ", expected 0x%04" PRIX16,
		           version, RDSTLS_VERSION_1);
		return -1;
	}

	Stream_Read_UINT16(s, pduType);
	switch (pduType)
	{
		case RDSTLS_TYPE_CAPABILITIES:
			if (!rdstls_process_capabilities(rdstls, s))
				return -1;
			break;
		case RDSTLS_TYPE_AUTHREQ:
			if (!rdstls_process_authentication_request(rdstls, s))
				return -1;
			break;
		case RDSTLS_TYPE_AUTHRSP:
			if (!rdstls_process_authentication_response(rdstls, s))
				return -1;
			break;
		default:
			WLog_Print(rdstls->log, WLOG_ERROR, "unknown RDSTLS PDU type [0x%04" PRIx16 "]",
			           pduType);
			return -1;
	}

	return 1;
}

#define rdstls_check_state_requirements(rdstls, expected) \
	rdstls_check_state_requirements_((rdstls), (expected), __FILE__, __FUNCTION__, __LINE__)
static BOOL rdstls_check_state_requirements_(rdpRdstls* rdstls, RDSTLS_STATE expected,
                                             const char* file, const char* fkt, size_t line)
{
	const RDSTLS_STATE current = rdstls_get_state(rdstls);
	if (current == expected)
		return TRUE;

	const DWORD log_level = WLOG_ERROR;
	if (WLog_IsLevelActive(rdstls->log, log_level))
		WLog_PrintMessage(rdstls->log, WLOG_MESSAGE_TEXT, log_level, line, file, fkt,
		                  "Unexpected rdstls state %s [%d], expected %s [%d]",
		                  rdstls_get_state_str(current), current, rdstls_get_state_str(expected),
		                  expected);

	return FALSE;
}

static BOOL rdstls_send_capabilities(rdpRdstls* rdstls)
{
	BOOL rc = FALSE;
	wStream* s = NULL;

	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_CAPABILITIES))
		goto fail;

	s = Stream_New(NULL, 512);
	if (!s)
		goto fail;

	if (!rdstls_send(rdstls->transport, s, rdstls))
		goto fail;

	rc = rdstls_set_state(rdstls, RDSTLS_STATE_AUTH_REQ);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL rdstls_recv_authentication_request(rdpRdstls* rdstls)
{
	BOOL rc = FALSE;
	int status;
	wStream* s = NULL;

	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_REQ))
		goto fail;

	s = Stream_New(NULL, 4096);
	if (!s)
		goto fail;

	status = transport_read_pdu(rdstls->transport, s);

	if (status < 0)
		goto fail;

	status = rdstls_recv(rdstls->transport, s, rdstls);

	if (status < 0)
		goto fail;

	rc = rdstls_set_state(rdstls, RDSTLS_STATE_AUTH_RSP);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL rdstls_send_authentication_response(rdpRdstls* rdstls)
{
	BOOL rc = FALSE;
	wStream* s = NULL;

	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_RSP))
		goto fail;

	s = Stream_New(NULL, 512);
	if (!s)
		goto fail;

	if (!rdstls_send(rdstls->transport, s, rdstls))
		goto fail;

	rc = rdstls_set_state(rdstls, RDSTLS_STATE_FINAL);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL rdstls_recv_capabilities(rdpRdstls* rdstls)
{
	BOOL rc = FALSE;
	int status;
	wStream* s = NULL;

	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_CAPABILITIES))
		goto fail;

	s = Stream_New(NULL, 512);
	if (!s)
		goto fail;

	status = transport_read_pdu(rdstls->transport, s);

	if (status < 0)
		goto fail;

	status = rdstls_recv(rdstls->transport, s, rdstls);

	if (status < 0)
		goto fail;

	rc = rdstls_set_state(rdstls, RDSTLS_STATE_AUTH_REQ);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL rdstls_send_authentication_request(rdpRdstls* rdstls)
{
	BOOL rc = FALSE;
	wStream* s = NULL;

	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_REQ))
		goto fail;

	s = Stream_New(NULL, 4096);
	if (!s)
		goto fail;

	if (!rdstls_send(rdstls->transport, s, rdstls))
		goto fail;

	rc = rdstls_set_state(rdstls, RDSTLS_STATE_AUTH_RSP);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL rdstls_recv_authentication_response(rdpRdstls* rdstls)
{
	BOOL rc = FALSE;
	int status;
	wStream* s = NULL;

	WINPR_ASSERT(rdstls);

	if (!rdstls_check_state_requirements(rdstls, RDSTLS_STATE_AUTH_RSP))
		goto fail;

	s = Stream_New(NULL, 512);
	if (!s)
		goto fail;

	status = transport_read_pdu(rdstls->transport, s);

	if (status < 0)
		goto fail;

	status = rdstls_recv(rdstls->transport, s, rdstls);

	if (status < 0)
		goto fail;

	rc = rdstls_set_state(rdstls, RDSTLS_STATE_FINAL);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

static int rdstls_server_authenticate(rdpRdstls* rdstls)
{
	if (!rdstls_set_state(rdstls, RDSTLS_STATE_CAPABILITIES))
		return -1;

	if (!rdstls_send_capabilities(rdstls))
		return -1;

	if (!rdstls_recv_authentication_request(rdstls))
		return -1;

	if (!rdstls_send_authentication_response(rdstls))
		return -1;

	if (rdstls->resultCode != 0)
		return -1;

	return 1;
}

static int rdstls_client_authenticate(rdpRdstls* rdstls)
{
	if (!rdstls_set_state(rdstls, RDSTLS_STATE_CAPABILITIES))
		return -1;

	if (!rdstls_recv_capabilities(rdstls))
		return -1;

	if (!rdstls_send_authentication_request(rdstls))
		return -1;

	if (!rdstls_recv_authentication_response(rdstls))
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

static SSIZE_T rdstls_parse_pdu_data_type(wLog* log, UINT16 dataType, wStream* s)
{
	switch (dataType)
	{
		case RDSTLS_DATA_PASSWORD_CREDS:
		{
			UINT16 redirGuidLength;
			if (Stream_GetRemainingLength(s) < 2)
				return 0;
			Stream_Read_UINT16(s, redirGuidLength);

			if (!Stream_SafeSeek(s, redirGuidLength))
				return 0;

			UINT16 usernameLength;
			if (Stream_GetRemainingLength(s) < 2)
				return 0;
			Stream_Read_UINT16(s, usernameLength);

			if (!Stream_SafeSeek(s, usernameLength))
				return 0;

			UINT16 domainLength;
			if (Stream_GetRemainingLength(s) < 2)
				return 0;
			Stream_Read_UINT16(s, domainLength);

			if (!Stream_SafeSeek(s, domainLength))
				return 0;

			UINT16 passwordLength;
			if (Stream_GetRemainingLength(s) < 2)
				return 0;
			Stream_Read_UINT16(s, passwordLength);

			return Stream_GetPosition(s) + passwordLength;
		}
		case RDSTLS_DATA_AUTORECONNECT_COOKIE:
		{
			if (!Stream_SafeSeek(s, 4))
				return 0;

			UINT16 cookieLength;
			if (Stream_GetRemainingLength(s) < 2)
				return 0;
			Stream_Read_UINT16(s, cookieLength);

			return 12u + cookieLength;
		}
		default:
			WLog_Print(log, WLOG_ERROR, "invalid RDSLTS dataType");
			return -1;
	}
}

SSIZE_T rdstls_parse_pdu(wLog* log, wStream* stream)
{
	SSIZE_T pduLength = -1;
	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticConstInit(&sbuffer, Stream_Buffer(stream), Stream_Length(stream));

	UINT16 version;
	if (Stream_GetRemainingLength(s) < 2)
		return 0;
	Stream_Read_UINT16(s, version);
	if (version != RDSTLS_VERSION_1)
	{
		WLog_Print(log, WLOG_ERROR, "invalid RDSTLS version");
		return -1;
	}

	UINT16 pduType;
	if (Stream_GetRemainingLength(s) < 2)
		return 0;
	Stream_Read_UINT16(s, pduType);
	switch (pduType)
	{
		case RDSTLS_TYPE_CAPABILITIES:
			pduLength = 8;
			break;
		case RDSTLS_TYPE_AUTHREQ:
			if (Stream_GetRemainingLength(s) < 2)
				return 0;
			UINT16 dataType;
			Stream_Read_UINT16(s, dataType);
			pduLength = rdstls_parse_pdu_data_type(log, dataType, s);

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
