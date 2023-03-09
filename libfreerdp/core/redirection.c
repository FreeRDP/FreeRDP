/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Server Redirection
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <freerdp/log.h>
#include <freerdp/crypto/certificate.h>
#include <freerdp/redirection.h>
#include <freerdp/utils/string.h>

#include "../crypto/certificate.h"
#include "redirection.h"
#include "utils.h"

#define TAG FREERDP_TAG("core.redirection")

struct rdp_redirection
{
	UINT32 flags;
	UINT32 sessionID;
	BYTE* TsvUrl;
	UINT32 TsvUrlLength;
	char* Username;
	char* Domain;
	BYTE* Password;
	UINT32 PasswordLength;
	char* TargetFQDN;
	BYTE* LoadBalanceInfo;
	UINT32 LoadBalanceInfoLength;
	char* TargetNetBiosName;
	char* TargetNetAddress;
	UINT32 TargetNetAddressesCount;
	char** TargetNetAddresses;
	UINT32 RedirectionGuidLength;
	BYTE* RedirectionGuid;

	rdpCertificate* TargetCertificate;
};

#define ELEMENT_TYPE_CERTIFICATE 32
#define ENCODING_TYPE_ASN1_DER 1

static void redirection_free_array(char*** what, UINT32* count)
{
	WINPR_ASSERT(what);
	WINPR_ASSERT(count);

	if (*what)
	{
		for (UINT32 x = 0; x < *count; x++)
			free((*what)[x]);
		free(*what);
	}

	*what = NULL;
	*count = 0;
}

static void redirection_free_string(char** str)
{
	WINPR_ASSERT(str);
	free(*str);
	*str = NULL;
}

static void redirection_free_data(BYTE** str, UINT32* length)
{
	WINPR_ASSERT(str);
	free(*str);
	if (length)
		*length = 0;
	*str = NULL;
}

static BOOL redirection_copy_string(char** dst, const char* str)
{
	redirection_free_string(dst);
	if (!str)
		return TRUE;

	*dst = _strdup(str);
	return *dst != NULL;
}

static BOOL redirection_copy_data(BYTE** dst, UINT32* plen, const BYTE* str, size_t len)
{
	redirection_free_data(dst, plen);

	if (!str || (len == 0))
		return TRUE;
	if (len > UINT32_MAX)
		return FALSE;

	*dst = malloc(len);
	if (!*dst)
		return FALSE;
	memcpy(*dst, str, len);
	*plen = (UINT32)len;
	return *dst != NULL;
}

static BOOL redirection_copy_array(char*** dst, UINT32* plen, const char** str, size_t len)
{
	redirection_free_array(dst, plen);

	if (!str || (len == 0))
		return TRUE;

	*dst = calloc(len, sizeof(char));
	if (!*dst)
		return FALSE;
	*plen = len;

	for (UINT32 x = 0; x < len; x++)
	{
		if (str[x])
			(*dst)[x] = _strdup(str[x]);

		if (!((*dst)[x]))
		{
			redirection_free_array(dst, plen);
			return FALSE;
		}
	}

	return *dst != NULL;
}

static BOOL rdp_redirection_get_data(wStream* s, UINT32* pLength, const BYTE** pData)
{
	WINPR_ASSERT(pLength);
	WINPR_ASSERT(pData);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, *pLength);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, *pLength))
		return FALSE;

	*pData = Stream_Pointer(s);
	Stream_Seek(s, *pLength);
	return TRUE;
}

static BOOL rdp_redirection_read_unicode_string(wStream* s, char** str, size_t maxLength)
{
	UINT32 length = 0;
	const BYTE* data = NULL;

	if (!rdp_redirection_get_data(s, &length, &data))
		return FALSE;

	const WCHAR* wstr = (const WCHAR*)data;

	if ((length % 2) || length < 2 || length > maxLength)
	{
		WLog_ERR(TAG, "failure: invalid unicode string length: %" PRIu32 "", length);
		return FALSE;
	}

	if (wstr[length / 2 - 1])
	{
		WLog_ERR(TAG, "failure: unterminated unicode string");
		return FALSE;
	}

	redirection_free_string(str);
	*str = ConvertWCharNToUtf8Alloc(wstr, length / sizeof(WCHAR), NULL);
	if (!*str)
	{
		WLog_ERR(TAG, "failure: string conversion failed");
		return FALSE;
	}

	return TRUE;
}

static BOOL replace_char(char* utf8, size_t length, char what, char with)
{
	for (size_t x = 0; x < length; x++)
	{
		char* cur = &utf8[x];
		if (*cur == what)
			*cur = with;
	}
	return TRUE;
}

static BOOL rdp_redirection_write_data(wStream* s, size_t length, const void* data)
{
	WINPR_ASSERT(data || (length == 0));

	if (!Stream_CheckAndLogRequiredCapacity(TAG, s, 4))
		return FALSE;

	Stream_Write_UINT32(s, length);

	if (!Stream_CheckAndLogRequiredCapacity(TAG, s, length))
		return FALSE;

	Stream_Write(s, data, length);
	return TRUE;
}

static BOOL rdp_redirection_write_base64_wchar(UINT32 flag, wStream* s, size_t length,
                                               const void* data)
{
	BOOL rc = FALSE;

	char* base64 = crypto_base64_encode(data, length);
	if (!base64)
		return FALSE;

	size_t wbase64len = 0;
	WCHAR* wbase64 = ConvertUtf8ToWCharAlloc(base64, &wbase64len);
	free(base64);
	if (!wbase64)
		return FALSE;

	rc = rdp_redirection_write_data(s, wbase64len * sizeof(WCHAR), wbase64);
	free(wbase64);
	return rc;
}

static BOOL rdp_redirection_read_base64_wchar(UINT32 flag, wStream* s, UINT32* pLength,
                                              BYTE** pData)
{
	BOOL rc = FALSE;
	char buffer[64] = { 0 };
	const BYTE* ptr = NULL;

	if (!rdp_redirection_get_data(s, pLength, &ptr))
		return FALSE;
	const WCHAR* wchar = (const WCHAR*)ptr;

	size_t utf8_len = 0;
	char* utf8 = ConvertWCharNToUtf8Alloc(wchar, *pLength, &utf8_len);
	if (!utf8)
		return FALSE;

	redirection_free_data(pData, NULL);

	utf8_len = strnlen(utf8, utf8_len);
	*pData = calloc(utf8_len, sizeof(BYTE));
	if (!*pData)
		goto fail;

	size_t rlen = utf8_len;
	size_t wpos = 0;
	char* tok = strtok(utf8, "\r\n");
	while (tok)
	{
		const size_t len = strnlen(tok, rlen);
		rlen -= len;

		size_t bplen = 0;
		BYTE* bptr = NULL;
		crypto_base64_decode(tok, len, &bptr, &bplen);
		if (!bptr)
			goto fail;
		memcpy(&(*pData)[wpos], bptr, bplen);
		wpos += bplen;
		free(bptr);

		tok = strtok(NULL, "\r\n");
	}
	*pLength = wpos;

	WLog_DBG(TAG, "%s:", rdp_redirection_flags_to_string(flag, buffer, sizeof(buffer)));

	rc = TRUE;
fail:
	free(utf8);
	return rc;
}

static BOOL rdp_target_cert_get_element(wStream* s, UINT32* pType, UINT32* pEncoding,
                                        const BYTE** ptr, size_t* pLength)
{
	WINPR_ASSERT(pType);
	WINPR_ASSERT(pEncoding);
	WINPR_ASSERT(ptr);
	WINPR_ASSERT(pLength);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return FALSE;

	UINT32 type = 0;
	UINT32 encoding = 0;
	UINT32 elementSize = 0;

	Stream_Read_UINT32(s, type);
	Stream_Read_UINT32(s, encoding);
	Stream_Read_UINT32(s, elementSize);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, elementSize))
		return FALSE;

	*ptr = Stream_Pointer(s);
	*pLength = elementSize;
	Stream_Seek(s, elementSize);

	*pType = type;
	*pEncoding = encoding;
	return TRUE;
}

static BOOL rdp_target_cert_write_element(wStream* s, UINT32 Type, UINT32 Encoding,
                                          const BYTE* data, size_t length)
{
	WINPR_ASSERT(data || (length == 0));

	if (!Stream_CheckAndLogRequiredCapacity(TAG, s, 12))
		return FALSE;

	Stream_Write_UINT32(s, Type);
	Stream_Write_UINT32(s, Encoding);
	Stream_Write_UINT32(s, length);

	if (!Stream_CheckAndLogRequiredCapacity(TAG, s, length))
		return FALSE;

	Stream_Write(s, data, length);
	return TRUE;
}

static BOOL rdp_redirection_read_target_cert(rdpRedirection* redirection, const BYTE* data,
                                             size_t length)
{
	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticConstInit(&sbuffer, data, length);

	freerdp_certificate_free(redirection->TargetCertificate);
	redirection->TargetCertificate = NULL;

	size_t plength = 0;
	const BYTE* ptr = NULL;
	while (Stream_GetRemainingLength(s) > 0)
	{
		UINT32 type = 0;
		UINT32 encoding = 0;
		if (!rdp_target_cert_get_element(s, &type, &encoding, &ptr, &plength))
			return FALSE;

		switch (type)
		{
			case ELEMENT_TYPE_CERTIFICATE:
				if (encoding == ENCODING_TYPE_ASN1_DER)
				{
					if (redirection->TargetCertificate)
						WLog_WARN(TAG, "Duplicate TargetCertificate in data detected!");
					else
						redirection->TargetCertificate =
						    freerdp_certificate_new_from_der(ptr, plength);
				}
				break;
			default: /* ignore unknown fields */
				WLog_WARN(TAG,
				          "Unknown TargetCertificate field type %" PRIu32 ", encoding %" PRIu32
				          " of length %" PRIu32,
				          type, encoding, plength);
				break;
		}
	}

	return redirection->TargetCertificate != NULL;
}

static BOOL rdp_redirection_write_target_cert(wStream* s, const rdpRedirection* redirection)
{
	BOOL rc = FALSE;
	WINPR_ASSERT(redirection);

	const rdpCertificate* cert = redirection->TargetCertificate;
	if (!cert)
		return FALSE;

	size_t derlen = 0;

	BYTE* der = freerdp_certificate_get_der(cert, &derlen);
	if (!rdp_target_cert_write_element(s, ELEMENT_TYPE_CERTIFICATE, ENCODING_TYPE_ASN1_DER, der,
	                                   derlen))
		goto fail;

	rc = TRUE;

fail:
	free(der);
	return rc;
}

static BOOL rdp_redireciton_write_target_cert_stream(wStream* s, const rdpRedirection* redirection)
{
	BOOL rc = FALSE;
	wStream* serialized = Stream_New(NULL, 1024);
	if (!serialized)
		goto fail;

	if (!rdp_redirection_write_target_cert(serialized, redirection))
		goto fail;

	if (!rdp_redirection_write_base64_wchar(
	        LB_TARGET_CERTIFICATE, s, Stream_GetPosition(serialized), Stream_Buffer(serialized)))
		return FALSE;

fail:
	Stream_Free(serialized, TRUE);
	return rc;
}

static BOOL rdp_redirection_read_target_cert_stream(wStream* s, rdpRedirection* redirection)
{
	UINT32 length = 0;
	BYTE* ptr = NULL;

	WINPR_ASSERT(redirection);

	if (!rdp_redirection_read_base64_wchar(LB_TARGET_CERTIFICATE, s, &length, &ptr))
		return FALSE;

	const BOOL rc = rdp_redirection_read_target_cert(redirection, ptr, length);
	free(ptr);
	return rc;
}

int rdp_redirection_apply_settings(rdpRdp* rdp)
{
	rdpSettings* settings = NULL;
	rdpRedirection* redirection = NULL;

	if (!rdp_reset_runtime_settings(rdp))
		return -1;

	settings = rdp->settings;
	WINPR_ASSERT(settings);

	redirection = rdp->redirection;
	WINPR_ASSERT(redirection);

	settings->RedirectionFlags = redirection->flags;
	settings->RedirectedSessionId = redirection->sessionID;

	if (settings->RedirectionFlags & LB_TARGET_NET_ADDRESS)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_TargetNetAddress,
		                                 redirection->TargetNetAddress))
			return -1;
	}

	if (settings->RedirectionFlags & LB_USERNAME)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RedirectionUsername,
		                                 redirection->Username))
			return -1;
	}

	if (settings->RedirectionFlags & LB_DOMAIN)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RedirectionDomain, redirection->Domain))
			return -1;
	}

	if (settings->RedirectionFlags & LB_PASSWORD)
	{
		/* Password may be a cookie without a null terminator */
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RedirectionPassword,
		                                      redirection->Password, redirection->PasswordLength))
			return -1;
	}

	if (settings->RedirectionFlags & LB_DONTSTOREUSERNAME)
	{
		// TODO
	}

	if (settings->RedirectionFlags & LB_SMARTCARD_LOGON)
	{
		// TODO
	}

	if (settings->RedirectionFlags & LB_NOREDIRECT)
	{
		// TODO
	}

	if (settings->RedirectionFlags & LB_TARGET_FQDN)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RedirectionTargetFQDN,
		                                 redirection->TargetFQDN))
			return -1;
	}

	if (settings->RedirectionFlags & LB_TARGET_NETBIOS_NAME)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RedirectionTargetNetBiosName,
		                                 redirection->TargetNetBiosName))
			return -1;
	}

	if (settings->RedirectionFlags & LB_TARGET_NET_ADDRESSES)
	{
		if (!freerdp_target_net_addresses_copy(settings, redirection->TargetNetAddresses,
		                                       redirection->TargetNetAddressesCount))
			return -1;
	}

	if (settings->RedirectionFlags & LB_CLIENT_TSV_URL)
	{
		/* TsvUrl may not contain a null terminator */
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RedirectionTsvUrl,
		                                      redirection->TsvUrl, redirection->TsvUrlLength))
			return -1;

		const size_t lblen = freerdp_settings_get_uint32(settings, FreeRDP_LoadBalanceInfoLength);
		const char* lb = freerdp_settings_get_pointer(settings, FreeRDP_LoadBalanceInfo);
		if (lblen > 0)
		{
			BOOL valid = TRUE;
			size_t tsvlen = 0;

			char* tsv =
			    ConvertWCharNToUtf8Alloc((const WCHAR*)redirection->TsvUrl,
			                             redirection->TsvUrlLength / sizeof(WCHAR), &tsvlen);
			if (!tsv || !lb)
				valid = FALSE;
			else if (tsvlen != lblen)
				valid = FALSE;
			else if (memcmp(tsv, lb, lblen) != 0)
				valid = FALSE;

			if (!valid)
			{
				WLog_ERR(TAG,
				         "[redirection] Expected TsvUrl '%s' [%" PRIuz "], but got '%s' [%" PRIuz
				         "]",
				         lb, lblen, tsv, tsvlen);
			}
			free(tsv);

			if (!valid)
				return -2;
		}
	}

	if (settings->RedirectionFlags & LB_SERVER_TSV_CAPABLE)
	{
		// TODO
	}

	if (settings->RedirectionFlags & LB_LOAD_BALANCE_INFO)
	{
		/* LoadBalanceInfo may not contain a null terminator */
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_LoadBalanceInfo,
		                                      redirection->LoadBalanceInfo,
		                                      redirection->LoadBalanceInfoLength))
			return -1;
	}
	else
	{
		/**
		 * Free previous LoadBalanceInfo, if any, otherwise it may end up
		 * being reused for the redirected session, which is not what we want.
		 */
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_LoadBalanceInfo, NULL, 0))
			return -1;
	}

	if (settings->RedirectionFlags & LB_PASSWORD_IS_PK_ENCRYPTED)
	{
		// TODO
	}

	if (settings->RedirectionFlags & LB_REDIRECTION_GUID)
	{
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RedirectionGuid,
		                                      redirection->RedirectionGuid,
		                                      redirection->RedirectionGuidLength))
			return -1;
	}

	if (settings->RedirectionFlags & LB_TARGET_CERTIFICATE)
	{
		rdpCertificate* cert = freerdp_certificate_clone(redirection->TargetCertificate);
		if (!freerdp_settings_set_pointer(settings, FreeRDP_RedirectionTargetCertificate, cert))
			return -1;

		BOOL pres = FALSE;
		size_t length = 0;
		char* pem = freerdp_certificate_get_pem(cert, &length);
		if (pem)
		{
			pres = freerdp_settings_set_string_len(settings, FreeRDP_RedirectionAcceptedCert, pem,
			                                       length);
			if (pres)
				pres = freerdp_settings_set_uint32(settings, FreeRDP_RedirectionAcceptedCertLength,
				                                   length);
		}
		free(pem);
		if (!pres)
			return -1;
	}

	return 0;
}

static BOOL rdp_redirection_read_data(UINT32 flag, wStream* s, UINT32* pLength, BYTE** pData)
{
	char buffer[64] = { 0 };
	const BYTE* ptr = NULL;

	if (!rdp_redirection_get_data(s, pLength, &ptr))
		return FALSE;

	redirection_free_data(pData, NULL);
	*pData = (BYTE*)malloc(*pLength);

	if (!*pData)
		return FALSE;
	memcpy(*pData, ptr, *pLength);

	WLog_DBG(TAG, "%s:", rdp_redirection_flags_to_string(flag, buffer, sizeof(buffer)));

	return TRUE;
}

static state_run_t rdp_recv_server_redirection_pdu(rdpRdp* rdp, wStream* s)
{
	char buffer[256] = { 0 };
	UINT16 flags = 0;
	UINT16 length = 0;
	rdpRedirection* redirection = rdp->redirection;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return STATE_RUN_FAILED;

	Stream_Read_UINT16(s, flags); /* flags (2 bytes) */
	if (flags != SEC_REDIRECTION_PKT)
	{
		char buffer1[1024] = { 0 };
		char buffer2[1024] = { 0 };
		WLog_ERR(TAG, "received invalid flags=%s, expected %s",
		         rdp_security_flag_string(flags, buffer1, sizeof(buffer1)),
		         rdp_security_flag_string(SEC_REDIRECTION_PKT, buffer2, sizeof(buffer2)));
		return STATE_RUN_FAILED;
	}
	Stream_Read_UINT16(s, length);                 /* length (2 bytes) */
	Stream_Read_UINT32(s, redirection->sessionID); /* sessionID (4 bytes) */
	Stream_Read_UINT32(s, redirection->flags);     /* redirFlags (4 bytes) */
	WLog_INFO(TAG,
	          "flags: 0x%04" PRIX16 ", length: %" PRIu16 ", sessionID: 0x%08" PRIX32
	          ", redirFlags: %s [0x%08" PRIX32 "]",
	          flags, length, redirection->sessionID,
	          rdp_redirection_flags_to_string(redirection->flags, buffer, sizeof(buffer)),
	          redirection->flags);

	/* Although MS-RDPBCGR does not mention any length constraints limits for the
	 * variable length null-terminated unicode strings in the RDP_SERVER_REDIRECTION_PACKET
	 * structure we will use the following limits in bytes including the null terminator:
	 *
	 * TargetNetAddress:     80 bytes
	 * UserName:            512 bytes
	 * Domain:               52 bytes
	 * Password(Cookie):    512 bytes
	 * TargetFQDN:          512 bytes
	 * TargetNetBiosName:    32 bytes
	 */

	if (redirection->flags & LB_TARGET_NET_ADDRESS)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->TargetNetAddress), 80))
			return STATE_RUN_FAILED;
	}

	if (redirection->flags & LB_LOAD_BALANCE_INFO)
	{
		/* See [MSFT-SDLBTS] (a.k.a. TS_Session_Directory.doc)
		 * load balance info example data:
		 * 0000  43 6f 6f 6b 69 65 3a 20 6d 73 74 73 3d 32 31 33  Cookie: msts=213
		 * 0010  34 30 32 36 34 33 32 2e 31 35 36 32 39 2e 30 30  4026432.15629.00
		 * 0020  30 30 0d 0a                                      00..
		 */
		if (!rdp_redirection_read_data(LB_LOAD_BALANCE_INFO, s, &redirection->LoadBalanceInfoLength,
		                               &redirection->LoadBalanceInfo))
			return STATE_RUN_FAILED;
	}

	if (redirection->flags & LB_USERNAME)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->Username), 512))
			return STATE_RUN_FAILED;

		WLog_DBG(TAG, "Username: %s", redirection->Username);
	}

	if (redirection->flags & LB_DOMAIN)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->Domain), 52))
			return STATE_RUN_FAILED;

		WLog_DBG(TAG, "Domain: %s", redirection->Domain);
	}

	if (redirection->flags & LB_PASSWORD)
	{
		/* Note: Password is a variable-length array of bytes containing the
		 * password used by the user in Unicode format, including a null-terminator
		 * or (!) or a cookie value that MUST be passed to the target server on
		 * successful connection.
		 * Since the format of the password cookie (probably some salted hash) is
		 * currently unknown we'll treat it as opaque data. All cookies seen so far
		 * are 120 bytes including \0\0 termination.
		 * Here is an observed example of a redirection password cookie:
		 *
		 * 0000  02 00 00 80 44 53 48 4c 60 ab 69 2f 07 d6 9e 2d  ....DSHL`.i/...-
		 * 0010  f0 3a 97 3b a9 c5 ec 7e 66 bd b3 84 6c b1 ef b9  .:.;...~f...l...
		 * 0020  b6 82 4e cc 3a df 64 b7 7b 25 04 54 c2 58 98 f8  ..N.:.d.{%.T.X..
		 * 0030  97 87 d4 93 c7 c1 e1 5b c2 85 f8 22 49 1f 81 88  .......[..."I...
		 * 0040  43 44 83 f6 9a 72 40 24 dc 4d 43 cb d9 92 3c 8f  CD...r@$.MC...<.
		 * 0050  3a 37 5c 77 13 a0 72 3c 72 08 64 2a 29 fb dc eb  :7\w..r<r.d*)...
		 * 0060  0d 2b 06 b4 c6 08 b4 73 34 16 93 62 6d 24 e9 93  .+.....s4..bm$..
		 * 0070  97 27 7b dd 9a 72 00 00                          .'{..r..
		 *
		 * Notwithstanding the above, we'll allocated an additional zero WCHAR at the
		 * end of the buffer which won't get counted in PasswordLength.
		 */
		if (!rdp_redirection_read_data(LB_PASSWORD, s, &redirection->PasswordLength,
		                               &redirection->Password))
			return STATE_RUN_FAILED;

		/* [MS-RDPBCGR] specifies 512 bytes as the upper limit for the password length
		 * including the null terminatior(s). This should also be enough for the unknown
		 * password cookie format (see previous comment).
		 */
		if ((redirection->flags & LB_PASSWORD_IS_PK_ENCRYPTED) == 0)
		{
			const size_t charLen = redirection->PasswordLength / sizeof(WCHAR);
			if (redirection->PasswordLength > LB_PASSWORD_MAX_LENGTH)
				return STATE_RUN_FAILED;

			/* Ensure the text password is '\0' terminated */
			if (_wcsnlen((const WCHAR*)redirection->Password, charLen) == charLen)
				return STATE_RUN_FAILED;
		}
	}

	if (redirection->flags & LB_TARGET_FQDN)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->TargetFQDN), 512))
			return STATE_RUN_FAILED;

		WLog_DBG(TAG, "TargetFQDN: %s", redirection->TargetFQDN);
	}

	if (redirection->flags & LB_TARGET_NETBIOS_NAME)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->TargetNetBiosName), 32))
			return STATE_RUN_FAILED;

		WLog_DBG(TAG, "TargetNetBiosName: %s", redirection->TargetNetBiosName);
	}

	if (redirection->flags & LB_CLIENT_TSV_URL)
	{
		if (!rdp_redirection_read_data(LB_CLIENT_TSV_URL, s, &redirection->TsvUrlLength,
		                               &redirection->TsvUrl))
			return STATE_RUN_FAILED;
	}

	if (redirection->flags & LB_REDIRECTION_GUID)
	{
		if (!rdp_redirection_read_data(LB_REDIRECTION_GUID, s, &redirection->RedirectionGuidLength,
		                               &redirection->RedirectionGuid))
			return STATE_RUN_FAILED;
	}

	if (redirection->flags & LB_TARGET_CERTIFICATE)
	{
		if (!rdp_redirection_read_target_cert_stream(s, redirection))
			return STATE_RUN_FAILED;
	}

	if (redirection->flags & LB_TARGET_NET_ADDRESSES)
	{
		UINT32 count = 0;
		UINT32 targetNetAddressesLength = 0;

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return STATE_RUN_FAILED;

		Stream_Read_UINT32(s, targetNetAddressesLength);
		Stream_Read_UINT32(s, redirection->TargetNetAddressesCount);
		count = redirection->TargetNetAddressesCount;
		redirection->TargetNetAddresses = (char**)calloc(count, sizeof(char*));

		if (!redirection->TargetNetAddresses)
			return STATE_RUN_FAILED;

		WLog_DBG(TAG, "TargetNetAddressesCount: %" PRIu32 "", count);

		for (UINT32 i = 0; i < count; i++)
		{
			if (!rdp_redirection_read_unicode_string(s, &(redirection->TargetNetAddresses[i]), 80))
				return STATE_RUN_FAILED;

			WLog_DBG(TAG, "TargetNetAddresses[%" PRIu32 "]: %s", i,
			         redirection->TargetNetAddresses[i]);
		}
	}

	if (Stream_GetRemainingLength(s) >= 8)
	{
		/* some versions of windows don't included this padding before closing the connection */
		Stream_Seek(s, 8); /* pad (8 bytes) */
	}

	if (redirection->flags & LB_NOREDIRECT)
		return STATE_RUN_SUCCESS;

	return STATE_RUN_REDIRECT;
}

state_run_t rdp_recv_enhanced_security_redirection_packet(rdpRdp* rdp, wStream* s)
{
	state_run_t status = STATE_RUN_SUCCESS;

	if (!Stream_SafeSeek(s, 2)) /* pad2Octets (2 bytes) */
		return STATE_RUN_FAILED;

	status = rdp_recv_server_redirection_pdu(rdp, s);

	if (state_run_failed(status))
		return status;

	if (Stream_GetRemainingLength(s) >= 1)
	{
		/* this field is optional, and its absence is not an error */
		Stream_Seek(s, 1); /* pad2Octets (1 byte) */
	}

	return status;
}

rdpRedirection* redirection_new(void)
{
	rdpRedirection* redirection = (rdpRedirection*)calloc(1, sizeof(rdpRedirection));

	if (redirection)
	{
	}

	return redirection;
}

void redirection_free(rdpRedirection* redirection)
{
	if (redirection)
	{
		redirection_free_data(&redirection->TsvUrl, &redirection->TsvUrlLength);
		redirection_free_string(&redirection->Username);
		redirection_free_string(&redirection->Domain);
		redirection_free_string(&redirection->TargetFQDN);
		redirection_free_string(&redirection->TargetNetBiosName);
		redirection_free_string(&redirection->TargetNetAddress);
		redirection_free_data(&redirection->LoadBalanceInfo, &redirection->LoadBalanceInfoLength);
		redirection_free_data(&redirection->Password, &redirection->PasswordLength);
		redirection_free_data(&redirection->RedirectionGuid, &redirection->RedirectionGuidLength);
		freerdp_certificate_free(redirection->TargetCertificate);
		redirection_free_array(&redirection->TargetNetAddresses,
		                       &redirection->TargetNetAddressesCount);

		free(redirection);
	}
}

static SSIZE_T redir_write_string(UINT32 flag, wStream* s, const char* str)
{
	const size_t length = (strlen(str) + 1);
	if (!Stream_EnsureRemainingCapacity(s, 4ull + length * sizeof(WCHAR)))
		return -1;

	const size_t pos = Stream_GetPosition(s);
	Stream_Write_UINT32(s, (UINT32)length * sizeof(WCHAR));
	if (Stream_Write_UTF16_String_From_UTF8(s, length, str, length, TRUE) < 0)
		return -1;
	return (SSIZE_T)(Stream_GetPosition(s) - pos);
}

static BOOL redir_write_data(UINT32 flag, wStream* s, UINT32 length, const BYTE* data)
{
	if (!Stream_EnsureRemainingCapacity(s, 4ull + length))
		return FALSE;

	Stream_Write_UINT32(s, length);
	Stream_Write(s, data, length);
	return TRUE;
}

BOOL rdp_write_enhanced_security_redirection_packet(wStream* s, const rdpRedirection* redirection)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(s);
	WINPR_ASSERT(redirection);

	if (!Stream_EnsureRemainingCapacity(s, 14))
		goto fail;

	Stream_Write_UINT16(s, 0);

	const size_t start = Stream_GetPosition(s);
	Stream_Write_UINT16(s, SEC_REDIRECTION_PKT);
	const size_t lengthOffset = Stream_GetPosition(s);
	Stream_Seek_UINT16(s); /* placeholder for length */

	if (redirection->sessionID)
		Stream_Write_UINT32(s, redirection->sessionID);
	else
		Stream_Write_UINT32(s, 0);

	Stream_Write_UINT32(s, redirection->flags);

	if (redirection->flags & LB_TARGET_NET_ADDRESS)
	{
		if (redir_write_string(LB_TARGET_NET_ADDRESS, s, redirection->TargetNetAddress) < 0)
			goto fail;
	}

	if (redirection->flags & LB_LOAD_BALANCE_INFO)
	{
		const UINT32 length = 13 + redirection->LoadBalanceInfoLength + 2;
		if (!Stream_EnsureRemainingCapacity(s, length))
			goto fail;
		Stream_Write_UINT32(s, length);
		Stream_Write(s, "Cookie: msts=", 13);
		Stream_Write(s, redirection->LoadBalanceInfo, redirection->LoadBalanceInfoLength);
		Stream_Write_UINT8(s, 0x0d);
		Stream_Write_UINT8(s, 0x0a);
	}

	if (redirection->flags & LB_USERNAME)
	{
		if (redir_write_string(LB_USERNAME, s, redirection->Username) < 0)
			goto fail;
	}

	if (redirection->flags & LB_DOMAIN)
	{
		if (redir_write_string(LB_DOMAIN, s, redirection->Domain) < 0)
			goto fail;
	}

	if (redirection->flags & LB_PASSWORD)
	{
		/* Password is eighter UNICODE or opaque data */
		if (!redir_write_data(LB_PASSWORD, s, redirection->PasswordLength, redirection->Password))
			goto fail;
	}

	if (redirection->flags & LB_TARGET_FQDN)
	{
		if (redir_write_string(LB_TARGET_FQDN, s, redirection->TargetFQDN) < 0)
			goto fail;
	}

	if (redirection->flags & LB_TARGET_NETBIOS_NAME)
	{
		if (redir_write_string(LB_TARGET_NETBIOS_NAME, s, redirection->TargetNetBiosName) < 0)
			goto fail;
	}

	if (redirection->flags & LB_CLIENT_TSV_URL)
	{
		if (!redir_write_data(LB_CLIENT_TSV_URL, s, redirection->TsvUrlLength, redirection->TsvUrl))
			goto fail;
	}

	if (redirection->flags & LB_REDIRECTION_GUID)
	{
		if (!redir_write_data(LB_REDIRECTION_GUID, s, redirection->RedirectionGuidLength,
		                      redirection->RedirectionGuid))
			goto fail;
	}

	if (redirection->flags & LB_TARGET_CERTIFICATE)
	{
		if (!rdp_redireciton_write_target_cert_stream(s, redirection))
			goto fail;
	}

	if (redirection->flags & LB_TARGET_NET_ADDRESSES)
	{
		UINT32 length = sizeof(UINT32);

		if (!Stream_EnsureRemainingCapacity(s, 2 * sizeof(UINT32)))
			goto fail;

		const size_t lstart = Stream_GetPosition(s);
		Stream_Seek_UINT32(s); /* length of field */
		Stream_Write_UINT32(s, redirection->TargetNetAddressesCount);
		for (UINT32 i = 0; i < redirection->TargetNetAddressesCount; i++)
		{
			const SSIZE_T rcc =
			    redir_write_string(LB_TARGET_NET_ADDRESSES, s, redirection->TargetNetAddresses[i]);
			if (rcc < 0)
				goto fail;
			length += (UINT32)rcc;
		}

		/* Write length field */
		const size_t lend = Stream_GetPosition(s);
		Stream_SetPosition(s, lstart);
		Stream_Write_UINT32(s, length);
		Stream_SetPosition(s, lend);
	}

	/* Padding 8 bytes */
	if (!Stream_EnsureRemainingCapacity(s, 8))
		goto fail;
	Stream_Zero(s, 8);

	const size_t end = Stream_GetPosition(s);
	Stream_SetPosition(s, lengthOffset);
	Stream_Write_UINT16(s, (UINT16)(end - start));
	Stream_SetPosition(s, end);

	rc = TRUE;
fail:
	return rc;
}

BOOL redirection_settings_are_valid(rdpRedirection* redirection, UINT32* pFlags)
{
	UINT32 flags = 0;

	WINPR_ASSERT(redirection);

	if (redirection->flags & LB_CLIENT_TSV_URL)
	{
		if (!redirection->TsvUrl || (redirection->TsvUrlLength == 0))
			flags |= LB_CLIENT_TSV_URL;
	}

	if (redirection->flags & LB_SERVER_TSV_CAPABLE)
	{
		if ((redirection->flags & LB_CLIENT_TSV_URL) == 0)
			flags |= LB_SERVER_TSV_CAPABLE;
	}

	if (redirection->flags & LB_USERNAME)
	{
		if (utils_str_is_empty(redirection->Username))
			flags |= LB_USERNAME;
	}

	if (redirection->flags & LB_DOMAIN)
	{
		if (utils_str_is_empty(redirection->Domain))
			flags |= LB_DOMAIN;
	}

	if (redirection->flags & LB_PASSWORD)
	{
		if (!redirection->Password || (redirection->PasswordLength == 0))
			flags |= LB_PASSWORD;
	}

	if (redirection->flags & LB_TARGET_FQDN)
	{
		if (utils_str_is_empty(redirection->TargetFQDN))
			flags |= LB_TARGET_FQDN;
	}

	if (redirection->flags & LB_LOAD_BALANCE_INFO)
	{
		if (!redirection->LoadBalanceInfo || (redirection->LoadBalanceInfoLength == 0))
			flags |= LB_LOAD_BALANCE_INFO;
	}

	if (redirection->flags & LB_TARGET_NETBIOS_NAME)
	{
		if (utils_str_is_empty(redirection->TargetNetBiosName))
			flags |= LB_TARGET_NETBIOS_NAME;
	}

	if (redirection->flags & LB_TARGET_NET_ADDRESS)
	{
		if (utils_str_is_empty(redirection->TargetNetAddress))
			flags |= LB_TARGET_NET_ADDRESS;
	}

	if (redirection->flags & LB_TARGET_NET_ADDRESSES)
	{
		if (!redirection->TargetNetAddresses || (redirection->TargetNetAddressesCount == 0))
			flags |= LB_TARGET_NET_ADDRESSES;
		else
		{
			for (UINT32 x = 0; x < redirection->TargetNetAddressesCount; x++)
			{
				if (!redirection->TargetNetAddresses[x])
					flags |= LB_TARGET_NET_ADDRESSES;
			}
		}
	}

	if (redirection->flags & LB_REDIRECTION_GUID)
	{
		if (!redirection->RedirectionGuid || (redirection->RedirectionGuidLength == 0))
			flags |= LB_REDIRECTION_GUID;
	}

	if (redirection->flags & LB_TARGET_CERTIFICATE)
	{
		if (!redirection->TargetCertificate)
			flags |= LB_TARGET_CERTIFICATE;
	}

	if (pFlags)
		*pFlags = flags;
	return flags == 0;
}

BOOL redirection_set_flags(rdpRedirection* redirection, UINT32 flags)
{
	WINPR_ASSERT(redirection);
	redirection->flags = flags;
	return TRUE;
}

BOOL redirection_set_session_id(rdpRedirection* redirection, UINT32 session_id)
{
	WINPR_ASSERT(redirection);
	redirection->sessionID = session_id;
	return TRUE;
}

static BOOL redirection_unsupported(const char* fkt, UINT32 flag, UINT32 mask)
{
	char buffer[1024] = { 0 };
	char buffer2[1024] = { 0 };
	WLog_WARN(TAG, "[%s] supported flags are {%s}, have {%s}", fkt,
	          rdp_redirection_flags_to_string(mask, buffer, sizeof(buffer)),
	          rdp_redirection_flags_to_string(flag, buffer2, sizeof(buffer2)));
	return FALSE;
}

BOOL redirection_set_byte_option(rdpRedirection* redirection, UINT32 flag, const BYTE* data,
                                 size_t length)
{
	WINPR_ASSERT(redirection);
	switch (flag)
	{
		case LB_CLIENT_TSV_URL:
			return redirection_copy_data(&redirection->TsvUrl, &redirection->TsvUrlLength, data,
			                             length);
		case LB_PASSWORD:
			return redirection_copy_data(&redirection->Password, &redirection->PasswordLength, data,
			                             length);
		case LB_LOAD_BALANCE_INFO:
			return redirection_copy_data(&redirection->LoadBalanceInfo,
			                             &redirection->LoadBalanceInfoLength, data, length);
		case LB_REDIRECTION_GUID:
			return redirection_copy_data(&redirection->RedirectionGuid,
			                             &redirection->RedirectionGuidLength, data, length);
		case LB_TARGET_CERTIFICATE:
			freerdp_certificate_free(redirection->TargetCertificate);
			return FALSE; // TODO rdp_redireciton_read_target_cert(redirection, data, length);
		default:
			return redirection_unsupported(__FUNCTION__, flag,
			                               LB_CLIENT_TSV_URL | LB_PASSWORD | LB_LOAD_BALANCE_INFO |
			                                   LB_REDIRECTION_GUID | LB_TARGET_CERTIFICATE);
	}
}

BOOL redirection_set_string_option(rdpRedirection* redirection, UINT32 flag, const char* str)
{
	WINPR_ASSERT(redirection);
	switch (flag)
	{
		case LB_USERNAME:
			return redirection_copy_string(&redirection->Username, str);
		case LB_DOMAIN:
			return redirection_copy_string(&redirection->Domain, str);
		case LB_TARGET_FQDN:
			return redirection_copy_string(&redirection->TargetFQDN, str);
		case LB_TARGET_NETBIOS_NAME:
			return redirection_copy_string(&redirection->TargetNetBiosName, str);
		case LB_TARGET_NET_ADDRESS:
			return redirection_copy_string(&redirection->TargetNetAddress, str);
		default:
			return redirection_unsupported(__FUNCTION__, flag,
			                               LB_USERNAME | LB_DOMAIN | LB_TARGET_FQDN |
			                                   LB_TARGET_NETBIOS_NAME | LB_TARGET_NET_ADDRESS);
	}
}

BOOL redirection_set_array_option(rdpRedirection* redirection, UINT32 flag, const char** str,
                                  size_t count)
{
	WINPR_ASSERT(redirection);
	switch (flag)
	{
		case LB_TARGET_NET_ADDRESSES:
			return redirection_copy_array(&redirection->TargetNetAddresses,
			                              &redirection->TargetNetAddressesCount, str, count);
		default:
			return redirection_unsupported(__FUNCTION__, flag, LB_TARGET_NET_ADDRESSES);
	}
}
