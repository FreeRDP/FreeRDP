/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Azure Virtual Desktop Gateway / Azure Resource Manager
 *
 * Copyright 2023 Michael Saxl <mike@mwsys.mine.bz>
 * Copyright 2023 David Fort <contact@hardening-consulting.com>
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
#include <freerdp/version.h>

#include "../settings.h"

#include <winpr/assert.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/winsock.h>
#include <winpr/cred.h>
#include <winpr/bcrypt.h>

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/crypto/certificate.h>
#include <freerdp/utils/ringbuffer.h>
#include <freerdp/utils/smartcardlogon.h>
#include <freerdp/utils/aad.h>

#include "arm.h"
#include "wst.h"
#include "websocket.h"
#include "http.h"
#include "../credssp_auth.h"
#include "../proxy.h"
#include "../rdp.h"
#include "../../crypto/crypto.h"
#include "../../crypto/certificate.h"
#include "../../crypto/opensslcompat.h"
#include "rpc_fault.h"
#include "../utils.h"
#include "../redirection.h"

#include <winpr/json.h>

#include <string.h>

struct rdp_arm
{
	rdpContext* context;
	rdpTls* tls;
	HttpContext* http;

	UINT32 gateway_retry;
	wLog* log;
};

typedef struct rdp_arm rdpArm;

#define TAG FREERDP_TAG("core.gateway.arm")

#ifdef WITH_AAD
static BOOL arm_tls_connect(rdpArm* arm, rdpTls* tls, UINT32 timeout)
{
	WINPR_ASSERT(arm);
	WINPR_ASSERT(tls);
	int sockfd = 0;
	long status = 0;
	BIO* socketBio = NULL;
	BIO* bufferedBio = NULL;
	rdpSettings* settings = arm->context->settings;
	if (!settings)
		return FALSE;

	const char* peerHostname = freerdp_settings_get_string(settings, FreeRDP_GatewayHostname);
	if (!peerHostname)
		return FALSE;

	UINT16 peerPort = (UINT16)freerdp_settings_get_uint32(settings, FreeRDP_GatewayPort);
	const char* proxyUsername = NULL;
	const char* proxyPassword = NULL;
	BOOL isProxyConnection =
	    proxy_prepare(settings, &peerHostname, &peerPort, &proxyUsername, &proxyPassword);

	sockfd = freerdp_tcp_connect(arm->context, peerHostname, peerPort, timeout);

	WLog_Print(arm->log, WLOG_DEBUG, "connecting to %s %d", peerHostname, peerPort);
	if (sockfd < 0)
		return FALSE;

	socketBio = BIO_new(BIO_s_simple_socket());

	if (!socketBio)
	{
		closesocket((SOCKET)sockfd);
		return FALSE;
	}

	BIO_set_fd(socketBio, sockfd, BIO_CLOSE);
	bufferedBio = BIO_new(BIO_s_buffered_socket());

	if (!bufferedBio)
	{
		BIO_free_all(socketBio);
		return FALSE;
	}

	bufferedBio = BIO_push(bufferedBio, socketBio);
	if (!bufferedBio)
		return FALSE;

	status = BIO_set_nonblock(bufferedBio, TRUE);

	if (isProxyConnection)
	{
		if (!proxy_connect(arm->context, bufferedBio, proxyUsername, proxyPassword,
		                   freerdp_settings_get_string(settings, FreeRDP_GatewayHostname),
		                   (UINT16)freerdp_settings_get_uint32(settings, FreeRDP_GatewayPort)))
		{
			BIO_free_all(bufferedBio);
			return FALSE;
		}
	}

	if (!status)
	{
		BIO_free_all(bufferedBio);
		return FALSE;
	}

	tls->hostname = freerdp_settings_get_string(settings, FreeRDP_GatewayHostname);
	tls->port = MIN(UINT16_MAX, WINPR_ASSERTING_INT_CAST(int32_t, settings->GatewayPort));
	tls->isGatewayTransport = TRUE;
	status = freerdp_tls_connect(tls, bufferedBio);
	if (status < 1)
	{
		rdpContext* context = arm->context;
		if (status < 0)
			freerdp_set_last_error_if_not(context, FREERDP_ERROR_TLS_CONNECT_FAILED);
		else
			freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_CANCELLED);

		return FALSE;
	}
	return (status >= 1);
}

static BOOL arm_fetch_wellknown(rdpArm* arm)
{
	WINPR_ASSERT(arm);
	WINPR_ASSERT(arm->context);
	WINPR_ASSERT(arm->context->rdp);

	rdpRdp* rdp = arm->context->rdp;
	if (rdp->wellknown)
		return TRUE;

	const char* base =
	    freerdp_settings_get_string(arm->context->settings, FreeRDP_GatewayAzureActiveDirectory);
	const BOOL useTenant =
	    freerdp_settings_get_bool(arm->context->settings, FreeRDP_GatewayAvdUseTenantid);
	const char* tenantid = "common";
	if (useTenant)
		tenantid =
		    freerdp_settings_get_string(arm->context->settings, FreeRDP_GatewayAvdAadtenantid);

	rdp->wellknown = freerdp_utils_aad_get_wellknown(arm->log, base, tenantid);
	return rdp->wellknown ? TRUE : FALSE;
}

static wStream* arm_build_http_request(rdpArm* arm, const char* method,
                                       TRANSFER_ENCODING transferEncoding, const char* content_type,
                                       size_t content_length)
{
	wStream* s = NULL;

	WINPR_ASSERT(arm);
	WINPR_ASSERT(method);
	WINPR_ASSERT(content_type);

	WINPR_ASSERT(arm->context);
	WINPR_ASSERT(arm->context->rdp);

	const char* uri = http_context_get_uri(arm->http);
	HttpRequest* request = http_request_new();

	if (!request)
		return NULL;

	rdpSettings* settings = arm->context->settings;

	if (!http_request_set_method(request, method) || !http_request_set_uri(request, uri))
		goto out;

	if (!freerdp_settings_get_string(settings, FreeRDP_GatewayHttpExtAuthBearer))
	{
		char* token = NULL;

		pGetCommonAccessToken GetCommonAccessToken = freerdp_get_common_access_token(arm->context);
		if (!GetCommonAccessToken)
		{
			WLog_Print(arm->log, WLOG_ERROR, "No authorization token provided");
			goto out;
		}

		if (!arm_fetch_wellknown(arm))
			goto out;

		if (!GetCommonAccessToken(arm->context, ACCESS_TOKEN_TYPE_AVD, &token, 0))
		{
			WLog_Print(arm->log, WLOG_ERROR, "Unable to obtain access token");
			goto out;
		}

		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayHttpExtAuthBearer, token))
		{
			free(token);
			goto out;
		}
		free(token);
	}

	if (!http_request_set_auth_scheme(request, "Bearer") ||
	    !http_request_set_auth_param(
	        request, freerdp_settings_get_string(settings, FreeRDP_GatewayHttpExtAuthBearer)))
		goto out;

	if (!http_request_set_transfer_encoding(request, transferEncoding) ||
	    !http_request_set_content_length(request, content_length) ||
	    !http_request_set_content_type(request, content_type))
		goto out;

	s = http_request_write(arm->http, request);
out:
	http_request_free(request);

	if (s)
		Stream_SealLength(s);

	return s;
}

static BOOL arm_send_http_request(rdpArm* arm, rdpTls* tls, const char* method,
                                  const char* content_type, const char* data, size_t content_length)
{
	int status = -1;
	wStream* s =
	    arm_build_http_request(arm, method, TransferEncodingIdentity, content_type, content_length);

	if (!s)
		return FALSE;

	const size_t sz = Stream_Length(s);
	WLog_Print(arm->log, WLOG_TRACE, "header [%" PRIuz "]: %s", sz, Stream_Buffer(s));
	WLog_Print(arm->log, WLOG_TRACE, "body   [%" PRIuz "]: %s", content_length, data);
	status = freerdp_tls_write_all(tls, Stream_Buffer(s), sz);

	Stream_Free(s, TRUE);
	if (status >= 0 && content_length > 0 && data)
		status = freerdp_tls_write_all(tls, (const BYTE*)data, content_length);

	return (status >= 0);
}

static void arm_free(rdpArm* arm)
{
	if (!arm)
		return;

	freerdp_tls_free(arm->tls);
	http_context_free(arm->http);

	free(arm);
}

static rdpArm* arm_new(rdpContext* context)
{
	WINPR_ASSERT(context);

	rdpArm* arm = (rdpArm*)calloc(1, sizeof(rdpArm));
	if (!arm)
		goto fail;

	arm->log = WLog_Get(TAG);
	arm->context = context;
	arm->tls = freerdp_tls_new(context);
	if (!arm->tls)
		goto fail;

	arm->http = http_context_new();

	if (!arm->http)
		goto fail;

	return arm;

fail:
	arm_free(arm);
	return NULL;
}

static char* arm_create_request_json(rdpArm* arm)
{
	char* lbi = NULL;
	char* message = NULL;

	WINPR_ASSERT(arm);

	WINPR_JSON* json = WINPR_JSON_CreateObject();
	if (!json)
		goto arm_create_cleanup;
	WINPR_JSON_AddStringToObject(
	    json, "application",
	    freerdp_settings_get_string(arm->context->settings, FreeRDP_RemoteApplicationProgram));

	lbi = calloc(
	    freerdp_settings_get_uint32(arm->context->settings, FreeRDP_LoadBalanceInfoLength) + 1,
	    sizeof(char));
	if (!lbi)
		goto arm_create_cleanup;

	{
		const size_t len =
		    freerdp_settings_get_uint32(arm->context->settings, FreeRDP_LoadBalanceInfoLength);
		memcpy(lbi, freerdp_settings_get_pointer(arm->context->settings, FreeRDP_LoadBalanceInfo),
		       len);
	}

	WINPR_JSON_AddStringToObject(json, "loadBalanceInfo", lbi);
	WINPR_JSON_AddNullToObject(json, "LogonToken");
	WINPR_JSON_AddNullToObject(json, "gatewayLoadBalancerToken");

	message = WINPR_JSON_PrintUnformatted(json);
arm_create_cleanup:
	if (json)
		WINPR_JSON_Delete(json);
	free(lbi);
	return message;
}

/**
 * @brief treats the redirectedAuthBlob
 *
 * sample pbInput:
 * @code
 *  41004500530000004b44424d01000000200000006ee71b295810b3fd13799da3825d0efa3a628e8f4a6eda609ffa975408556546
 *  'A\x00E\x00S\x00\x00\x00KDBM\x01\x00\x00\x00
 * \x00\x00\x00n\xe7\x1b)X\x10\xb3\xfd\x13y\x9d\xa3\x82]\x0e\xfa:b\x8e\x8fJn\xda`\x9f\xfa\x97T\x08UeF'
 * @endcode
 *
 * @param pbInput the raw auth blob (base64 and utf16 decoded)
 * @param cbInput size of pbInput
 * @return the corresponding WINPR_CIPHER_CTX if success, NULL otherwise
 */
static WINPR_CIPHER_CTX* treatAuthBlob(wLog* log, const BYTE* pbInput, size_t cbInput,
                                       size_t* pBlockSize)
{
	WINPR_CIPHER_CTX* ret = NULL;
	char algoName[100] = { 0 };

	WINPR_ASSERT(pBlockSize);
	SSIZE_T algoSz = ConvertWCharNToUtf8((const WCHAR*)pbInput, cbInput / sizeof(WCHAR), algoName,
	                                     sizeof(algoName) - 1);
	if (algoSz <= 0)
	{
		WLog_Print(log, WLOG_ERROR, "invalid algoName");
		return NULL;
	}

	if (strcmp(algoName, "AES") != 0)
	{
		WLog_Print(log, WLOG_ERROR, "only AES is supported for now");
		return NULL;
	}

	*pBlockSize = WINPR_AES_BLOCK_SIZE;
	const size_t algoLen = WINPR_ASSERTING_INT_CAST(size_t, (algoSz + 1)) * sizeof(WCHAR);
	if (cbInput < algoLen)
	{
		WLog_Print(log, WLOG_ERROR, "invalid AuthBlob size");
		return NULL;
	}

	cbInput -= algoLen;

	/* BCRYPT_KEY_DATA_BLOB_HEADER */
	wStream staticStream = { 0 };
	wStream* s = Stream_StaticConstInit(&staticStream, &pbInput[algoLen], cbInput);

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 12))
		return NULL;

	const UINT32 dwMagic = Stream_Get_UINT32(s);
	if (dwMagic != BCRYPT_KEY_DATA_BLOB_MAGIC)
	{
		WLog_Print(log, WLOG_ERROR, "unsupported authBlob type");
		return NULL;
	}

	const UINT32 dwVersion = Stream_Get_UINT32(s);
	if (dwVersion != BCRYPT_KEY_DATA_BLOB_VERSION1)
	{
		WLog_Print(log, WLOG_ERROR, "unsupported authBlob version %" PRIu32 ", expecting %d",
		           dwVersion, BCRYPT_KEY_DATA_BLOB_VERSION1);
		return NULL;
	}

	const UINT32 cbKeyData = Stream_Get_UINT32(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, cbKeyData))
	{
		WLog_Print(log, WLOG_ERROR, "invalid authBlob size");
		return NULL;
	}

	WINPR_CIPHER_TYPE cipherType = WINPR_CIPHER_NONE;
	switch (cbKeyData)
	{
		case 16:
			cipherType = WINPR_CIPHER_AES_128_CBC;
			break;
		case 24:
			cipherType = WINPR_CIPHER_AES_192_CBC;
			break;
		case 32:
			cipherType = WINPR_CIPHER_AES_256_CBC;
			break;
		default:
			WLog_Print(log, WLOG_ERROR, "invalid authBlob cipher size");
			return NULL;
	}

	ret = winpr_Cipher_NewEx(cipherType, WINPR_ENCRYPT, Stream_Pointer(s), cbKeyData, NULL, 0);
	if (!ret)
	{
		WLog_Print(log, WLOG_ERROR, "error creating cipher");
		return NULL;
	}

	if (!winpr_Cipher_SetPadding(ret, TRUE))
	{
		WLog_Print(log, WLOG_ERROR, "unable to enable padding on cipher");
		winpr_Cipher_Free(ret);
		return NULL;
	}

	return ret;
}

static BOOL arm_stringEncodeW(const BYTE* pin, size_t cbIn, BYTE** ppOut, size_t* pcbOut)
{
	*ppOut = NULL;
	*pcbOut = 0;

	/* encode to base64 with crlf */
	char* b64encoded = crypto_base64_encode_ex(pin, cbIn, TRUE);
	if (!b64encoded)
		return FALSE;

	/* and then convert to Unicode */
	size_t outSz = 0;
	*ppOut = (BYTE*)ConvertUtf8NToWCharAlloc(b64encoded, strlen(b64encoded), &outSz);
	free(b64encoded);

	if (!*ppOut)
		return FALSE;

	*pcbOut = (outSz + 1) * sizeof(WCHAR);
	return TRUE;
}

static BOOL arm_encodeRedirectPasswd(wLog* log, rdpSettings* settings, const rdpCertificate* cert,
                                     WINPR_CIPHER_CTX* cipher, size_t blockSize)
{
	BOOL ret = FALSE;
	BYTE* output = NULL;
	BYTE* finalOutput = NULL;

	/* let's prepare the encrypted password, first we do a
	 *    cipheredPass = AES(redirectedAuthBlob, toUtf16(passwd))
	 */

	size_t wpasswdLen = 0;
	WCHAR* wpasswd = freerdp_settings_get_string_as_utf16(settings, FreeRDP_Password, &wpasswdLen);
	if (!wpasswd)
	{
		WLog_Print(log, WLOG_ERROR, "error when converting password to UTF16");
		return FALSE;
	}

	const size_t wpasswdBytes = (wpasswdLen + 1) * sizeof(WCHAR);
	BYTE* encryptedPass = calloc(1, wpasswdBytes + blockSize); /* 16: block size of AES (padding) */

	if (!encryptedPass)
		goto out;

	{
		size_t encryptedPassLen = 0;
		if (!winpr_Cipher_Update(cipher, wpasswd, wpasswdBytes, encryptedPass, &encryptedPassLen))
			goto out;

		if (encryptedPassLen > wpasswdBytes)
			goto out;

		{
			size_t finalLen = 0;
			if (!winpr_Cipher_Final(cipher, &encryptedPass[encryptedPassLen], &finalLen))
			{
				WLog_Print(log, WLOG_ERROR, "error when ciphering password");
				goto out;
			}
			encryptedPassLen += finalLen;
		}

		/* then encrypt(cipheredPass, publicKey(redirectedServerCert) */
		{
			size_t output_length = 0;
			if (!freerdp_certificate_publickey_encrypt(cert, encryptedPass, encryptedPassLen,
			                                           &output, &output_length))
			{
				WLog_Print(log, WLOG_ERROR, "unable to encrypt with the server's public key");
				goto out;
			}

			{
				size_t finalOutputLen = 0;
				if (!arm_stringEncodeW(output, output_length, &finalOutput, &finalOutputLen))
				{
					WLog_Print(log, WLOG_ERROR, "unable to base64+utf16 final blob");
					goto out;
				}

				if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RedirectionPassword,
				                                      finalOutput, finalOutputLen))
				{
					WLog_Print(log, WLOG_ERROR,
					           "unable to set the redirection password in settings");
					goto out;
				}
			}
		}
	}

	settings->RdstlsSecurity = TRUE;
	settings->AadSecurity = FALSE;
	settings->NlaSecurity = FALSE;
	settings->RdpSecurity = FALSE;
	settings->TlsSecurity = FALSE;
	settings->RedirectionFlags = LB_PASSWORD_IS_PK_ENCRYPTED;
	ret = TRUE;
out:
	free(finalOutput);
	free(output);
	free(encryptedPass);
	free(wpasswd);
	return ret;
}

/** extract that stupidly over-encoded field that is the equivalent of
 *       base64.b64decode( base64.b64decode(input).decode('utf-16') )
 *  in python
 */
static BOOL arm_pick_base64Utf16Field(wLog* log, const WINPR_JSON* json, const char* name,
                                      BYTE** poutput, size_t* plen)
{
	*poutput = NULL;
	*plen = 0;

	WINPR_JSON* node = WINPR_JSON_GetObjectItemCaseSensitive(json, name);
	if (!node || !WINPR_JSON_IsString(node))
		return TRUE;

	const char* nodeValue = WINPR_JSON_GetStringValue(node);
	if (!nodeValue)
		return TRUE;

	BYTE* output1 = NULL;
	size_t len1 = 0;
	crypto_base64_decode(nodeValue, strlen(nodeValue), &output1, &len1);
	if (!output1 || !len1)
	{
		WLog_Print(log, WLOG_ERROR, "error when first unbase64 for %s", name);
		free(output1);
		return FALSE;
	}

	size_t len2 = 0;
	char* output2 = ConvertWCharNToUtf8Alloc((WCHAR*)output1, len1 / sizeof(WCHAR), &len2);
	free(output1);
	if (!output2 || !len2)
	{
		WLog_Print(log, WLOG_ERROR, "error when decode('utf-16') for %s", name);
		free(output2);
		return FALSE;
	}

	BYTE* output = NULL;
	crypto_base64_decode(output2, len2, &output, plen);
	free(output2);
	if (!output || !*plen)
	{
		WLog_Print(log, WLOG_ERROR, "error when second unbase64 for %s", name);
		free(output);
		return FALSE;
	}

	*poutput = output;
	return TRUE;
}

/**
 * treats the Azure network meta data that will typically look like:
 *
 *   {'interface': [
 *      {'ipv4': {
 *          'ipAddress': [
 *              {'privateIpAddress': 'X.X.X.X',
 *               'publicIpAddress': 'X.X.X.X'}
 *           ],
 *           'subnet': [
 *              {'address': 'X.X.X.X', 'prefix': '24'}
 *           ]
 *      },
 *      'ipv6': {'ipAddress': []},
 *      'macAddress': 'YYYYYYY'}
 *  ]
 *  }
 *
 */

static size_t arm_parse_ipvx_count(WINPR_JSON* ipvX)
{
	WINPR_ASSERT(ipvX);
	WINPR_JSON* ipAddress = WINPR_JSON_GetObjectItemCaseSensitive(ipvX, "ipAddress");
	if (!ipAddress || !WINPR_JSON_IsArray(ipAddress))
		return 0;

	/* Each entry might contain a public and a private address */
	return WINPR_JSON_GetArraySize(ipAddress) * 2;
}

static BOOL arm_parse_ipv6(rdpSettings* settings, WINPR_JSON* ipv6, size_t* pAddressIdx)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(ipv6);
	WINPR_ASSERT(pAddressIdx);

	if (!freerdp_settings_get_bool(settings, FreeRDP_IPv6Enabled))
		return TRUE;

	WINPR_JSON* ipAddress = WINPR_JSON_GetObjectItemCaseSensitive(ipv6, "ipAddress");
	if (!ipAddress || !WINPR_JSON_IsArray(ipAddress))
		return TRUE;
	const size_t naddresses = WINPR_JSON_GetArraySize(ipAddress);
	const uint32_t count = freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
	for (size_t j = 0; j < naddresses; j++)
	{
		WINPR_JSON* adressN = WINPR_JSON_GetArrayItem(ipAddress, j);
		if (!adressN || !WINPR_JSON_IsString(adressN))
			continue;

		const char* addr = WINPR_JSON_GetStringValue(adressN);
		if (utils_str_is_empty(addr))
			continue;

		if (*pAddressIdx >= count)
		{
			WLog_ERR(TAG, "Exceeded TargetNetAddresses, parsing failed");
			return FALSE;
		}

		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_TargetNetAddresses,
		                                        (*pAddressIdx)++, addr))
			return FALSE;
	}
	return TRUE;
}

static BOOL arm_parse_ipv4(rdpSettings* settings, WINPR_JSON* ipv4, size_t* pAddressIdx)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(ipv4);
	WINPR_ASSERT(pAddressIdx);

	WINPR_JSON* ipAddress = WINPR_JSON_GetObjectItemCaseSensitive(ipv4, "ipAddress");
	if (!ipAddress || !WINPR_JSON_IsArray(ipAddress))
		return TRUE;

	const size_t naddresses = WINPR_JSON_GetArraySize(ipAddress);
	const uint32_t count = freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
	for (size_t j = 0; j < naddresses; j++)
	{
		WINPR_JSON* adressN = WINPR_JSON_GetArrayItem(ipAddress, j);
		if (!adressN)
			continue;

		WINPR_JSON* publicIpNode =
		    WINPR_JSON_GetObjectItemCaseSensitive(adressN, "publicIpAddress");
		if (publicIpNode && WINPR_JSON_IsString(publicIpNode))
		{
			const char* publicIp = WINPR_JSON_GetStringValue(publicIpNode);
			if (!utils_str_is_empty(publicIp))
			{
				if (*pAddressIdx >= count)
				{
					WLog_ERR(TAG, "Exceeded TargetNetAddresses, parsing failed");
					return FALSE;
				}
				if (!freerdp_settings_set_pointer_array(settings, FreeRDP_TargetNetAddresses,
				                                        (*pAddressIdx)++, publicIp))
					return FALSE;
			}
		}

		WINPR_JSON* privateIpNode =
		    WINPR_JSON_GetObjectItemCaseSensitive(adressN, "privateIpAddress");
		if (privateIpNode && WINPR_JSON_IsString(privateIpNode))
		{
			const char* privateIp = WINPR_JSON_GetStringValue(privateIpNode);
			if (!utils_str_is_empty(privateIp))
			{
				if (*pAddressIdx >= count)
				{
					WLog_ERR(TAG, "Exceeded TargetNetAddresses, parsing failed");
					return FALSE;
				}
				if (!freerdp_settings_set_pointer_array(settings, FreeRDP_TargetNetAddresses,
				                                        (*pAddressIdx)++, privateIp))
					return FALSE;
			}
		}
	}
	return TRUE;
}

static BOOL arm_treat_azureInstanceNetworkMetadata(wLog* log, const char* metadata,
                                                   rdpSettings* settings)
{
	BOOL ret = FALSE;

	WINPR_ASSERT(settings);

	if (!freerdp_target_net_adresses_reset(settings, 0))
		return FALSE;

	WINPR_JSON* json = WINPR_JSON_Parse(metadata);
	if (!json)
	{
		WLog_Print(log, WLOG_ERROR, "invalid azureInstanceNetworkMetadata");
		return FALSE;
	}

	WINPR_JSON* iface = WINPR_JSON_GetObjectItemCaseSensitive(json, "interface");
	if (!iface)
	{
		ret = TRUE;
		goto out;
	}

	if (!WINPR_JSON_IsArray(iface))
	{
		WLog_Print(log, WLOG_ERROR, "expecting interface to be an Array");
		goto out;
	}

	{
		const size_t interfaceSz = WINPR_JSON_GetArraySize(iface);
		if (interfaceSz == 0)
		{
			WLog_WARN(TAG, "no addresses in azure instance metadata");
			ret = TRUE;
			goto out;
		}

		{
			size_t count = 0;
			for (size_t i = 0; i < interfaceSz; i++)
			{
				WINPR_JSON* interN = WINPR_JSON_GetArrayItem(iface, i);
				if (!interN)
					continue;

				WINPR_JSON* ipv6 = WINPR_JSON_GetObjectItemCaseSensitive(interN, "ipv6");
				if (ipv6)
					count += arm_parse_ipvx_count(ipv6);

				WINPR_JSON* ipv4 = WINPR_JSON_GetObjectItemCaseSensitive(interN, "ipv4");
				if (ipv4)
					count += arm_parse_ipvx_count(ipv4);
			}

			if (!freerdp_target_net_adresses_reset(settings, count))
				return FALSE;
		}

		{
			size_t addressIdx = 0;
			for (size_t i = 0; i < interfaceSz; i++)
			{
				WINPR_JSON* interN = WINPR_JSON_GetArrayItem(iface, i);
				if (!interN)
					continue;

				WINPR_JSON* ipv6 = WINPR_JSON_GetObjectItemCaseSensitive(interN, "ipv6");
				if (ipv6)
				{
					if (!arm_parse_ipv6(settings, ipv6, &addressIdx))
						goto out;
				}

				WINPR_JSON* ipv4 = WINPR_JSON_GetObjectItemCaseSensitive(interN, "ipv4");
				if (ipv4)
				{
					if (!arm_parse_ipv4(settings, ipv4, &addressIdx))
						goto out;
				}
			}
			if (addressIdx > UINT32_MAX)
				goto out;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_TargetNetAddressCount,
			                                 (UINT32)addressIdx))
				goto out;

			ret = addressIdx > 0;
		}
	}

out:
	WINPR_JSON_Delete(json);
	return ret;
}

static void zfree(char* str)
{
	if (str)
	{
		char* cur = str;
		while (*cur != '\0')
			*cur++ = '\0';
	}
	free(str);
}

static BOOL arm_fill_rdstls(rdpArm* arm, rdpSettings* settings, const WINPR_JSON* json,
                            const rdpCertificate* redirectedServerCert)
{
	WINPR_ASSERT(arm);
	BOOL ret = FALSE;
	BYTE* authBlob = NULL;
	WCHAR* wGUID = NULL;

	const char* redirUser = freerdp_settings_get_string(settings, FreeRDP_RedirectionUsername);
	if (redirUser)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_Username, redirUser))
			goto end;
	}

	/* Azure/Entra requires the domain field to be set to 'AzureAD' in most cases.
	 * Some setups have been reported to require a different one, so only supply the suggested
	 * default if there was no other domain provided.
	 */
	{
		const char* redirDomain = freerdp_settings_get_string(settings, FreeRDP_Domain);
		if (!redirDomain)
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_Domain, "AzureAD"))
				goto end;
		}
	}

	{
		const char* duser = freerdp_settings_get_string(settings, FreeRDP_Username);
		const char* ddomain = freerdp_settings_get_string(settings, FreeRDP_Domain);
		const char* dpwd = freerdp_settings_get_string(settings, FreeRDP_Password);
		if (!duser || !dpwd)
		{
			WINPR_ASSERT(arm->context);
			WINPR_ASSERT(arm->context->instance);

			char* username = NULL;
			char* password = NULL;
			char* domain = NULL;

			if (ddomain)
				domain = _strdup(ddomain);
			if (duser)
				username = _strdup(duser);

			const BOOL rc =
			    IFCALLRESULT(FALSE, arm->context->instance->AuthenticateEx, arm->context->instance,
			                 &username, &password, &domain, AUTH_RDSTLS);

			const BOOL rc1 = freerdp_settings_set_string(settings, FreeRDP_Username, username);
			const BOOL rc2 = freerdp_settings_set_string(settings, FreeRDP_Password, password);
			const BOOL rc3 = freerdp_settings_set_string(settings, FreeRDP_Domain, domain);
			zfree(username);
			zfree(password);
			zfree(domain);
			if (!rc || !rc1 || !rc2 || !rc3)
				goto end;
		}
	}

	/* redirectedAuthGuid */
	{
		WINPR_JSON* redirectedAuthGuidNode =
		    WINPR_JSON_GetObjectItemCaseSensitive(json, "redirectedAuthGuid");
		if (!redirectedAuthGuidNode || !WINPR_JSON_IsString(redirectedAuthGuidNode))
			goto end;

		{
			const char* redirectedAuthGuid = WINPR_JSON_GetStringValue(redirectedAuthGuidNode);
			if (!redirectedAuthGuid)
				goto end;

			{
				size_t wGUID_len = 0;
				wGUID = ConvertUtf8ToWCharAlloc(redirectedAuthGuid, &wGUID_len);
				if (!wGUID || (wGUID_len == 0))
				{
					WLog_Print(arm->log, WLOG_ERROR,
					           "unable to allocate space for redirectedAuthGuid");
					goto end;
				}

				{
					const BOOL status = freerdp_settings_set_pointer_len(
					    settings, FreeRDP_RedirectionGuid, wGUID, (wGUID_len + 1) * sizeof(WCHAR));

					if (!status)
					{
						WLog_Print(arm->log, WLOG_ERROR, "unable to set RedirectionGuid");
						goto end;
					}
				}
			}
		}
	}

	/* redirectedAuthBlob */
	{
		size_t authBlobLen = 0;
		if (!arm_pick_base64Utf16Field(arm->log, json, "redirectedAuthBlob", &authBlob,
		                               &authBlobLen))
			goto end;

		{
			size_t blockSize = 0;
			WINPR_CIPHER_CTX* cipher = treatAuthBlob(arm->log, authBlob, authBlobLen, &blockSize);
			if (!cipher)
				goto end;

			{
				const BOOL rerp = arm_encodeRedirectPasswd(arm->log, settings, redirectedServerCert,
				                                           cipher, blockSize);
				winpr_Cipher_Free(cipher);
				if (!rerp)
					goto end;
			}
		}
	}

	ret = TRUE;

end:
	free(wGUID);
	free(authBlob);
	return ret;
}

static BOOL arm_fill_gateway_parameters(rdpArm* arm, const char* message, size_t len)
{
	WINPR_ASSERT(arm);
	WINPR_ASSERT(arm->context);
	WINPR_ASSERT(message);

	rdpCertificate* redirectedServerCert = NULL;
	WINPR_JSON* json = WINPR_JSON_ParseWithLength(message, len);
	BOOL status = FALSE;
	if (!json)
	{
		WLog_Print(arm->log, WLOG_ERROR, "Response data is not valid JSON: %s",
		           WINPR_JSON_GetErrorPtr());
		return FALSE;
	}

	if (WLog_IsLevelActive(arm->log, WLOG_DEBUG))
	{
		char* str = WINPR_JSON_PrintUnformatted(json);
		WLog_Print(arm->log, WLOG_DEBUG, "Got HTTP Response data: %s", str);
		free(str);
	}

	rdpSettings* settings = arm->context->settings;
	WINPR_JSON* gwurl = WINPR_JSON_GetObjectItemCaseSensitive(json, "gatewayLocationPreWebSocket");
	if (!gwurl)
		gwurl = WINPR_JSON_GetObjectItemCaseSensitive(json, "gatewayLocation");
	const char* gwurlstr = WINPR_JSON_GetStringValue(gwurl);
	if (gwurlstr != NULL)
	{
		WLog_Print(arm->log, WLOG_DEBUG, "extracted target url %s", gwurlstr);
		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayUrl, gwurlstr))
			goto fail;
	}

	{
		WINPR_JSON* serverNameNode =
		    WINPR_JSON_GetObjectItemCaseSensitive(json, "redirectedServerName");
		if (serverNameNode)
		{
			const char* serverName = WINPR_JSON_GetStringValue(serverNameNode);
			if (serverName)
			{
				if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, serverName))
					goto fail;
			}
		}
	}

	{
		const char key[] = "redirectedUsername";
		if (WINPR_JSON_HasObjectItem(json, key))
		{
			const char* userName = NULL;
			WINPR_JSON* userNameNode = WINPR_JSON_GetObjectItemCaseSensitive(json, key);
			if (userNameNode)
				userName = WINPR_JSON_GetStringValue(userNameNode);
			if (!freerdp_settings_set_string(settings, FreeRDP_RedirectionUsername, userName))
				goto fail;
		}
	}

	{
		WINPR_JSON* azureMeta =
		    WINPR_JSON_GetObjectItemCaseSensitive(json, "azureInstanceNetworkMetadata");
		if (azureMeta && WINPR_JSON_IsString(azureMeta))
		{
			if (!arm_treat_azureInstanceNetworkMetadata(
			        arm->log, WINPR_JSON_GetStringValue(azureMeta), settings))
			{
				WLog_Print(arm->log, WLOG_ERROR,
				           "error when treating azureInstanceNetworkMetadata");
				goto fail;
			}
		}
	}

	/* redirectedServerCert */
	{
		size_t certLen = 0;
		BYTE* cert = NULL;
		if (arm_pick_base64Utf16Field(arm->log, json, "redirectedServerCert", &cert, &certLen))
		{
			const BOOL rc = rdp_redirection_read_target_cert(&redirectedServerCert, cert, certLen);
			free(cert);
			if (!rc)
				goto fail;
			else if (!rdp_set_target_certificate(settings, redirectedServerCert))
				goto fail;
		}
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_AadSecurity))
		status = TRUE;
	else
		status = arm_fill_rdstls(arm, settings, json, redirectedServerCert);

fail:
	WINPR_JSON_Delete(json);
	freerdp_certificate_free(redirectedServerCert);
	return status;
}

static BOOL arm_handle_request_ok(rdpArm* arm, const HttpResponse* response)
{
	const size_t len = http_response_get_body_length(response);
	const char* msg = http_response_get_body(response);
	const size_t alen = strnlen(msg, len + 1);
	if (alen > len)
	{
		WLog_Print(arm->log, WLOG_ERROR, "Got HTTP Response data with invalid termination");
		return FALSE;
	}

	return arm_fill_gateway_parameters(arm, msg, len);
}

static BOOL arm_handle_bad_request(rdpArm* arm, const HttpResponse* response, BOOL* retry)
{
	WINPR_ASSERT(response);
	WINPR_ASSERT(retry);

	*retry = FALSE;

	BOOL rc = FALSE;

	http_response_log_error_status(WLog_Get(TAG), WLOG_ERROR, response);

	const size_t len = http_response_get_body_length(response);
	const char* msg = http_response_get_body(response);
	if (msg && (strnlen(msg, len + 1) > len))
	{
		WLog_Print(arm->log, WLOG_ERROR, "Got HTTP Response data, but length is invalid");

		return FALSE;
	}

	WLog_Print(arm->log, WLOG_DEBUG, "Got HTTP Response data: %s", msg);

	WINPR_JSON* json = WINPR_JSON_ParseWithLength(msg, len);
	if (json == NULL)
	{
		const char* error_ptr = WINPR_JSON_GetErrorPtr();
		WLog_Print(arm->log, WLOG_ERROR, "WINPR_JSON_ParseWithLength: %s", error_ptr);

		return FALSE;
	}
	else
	{
		WINPR_JSON* gateway_code_obj = WINPR_JSON_GetObjectItemCaseSensitive(json, "Code");
		const char* gw_code_str = WINPR_JSON_GetStringValue(gateway_code_obj);
		if (gw_code_str == NULL)
		{
			WLog_Print(arm->log, WLOG_ERROR, "Response has no \"Code\" property");
			goto fail;
		}

		if (strcmp(gw_code_str, "E_PROXY_ORCHESTRATION_LB_SESSIONHOST_DEALLOCATED") == 0)
		{
			*retry = TRUE;
			WINPR_JSON* message = WINPR_JSON_GetObjectItemCaseSensitive(json, "Message");
			const char* msgstr = WINPR_JSON_GetStringValue(message);
			if (!msgstr)
				WLog_WARN(TAG, "Starting your VM. It may take up to 5 minutes");
			else
				WLog_WARN(TAG, "%s", msgstr);
			freerdp_set_last_error_if_not(arm->context, FREERDP_ERROR_CONNECT_TARGET_BOOTING);
		}
		else
		{
			goto fail;
		}
	}

	rc = TRUE;
fail:
	WINPR_JSON_Delete(json);
	return rc;
}

static BOOL arm_handle_request(rdpArm* arm, BOOL* retry, DWORD timeout)
{
	WINPR_ASSERT(retry);

	if (!arm_fetch_wellknown(arm))
	{
		*retry = TRUE;
		return FALSE;
	}

	*retry = FALSE;

	char* message = NULL;
	BOOL rc = FALSE;

	HttpResponse* response = NULL;
	long StatusCode = 0;

	const char* useragent =
	    freerdp_settings_get_string(arm->context->settings, FreeRDP_GatewayHttpUserAgent);
	const char* msuseragent =
	    freerdp_settings_get_string(arm->context->settings, FreeRDP_GatewayHttpMsUserAgent);
	if (!http_context_set_uri(arm->http, "/api/arm/v2/connections") ||
	    !http_context_set_accept(arm->http, "*/*") ||
	    !http_context_set_cache_control(arm->http, "no-cache") ||
	    !http_context_set_pragma(arm->http, "no-cache") ||
	    !http_context_set_connection(arm->http, "Keep-Alive") ||
	    !http_context_set_user_agent(arm->http, useragent) ||
	    !http_context_set_x_ms_user_agent(arm->http, msuseragent) ||
	    !http_context_set_host(arm->http, freerdp_settings_get_string(arm->context->settings,
	                                                                  FreeRDP_GatewayHostname)))
		goto arm_error;

	if (!arm_tls_connect(arm, arm->tls, timeout))
		goto arm_error;

	message = arm_create_request_json(arm);
	if (!message)
		goto arm_error;

	if (!arm_send_http_request(arm, arm->tls, "POST", "application/json", message, strlen(message)))
		goto arm_error;

	response = http_response_recv(arm->tls, TRUE);
	if (!response)
		goto arm_error;

	StatusCode = http_response_get_status_code(response);
	if (StatusCode == HTTP_STATUS_OK)
	{
		if (!arm_handle_request_ok(arm, response))
			goto arm_error;
	}
	else if (StatusCode == HTTP_STATUS_BAD_REQUEST)
	{
		if (!arm_handle_bad_request(arm, response, retry))
			goto arm_error;
	}
	else
	{
		http_response_log_error_status(WLog_Get(TAG), WLOG_ERROR, response);
		goto arm_error;
	}

	rc = TRUE;
arm_error:
	http_response_free(response);
	free(message);
	return rc;
}

#endif

BOOL arm_resolve_endpoint(wLog* log, rdpContext* context, DWORD timeout)
{
#ifndef WITH_AAD
	WLog_Print(log, WLOG_ERROR, "arm gateway support not compiled in");
	return FALSE;
#else

	if (!context)
		return FALSE;

	if (!context->settings)
		return FALSE;

	if ((freerdp_settings_get_uint32(context->settings, FreeRDP_LoadBalanceInfoLength) == 0) ||
	    (freerdp_settings_get_string(context->settings, FreeRDP_RemoteApplicationProgram) == NULL))
	{
		WLog_Print(log, WLOG_ERROR, "loadBalanceInfo and RemoteApplicationProgram needed");
		return FALSE;
	}

	rdpArm* arm = arm_new(context);
	if (!arm)
		return FALSE;

	BOOL retry = FALSE;
	BOOL rc = FALSE;
	do
	{
		if (retry && rc)
		{
			freerdp* instance = context->instance;
			WINPR_ASSERT(instance);
			SSIZE_T delay = IFCALLRESULT(-1, instance->RetryDialog, instance, "arm-transport",
			                             arm->gateway_retry, arm);
			arm->gateway_retry++;
			if (delay <= 0)
				break; /* error or no retry desired, abort loop */
			else
			{
				WLog_Print(arm->log, WLOG_DEBUG, "Delay for %" PRIdz "ms before next attempt",
				           delay);
				while (delay > 0)
				{
					DWORD slp = (UINT32)delay;
					if (delay > UINT32_MAX)
						slp = UINT32_MAX;
					Sleep(slp);
					delay -= slp;
				}
			}
		}
		rc = arm_handle_request(arm, &retry, timeout);

	} while (retry && rc);
	arm_free(arm);
	return rc;
#endif
}
