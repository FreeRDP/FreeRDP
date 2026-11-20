/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote session shadowing ([MS-TSTS] RpcShadow2)
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

/*
 * Shadowing a Windows session the way mstsc /shadow does it:
 *
 * 1. Call RpcShadow2 (interface SessEnvPublicRpc, [MS-TSTS] 3.10.4.1.1) over the named pipe
 *    \pipe\SessEnvPublicRpc. The server asks the session user for consent if the policy
 *    requires it and returns an invitation, an [MS-RAI] 2.2.2 connection string 2 naming a
 *    listener (address and dynamic port) in the shadowed session.
 * 2. Connect to that listener in remote assistance mode with an empty remote assistance
 *    password; the listener authenticates the caller with NLA, so the same credentials are
 *    used. A non-empty AlternateShell makes the server drop the connection.
 */

#include <freerdp/config.h>

#include <ctype.h>
#include <string.h>

#include <winpr/assert.h>
#include <winpr/string.h>
#include <winpr/crypto.h>

#include <freerdp/log.h>
#include <freerdp/settings.h>
#include <freerdp/assistance.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/crypto/certificate.h>

#include "rdp.h"
#include "utils.h"
#include "settings.h"
#include "remote_shadow.h"

#define TAG FREERDP_TAG("core.shadow")

#define SHADOW2_MAX_INVITATION 8192

BOOL remote_shadow_write_shadow2_stub(wStream* s, UINT32 sessionId, BOOL control,
                                      BOOL noConsentPrompt)
{
	WINPR_ASSERT(s);
	if (!Stream_EnsureRemainingCapacity(s, 12))
		return FALSE;

	/* NDR enums are 16 bit on the wire */
	Stream_Write_UINT32(s, sessionId);
	Stream_Write_UINT16(s, control ? 1 : 0);         /* SHADOW_CONTROL_REQUEST_TAKECONTROL */
	Stream_Write_UINT16(s, noConsentPrompt ? 0 : 1); /* SHADOW_PERMISSION_REQUEST_SILENT */
	Stream_Write_UINT32(s, SHADOW2_MAX_INVITATION);  /* cchInvitation */
	return TRUE;
}

BOOL remote_shadow_read_shadow2_stub(wLog* log, wStream* s, UINT16* permission, UINT32* hresult,
                                     char** invitation)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(permission);
	WINPR_ASSERT(hresult);
	WINPR_ASSERT(invitation);

	*invitation = nullptr;

	/* SHADOW_REQUEST_RESPONSE, then the conformant varying string pszInvitation */
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 16))
		return FALSE;
	*permission = Stream_Get_UINT16(s);
	Stream_Seek_UINT16(s); /* alignment */
	const UINT32 maxCount = Stream_Get_UINT32(s);
	const UINT32 offset = Stream_Get_UINT32(s);
	const UINT32 actualCount = Stream_Get_UINT32(s);

	if ((offset != 0) || (actualCount > maxCount) || (maxCount > SHADOW2_MAX_INVITATION))
	{
		WLog_Print(log, WLOG_ERROR,
		           "invalid RpcShadow2 invitation: max %" PRIu32 ", offset %" PRIu32
		           ", actual %" PRIu32,
		           maxCount, offset, actualCount);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(log, s, actualCount, sizeof(WCHAR)))
		return FALSE;

	if (actualCount > 0)
	{
		const WCHAR* str = Stream_ConstPointer(s);
		size_t len = 0;
		*invitation = ConvertWCharNToUtf8Alloc(str, actualCount, &len);
		if (!*invitation)
			return FALSE;
	}
	Stream_Seek(s, 2ull * actualCount);

	/* the HRESULT is 4 byte aligned relative to the start of the stub */
	const size_t pad = (4 - (Stream_GetPosition(s) % 4)) % 4;
	if (!Stream_SafeSeek(s, pad) || !Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
	{
		free(*invitation);
		*invitation = nullptr;
		return FALSE;
	}
	*hresult = Stream_Get_UINT32(s);
	return TRUE;
}

#if defined(WITH_LIBSMB2)
static const char* remote_shadow_response_string(UINT16 permission)
{
	switch (permission)
	{
		case REMOTE_SHADOW_RESPONSE_ALLOW:
			return "allowed";
		case REMOTE_SHADOW_RESPONSE_DECLINE:
			return "the session user declined the request";
		case REMOTE_SHADOW_RESPONSE_POLICY_PERMISSION_REQUIRED:
			return "the shadow policy requires the session user's consent";
		case REMOTE_SHADOW_RESPONSE_POLICY_DISABLED:
			return "shadowing is disabled by policy";
		case REMOTE_SHADOW_RESPONSE_POLICY_VIEW_ONLY:
			return "the shadow policy allows view only";
		case REMOTE_SHADOW_RESPONSE_POLICY_VIEW_ONLY_PERMISSION_REQUIRED:
			return "the shadow policy allows view only, with the session user's consent";
		case REMOTE_SHADOW_RESPONSE_SESSION_ALREADY_CONTROLLED:
			return "the session is already being shadowed";
		default:
			return "unknown response";
	}
}

/** Drops IPv6 link-local entries: the invitation carries the server's own interface scope ids. */
static BOOL remote_shadow_drop_link_local(rdpSettings* settings)
{
	const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
	char** addresses = (char**)calloc(count + 1, sizeof(char*));
	UINT32* ports = calloc(count + 1, sizeof(UINT32));
	UINT32 kept = 0;
	BOOL rc = FALSE;

	if (!addresses || !ports)
		goto out;

	for (UINT32 x = 0; x < count; x++)
	{
		const char* addr =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_TargetNetAddresses, x);
		const UINT32* port =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_TargetNetPorts, x);
		if (!addr || !port)
			continue;
		/* fe80::/10: the first group is fe80 to febf */
		if ((_strnicmp(addr, "fe", 2) == 0) && addr[2] && strchr("89abAB", addr[2]) && addr[3] &&
		    (addr[4] == ':'))
		{
			WLog_DBG(TAG, "skipping link-local listener address %s", addr);
			continue;
		}
		addresses[kept] = _strdup(addr);
		if (!addresses[kept])
			goto out;
		ports[kept++] = *port;
	}

	if (kept == 0)
	{
		WLog_ERR(TAG, "the shadow invitation names no reachable listener address");
		goto out;
	}

	if (!freerdp_target_net_adresses_reset(settings, kept))
		goto out;
	for (UINT32 x = 0; x < kept; x++)
	{
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_TargetNetAddresses, x,
		                                        addresses[x]) ||
		    !freerdp_settings_set_pointer_array(settings, FreeRDP_TargetNetPorts, x, &ports[x]))
			goto out;
	}
	rc = freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, ports[0]);

out:
	if (addresses)
	{
		for (UINT32 x = 0; x < count; x++)
			free(addresses[x]);
	}
	free((void*)addresses);
	free(ports);
	return rc;
}

/**
 * The invitation carries the listener's certificate (attribute CE of element A, base64 DER).
 * It arrived in the RpcShadow2 response, sealed with the NTLM session key (PKT_PRIVACY) that
 * only a server knowing the account's password can derive, so accept exactly that certificate.
 */
static BOOL remote_shadow_pin_certificate(rdpSettings* settings, const char* invitation)
{
	BOOL rc = FALSE;
	char* der64 = nullptr;
	BYTE* der = nullptr;
	size_t derLength = 0;
	rdpCertificate* cert = nullptr;
	char* fp = nullptr;
	char* list = nullptr;

	/* only the A element: another element's CE attribute is not the listener certificate */
	const char* a = strstr(invitation, "<A ");
	const char* tagEnd = a ? strchr(a, '>') : nullptr;
	const char* ce = a ? strstr(a, " CE=\"") : nullptr;
	const char* end = ce ? strchr(ce + 5, '"') : nullptr;
	if (!ce || !end || !tagEnd || (end > tagEnd))
	{
		WLog_WARN(TAG,
		          "the shadow invitation carries no certificate, it will be verified as usual");
		return TRUE;
	}

	/* the base64 text is wrapped with XML character references (&#xA;) and white space */
	der64 = calloc((size_t)(end - (ce + 5)) + 1, sizeof(char));
	if (!der64)
		goto out;
	char* dst = der64;
	for (const char* cur = ce + 5; cur < end; cur++)
	{
		if (*cur == '&')
		{
			const char* semicolon = memchr(cur, ';', (size_t)(end - cur));
			if (!semicolon)
				break;
			cur = semicolon;
		}
		else if (!isspace((unsigned char)*cur))
			*dst++ = *cur;
	}
	crypto_base64_decode(der64, strlen(der64), &der, &derLength);
	if (!der || (derLength == 0))
		goto out;
	cert = freerdp_certificate_new_from_der(der, derLength);
	if (!cert)
		goto out;
	fp = freerdp_certificate_get_fingerprint_by_hash_ex(cert, "sha256", FALSE);
	if (!fp)
		goto out;

	const char* old =
	    freerdp_settings_get_string(settings, FreeRDP_CertificateAcceptedFingerprints);
	if (old && strstr(old, fp))
	{
		rc = TRUE;
		goto out;
	}

	size_t size = 0;
	if (winpr_asprintf(&list, &size, "%s%ssha256:%s", old ? old : "", (old && *old) ? "," : "",
	                   fp) < 0)
		goto out;
	rc = freerdp_settings_set_string(settings, FreeRDP_CertificateAcceptedFingerprints, list);

out:
	if (!rc)
		WLog_ERR(TAG, "failed to read the certificate from the shadow invitation");
	free(list);
	free(fp);
	freerdp_certificate_free(cert);
	free(der);
	free(der64);
	return rc;
}

BOOL remote_shadow_apply_invitation(rdpSettings* settings, const char* invitation)
{
	BOOL rc = FALSE;
	rdpAssistanceFile* file = freerdp_assistance_file_new();
	if (!file)
		return FALSE;

	/* The shadow listener expects an empty remote assistance password (AlternateShell). */
	if (freerdp_assistance_set_connection_string2(file, invitation, "") < 0)
	{
		WLog_ERR(TAG, "failed to parse the shadow invitation");
		goto out;
	}

	/* populate overwrites the host name; keep the one the user gave for the next request */
	char* host = _strdup(freerdp_settings_get_string(settings, FreeRDP_ServerHostname));
	if (!host)
		goto out;
	const BOOL populated =
	    freerdp_assistance_populate_settings_from_assistance_file(file, settings);
	const BOOL restored = freerdp_settings_set_string(settings, FreeRDP_ServerHostname, host);
	free(host);
	if (!populated || !restored)
	{
		WLog_ERR(TAG, "the shadow invitation names no listener");
		goto out;
	}

	rc = remote_shadow_drop_link_local(settings) &&
	     remote_shadow_pin_certificate(settings, invitation) &&
	     freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, TRUE);

out:
	freerdp_assistance_file_free(file);
	return rc;
}
#endif

BOOL remote_shadow_prepare_connect(rdpContext* context)
{
	WINPR_ASSERT(context);
	rdpSettings* settings = context->settings;
	WINPR_ASSERT(settings);

	if (!freerdp_settings_get_bool(settings, FreeRDP_RemoteShadowMode))
		return TRUE;

#if !defined(WITH_LIBSMB2)
	WLog_ERR(TAG, "session shadowing needs FreeRDP built with -DWITH_LIBSMB2=ON");
	freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_FAILED);
	return FALSE;
#else
	switch (utils_authenticate(context->instance, AUTH_NLA, TRUE))
	{
		case AUTH_SKIP:
		case AUTH_SUCCESS:
			break;
		case AUTH_CANCELLED:
			freerdp_set_last_error_log(context, FREERDP_ERROR_CONNECT_CANCELLED);
			return FALSE;
		default:
			WLog_ERR(TAG, "session shadowing needs a user name and password");
			freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_NO_OR_MISSING_CREDENTIALS);
			return FALSE;
	}

	const UINT32 sessionId = freerdp_settings_get_uint32(settings, FreeRDP_RemoteShadowSessionId);
	const BOOL control = freerdp_settings_get_bool(settings, FreeRDP_RemoteShadowControl);
	const BOOL noConsent = freerdp_settings_get_bool(settings, FreeRDP_RemoteShadowNoConsentPrompt);
	UINT16 permission = 0;
	UINT32 hresult = 0;
	char* invitation = nullptr;

	WLog_INFO(TAG, "requesting to %s session %" PRIu32 " on %s%s", control ? "control" : "view",
	          sessionId, freerdp_settings_get_string(settings, FreeRDP_ServerHostname),
	          noConsent ? "" : ", the session user will be asked");

	/* consent can take the user a while: allow up to a minute on top of the connect timeout */
	const UINT32 connectTimeout = freerdp_settings_get_uint32(settings, FreeRDP_TcpConnectTimeout);
	const UINT32 timeout =
	    (connectTimeout > UINT32_MAX - 60000u) ? UINT32_MAX : connectTimeout + 60000u;
	if (!remote_shadow_rpc_shadow2(context, timeout, &permission, &hresult, &invitation))
	{
		/* the client gave up, for example closed while reconnecting */
		const BOOL aborted = utils_abort_event_is_set(context->rdp);
		freerdp_set_last_error_if_not(context, aborted ? FREERDP_ERROR_CONNECT_CANCELLED
		                                               : FREERDP_ERROR_CONNECT_FAILED);
		return FALSE;
	}

	BOOL rc = FALSE;
	if (hresult != 0)
		WLog_ERR(TAG, "RpcShadow2 failed with HRESULT 0x%08" PRIx32, hresult);
	else if (permission != REMOTE_SHADOW_RESPONSE_ALLOW)
		WLog_ERR(TAG, "shadowing session %" PRIu32 " refused: %s", sessionId,
		         remote_shadow_response_string(permission));
	else if (!invitation)
		WLog_ERR(TAG, "RpcShadow2 returned no invitation");
	else
		rc = remote_shadow_apply_invitation(settings, invitation);

	free(invitation);
	if (!rc)
		freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_FAILED);
	return rc;
#endif
}
