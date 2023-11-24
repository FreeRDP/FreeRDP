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

//#define WITH_AAD
#ifdef WITH_AAD
#include <cjson/cJSON.h>
#endif

#include <string.h>

struct rdp_arm
{
	rdpContext* context;
	rdpTls* tls;
	HttpContext* http;

	UINT32 gateway_retry;
};

typedef struct rdp_arm rdpArm;

#define TAG FREERDP_TAG("core.gateway.arm")

#ifdef WITH_AAD
static BOOL arm_tls_connect(rdpArm* arm, rdpTls* tls, int timeout)
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
	const char *proxyUsername, *proxyPassword;
	BOOL isProxyConnection =
	    proxy_prepare(settings, &peerHostname, &peerPort, &proxyUsername, &proxyPassword);

	sockfd = freerdp_tcp_connect(arm->context, peerHostname, peerPort, timeout);

	WLog_DBG(TAG, "connecting to %s %d", peerHostname, peerPort);
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
		if (!proxy_connect(settings, bufferedBio, proxyUsername, proxyPassword,
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
	tls->port = settings->GatewayPort;
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

static wStream* arm_build_http_request(rdpArm* arm, const char* method,
                                       TRANSFER_ENCODING transferEncoding, const char* content_type,
                                       size_t content_length)
{
	wStream* s = NULL;
	HttpRequest* request = NULL;
	const char* uri;

	WINPR_ASSERT(arm);
	WINPR_ASSERT(method);
	WINPR_ASSERT(content_type);

	WINPR_ASSERT(arm->context);

	freerdp* instance = arm->context->instance;
	WINPR_ASSERT(instance);

	uri = http_context_get_uri(arm->http);
	request = http_request_new();

	if (!request)
		return NULL;

	if (!http_request_set_method(request, method) || !http_request_set_uri(request, uri))
		goto out;

	if (!freerdp_settings_get_string(arm->context->settings, FreeRDP_GatewayHttpExtAuthBearer))
	{
		char* token = NULL;

		if (!instance->GetAccessToken)
		{
			WLog_ERR(TAG, "No authorization token provided");
			goto out;
		}

		if (!instance->GetAccessToken(instance, ACCESS_TOKEN_TYPE_AVD, &token, 0))
		{
			WLog_ERR(TAG, "Unable to obtain access token");
			goto out;
		}

		if (!freerdp_settings_set_string(arm->context->settings, FreeRDP_GatewayHttpExtAuthBearer,
		                                 token))
		{
			free(token);
			goto out;
		}
		free(token);
	}

	if (!http_request_set_auth_scheme(request, "Bearer") ||
	    !http_request_set_auth_param(
	        request,
	        freerdp_settings_get_string(arm->context->settings, FreeRDP_GatewayHttpExtAuthBearer)))
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

	if (sz <= INT_MAX)
		status = freerdp_tls_write_all(tls, Stream_Buffer(s), (int)sz);

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

	arm->context = context;
	arm->tls = freerdp_tls_new(context->settings);
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

	cJSON* json = cJSON_CreateObject();
	if (!json)
		goto arm_create_cleanup;
	cJSON_AddStringToObject(
	    json, "application",
	    freerdp_settings_get_string(arm->context->settings, FreeRDP_RemoteApplicationProgram));

	lbi = calloc(
	    freerdp_settings_get_uint32(arm->context->settings, FreeRDP_LoadBalanceInfoLength) + 1,
	    sizeof(char));
	if (!lbi)
		goto arm_create_cleanup;

	const size_t len =
	    freerdp_settings_get_uint32(arm->context->settings, FreeRDP_LoadBalanceInfoLength);
	memcpy(lbi, freerdp_settings_get_pointer(arm->context->settings, FreeRDP_LoadBalanceInfo), len);

	cJSON_AddStringToObject(json, "loadBalanceInfo", lbi);
	cJSON_AddNullToObject(json, "LogonToken");
	cJSON_AddNullToObject(json, "gatewayLoadBalancerToken");

	message = cJSON_PrintUnformatted(json);
arm_create_cleanup:
	if (json)
		cJSON_Delete(json);
	free(lbi);
	return message;
}

/**
 * treats the redirectedAuthBlob
 *
 * sample pbInput:
 *  41004500530000004b44424d01000000200000006ee71b295810b3fd13799da3825d0efa3a628e8f4a6eda609ffa975408556546
 *  'A\x00E\x00S\x00\x00\x00KDBM\x01\x00\x00\x00
 * \x00\x00\x00n\xe7\x1b)X\x10\xb3\xfd\x13y\x9d\xa3\x82]\x0e\xfa:b\x8e\x8fJn\xda`\x9f\xfa\x97T\x08UeF'
 *
 * @param pbInput the raw auth blob (base64 and utf16 decoded)
 * @param cbInput size of pbInput
 * @return the corresponding WINPR_CIPHER_CTX if success, NULL otherwise
 */
static WINPR_CIPHER_CTX* treatAuthBlob(const BYTE* pbInput, size_t cbInput)
{
	WINPR_CIPHER_CTX* ret = NULL;
	char algoName[100] = { 0 };

	SSIZE_T algoSz = ConvertWCharNToUtf8((const WCHAR*)pbInput, cbInput / sizeof(WCHAR), algoName,
	                                     sizeof(algoName));
	if (algoSz <= 0)
	{
		WLog_ERR(TAG, "invalid algoName");
		return NULL;
	}

	algoName[algoSz] = 0;
	if (strcmp(algoName, "AES") != 0)
	{
		WLog_ERR(TAG, "only AES is supported for now");
		return NULL;
	}

	cbInput -= (algoSz + 1) * sizeof(WCHAR);

	if (cbInput < 12)
	{
		WLog_ERR(TAG, "invalid AuthBlob size");
		return NULL;
	}

	/* BCRYPT_KEY_DATA_BLOB_HEADER */
	wStream staticStream = { 0 };
	wStream* s =
	    Stream_StaticConstInit(&staticStream, pbInput + (algoSz + 1) * sizeof(WCHAR), cbInput);

	UINT32 dwMagic = 0;
	Stream_Read_UINT32(s, dwMagic);

	if (dwMagic != BCRYPT_KEY_DATA_BLOB_MAGIC)
	{
		WLog_ERR(TAG, "unsupported authBlob type");
		return NULL;
	}

	UINT32 dwVersion = 0;
	Stream_Read_UINT32(s, dwVersion);
	if (dwVersion != BCRYPT_KEY_DATA_BLOB_VERSION1)
	{
		WLog_ERR(TAG, "unsupported authBlob version %d, expecting %d", dwVersion,
		         BCRYPT_KEY_DATA_BLOB_VERSION1);
		return NULL;
	}

	UINT32 cbKeyData = 0;
	Stream_Read_UINT32(s, cbKeyData);
	cbInput -= 12;

	if (cbKeyData > cbInput)
	{
		WLog_ERR(TAG, "invalid authBlob size");
		return NULL;
	}

	int cipherType = 0;
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
			WLog_ERR(TAG, "invalid authBlob cipher size");
			return NULL;
	}

	ret = winpr_Cipher_New(cipherType, WINPR_ENCRYPT, Stream_Pointer(s), NULL);
	if (!ret)
	{
		WLog_ERR(TAG, "error creating cipher");
		return NULL;
	}

	if (!winpr_Cipher_SetPadding(ret, TRUE))
	{
		WLog_ERR(TAG, "unable to enable padding on cipher");
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
	size_t outSz;
	*ppOut = (BYTE*)ConvertUtf8NToWCharAlloc(b64encoded, strlen(b64encoded), &outSz);
	free(b64encoded);

	if (!*ppOut)
		return FALSE;

	*pcbOut = (outSz + 1) * sizeof(WCHAR);
	return TRUE;
}

static BOOL arm_encodeRedirectPasswd(rdpSettings* settings, const rdpCertificate* cert,
                                     WINPR_CIPHER_CTX* cipher)
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
		WLog_ERR(TAG, "error when converting password to UTF16");
		return FALSE;
	}

	size_t wpasswdBytes = (wpasswdLen + 1) * sizeof(WCHAR);
	BYTE* encryptedPass = calloc(1, wpasswdBytes + 16); /* 16: block size of AES (padding) */
	size_t encryptedPassLen = 0;
	size_t finalLen = 0;
	if (!encryptedPass ||
	    !winpr_Cipher_Update(cipher, wpasswd, wpasswdBytes, encryptedPass, &encryptedPassLen) ||
	    !winpr_Cipher_Final(cipher, encryptedPass + encryptedPassLen, &finalLen))
	{
		WLog_ERR(TAG, "error when ciphering password");
		goto out;
	}
	encryptedPassLen += finalLen;

	/* then encrypt(cipheredPass, publicKey(redirectedServerCert) */
	size_t output_length = 0;

	if (!freerdp_certificate_publickey_encrypt(cert, encryptedPass, encryptedPassLen, &output,
	                                           &output_length))
	{
		WLog_ERR(TAG, "unable to encrypt with the server's public key");
		goto out;
	}

	size_t finalOutputLen = 0;
	if (!arm_stringEncodeW(output, output_length, &finalOutput, &finalOutputLen))
	{
		WLog_ERR(TAG, "unable to base64+utf16 final blob");
		goto out;
	}

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RedirectionPassword, finalOutput,
	                                      finalOutputLen))
	{
		WLog_ERR(TAG, "unable to set the redirection password in settings");
		goto out;
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
static BOOL arm_pick_base64Utf16Field(const cJSON* json, const char* name, BYTE** poutput,
                                      size_t* plen)
{
	*poutput = NULL;
	*plen = 0;

	const cJSON* node = cJSON_GetObjectItemCaseSensitive(json, name);
	if (!node || !cJSON_IsString(node))
		return TRUE;

	char* nodeValue = cJSON_GetStringValue(node);
	if (!nodeValue)
		return TRUE;

	BYTE* output1 = NULL;
	size_t len1 = 0;
	crypto_base64_decode(nodeValue, strlen(nodeValue), &output1, &len1);
	if (!output1 || !len1)
	{
		WLog_ERR(TAG, "error when first unbase64 for %s", name);
		return FALSE;
	}

	size_t len2 = 0;
	char* output2 = ConvertWCharNToUtf8Alloc((WCHAR*)output1, len1 / sizeof(WCHAR), &len2);
	free(output1);
	if (!output2 || !len2)
	{
		WLog_ERR(TAG, "error when decode('utf-16') for %s", name);
		return FALSE;
	}

	BYTE* output = NULL;
	crypto_base64_decode(output2, len2, &output, plen);
	free(output2);
	if (!output || !*plen)
	{
		WLog_ERR(TAG, "error when second unbase64 for %s", name);
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

static BOOL arm_treat_azureInstanceNetworkMetadata(const char* metadata, rdpSettings* settings)
{
	BOOL ret = FALSE;
	cJSON* json = cJSON_Parse(metadata);
	if (!json)
	{
		WLog_ERR(TAG, "invalid azureInstanceNetworkMetadata");
		return FALSE;
	}

	const cJSON* interface = cJSON_GetObjectItem(json, "interface");
	if (!interface)
	{
		ret = TRUE;
		goto out;
	}

	if (!cJSON_IsArray(interface))
	{
		WLog_ERR(TAG, "expecting interface to be an Array");
		goto out;
	}

	int interfaceSz = cJSON_GetArraySize(interface);
	if (interfaceSz == 0)
	{
		WLog_WARN(TAG, "no addresses in azure instance metadata");
		ret = TRUE;
		goto out;
	}

	for (int i = 0; i < interfaceSz; i++)
	{
		const cJSON* interN = cJSON_GetArrayItem(interface, i);
		if (!interN)
			continue;

		const cJSON* ipv4 = cJSON_GetObjectItem(interN, "ipv4");
		if (!ipv4)
			continue;

		const cJSON* ipAddress = cJSON_GetObjectItem(ipv4, "ipAddress");
		if (!ipAddress || !cJSON_IsArray(ipAddress))
			continue;

		int naddresses = cJSON_GetArraySize(ipAddress);
		for (int j = 0; j < naddresses; j++)
		{
			const cJSON* adressN = cJSON_GetArrayItem(ipAddress, j);
			if (!adressN)
				continue;

			const cJSON* publicIpNode = cJSON_GetObjectItem(adressN, "publicIpAddress");
			if (!publicIpNode || !cJSON_IsString(publicIpNode))
				continue;

			char* publicIp = cJSON_GetStringValue(publicIpNode);
			if (publicIp && strlen(publicIp) &&
			    freerdp_settings_set_string(settings, FreeRDP_RedirectionTargetFQDN, publicIp))
			{
				WLog_INFO(TAG, "connecting to publicIp %s", publicIp);
				ret = TRUE;
				goto out;
			}
		}
	}

	ret = TRUE;

out:
	cJSON_Delete(json);
	return ret;
}

static BOOL arm_fill_rdstls(rdpArm* arm, rdpSettings* settings, const cJSON* json)
{
	BOOL ret = TRUE;
	BYTE* cert = NULL;
	BYTE* authBlob = NULL;
	rdpCertificate* redirectedServerCert = NULL;

	do
	{
		/* redirectedAuthGuid */
		const cJSON* redirectedAuthGuidNode =
		    cJSON_GetObjectItemCaseSensitive(json, "redirectedAuthGuid");
		if (!redirectedAuthGuidNode || !cJSON_IsString(redirectedAuthGuidNode))
			break;

		char* redirectedAuthGuid = cJSON_GetStringValue(redirectedAuthGuidNode);
		if (!redirectedAuthGuid)
			break;

		WCHAR wGUID[72] = {
			0
		}; /* A GUID string is between 32 and 68 characters as string, depending on representation.
		      Add a few extra bytes for braces et al */
		const SSIZE_T wGUID_len = ConvertUtf8ToWChar(redirectedAuthGuid, wGUID, ARRAYSIZE(wGUID));
		if (wGUID_len < 0)
		{
			WLog_ERR(TAG, "unable to allocate space for redirectedAuthGuid");
			ret = FALSE;
			goto endOfFunction;
		}

		BOOL status = freerdp_settings_set_pointer_len(settings, FreeRDP_RedirectionGuid, wGUID,
		                                               (wGUID_len + 1) * sizeof(WCHAR));
		if (!status)
		{
			WLog_ERR(TAG, "unable to set RedirectionGuid");
			ret = FALSE;
			goto endOfFunction;
		}

		/* redirectedServerCert */
		size_t certLen = 0;
		if (!arm_pick_base64Utf16Field(json, "redirectedServerCert", &cert, &certLen))
			break;

		if (!rdp_redirection_read_target_cert(&redirectedServerCert, cert, certLen))
			break;

		/* redirectedAuthBlob */
		size_t authBlobLen = 0;
		if (!arm_pick_base64Utf16Field(json, "redirectedAuthBlob", &authBlob, &authBlobLen))
			break;

		WINPR_CIPHER_CTX* cipher = treatAuthBlob(authBlob, authBlobLen);
		if (!cipher)
			break;

		const BOOL rerp = arm_encodeRedirectPasswd(settings, redirectedServerCert, cipher);
		winpr_Cipher_Free(cipher);
		if (!rerp)
			break;

		ret = TRUE;
	} while (FALSE);

	free(cert);
	freerdp_certificate_free(redirectedServerCert);
	free(authBlob);

endOfFunction:
	return ret;
}

static BOOL arm_fill_gateway_parameters(rdpArm* arm, const char* message, size_t len)
{
	WINPR_ASSERT(arm);
	WINPR_ASSERT(arm->context);
	WINPR_ASSERT(message);

	cJSON* json = cJSON_ParseWithLength(message, len);
	BOOL status = FALSE;
	if (!json)
		return FALSE;

	rdpSettings* settings = arm->context->settings;
	const cJSON* gwurl = cJSON_GetObjectItemCaseSensitive(json, "gatewayLocation");
	if (cJSON_IsString(gwurl) && (gwurl->valuestring != NULL))
	{
		WLog_DBG(TAG, "extracted target url %s", gwurl->valuestring);
		if (!freerdp_settings_set_string(settings, FreeRDP_GatewayUrl, gwurl->valuestring))
			status = FALSE;
		else
			status = TRUE;
	}

	const cJSON* serverNameNode = cJSON_GetObjectItem(json, "redirectedServerName");
	if (serverNameNode)
	{
		char* serverName = cJSON_GetStringValue(serverNameNode);
		if (serverName)
			status = freerdp_settings_set_string(settings, FreeRDP_ServerHostname, serverName);
	}

	const cJSON* azureMeta = cJSON_GetObjectItem(json, "azureInstanceNetworkMetadata");
	if (azureMeta && cJSON_IsString(azureMeta))
	{
		if (!arm_treat_azureInstanceNetworkMetadata(cJSON_GetStringValue(azureMeta), settings))
		{
			WLog_ERR(TAG, "error when treating azureInstanceNetworkMetadata");
		}
	}

	if (freerdp_settings_get_string(settings, FreeRDP_Password))
	{
		/* note: we retrieve some more fields for RDSTLS only if we have a password provided by the
		 * user, otherwise these are useless: we will not be able to do RDSTLS
		 */
		status = arm_fill_rdstls(arm, settings, json);
	}

	cJSON_Delete(json);
	return status;
}

static BOOL arm_handle_request_ok(rdpArm* arm, const HttpResponse* response)
{
	const size_t len = http_response_get_body_length(response);
	const char* msg = http_response_get_body(response);
	if (strnlen(msg, len + 1) > len)
		return FALSE;

	WLog_DBG(TAG, "Got HTTP Response data: %s", msg);
	return arm_fill_gateway_parameters(arm, msg, len);
}

static BOOL arm_handle_bad_request(rdpArm* arm, const HttpResponse* response, BOOL* retry)
{
	WINPR_ASSERT(response);
	WINPR_ASSERT(retry);

	*retry = FALSE;

	BOOL rc = FALSE;

	const size_t len = http_response_get_body_length(response);
	const char* msg = http_response_get_body(response);
	if (strnlen(msg, len + 1) > len)
		return FALSE;

	WLog_DBG(TAG, "Got HTTP Response data: %s", msg);

	cJSON* json = cJSON_ParseWithLength(msg, len);
	if (json == NULL)
	{
		const char* error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			WLog_ERR(TAG, "NullPoException: %s", error_ptr);
			return FALSE;
		}
	}

	const cJSON* gateway_code_str = cJSON_GetObjectItemCaseSensitive(json, "Code");
	if (!cJSON_IsString(gateway_code_str) || (gateway_code_str->valuestring == NULL))
	{
		WLog_ERR(TAG, "Response has no \"Code\" property");
		http_response_log_error_status(WLog_Get(TAG), WLOG_ERROR, response);
		goto fail;
	}

	if (strcmp(gateway_code_str->valuestring, "E_PROXY_ORCHESTRATION_LB_SESSIONHOST_DEALLOCATED") ==
	    0)
	{
		*retry = TRUE;
		WLog_DBG(TAG, "Starting your VM. It may take up to 5 minutes");
	}
	else
	{
		http_response_log_error_status(WLog_Get(TAG), WLOG_ERROR, response);
		goto fail;
	}

	rc = TRUE;
fail:
	cJSON_Delete(json);
	return rc;
}

static BOOL arm_handle_request(rdpArm* arm, BOOL* retry, DWORD timeout)
{
	WINPR_ASSERT(retry);

	*retry = FALSE;

	char* message = NULL;
	BOOL rc = FALSE;

	HttpResponse* response = NULL;
	long StatusCode;

	if (!http_context_set_uri(arm->http, "/api/arm/v2/connections/") ||
	    !http_context_set_accept(arm->http, "application/json") ||
	    !http_context_set_cache_control(arm->http, "no-cache") ||
	    !http_context_set_pragma(arm->http, "no-cache") ||
	    !http_context_set_connection(arm->http, "Keep-Alive") ||
	    !http_context_set_user_agent(arm->http, "FreeRDP/3.0") ||
	    !http_context_set_x_ms_user_agent(arm->http, "FreeRDP/3.0") ||
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

BOOL arm_resolve_endpoint(rdpContext* context, DWORD timeout)
{
#ifndef WITH_AAD
	WLog_ERR(TAG, "arm gateway support not compiled in");
	return FALSE;
#else

	if (!context)
		return FALSE;

	if (!context->settings)
		return FALSE;

	if ((freerdp_settings_get_uint32(context->settings, FreeRDP_LoadBalanceInfoLength) == 0) ||
	    (freerdp_settings_get_string(context->settings, FreeRDP_RemoteApplicationProgram) == NULL))
	{
		WLog_ERR(TAG, "loadBalanceInfo and RemoteApplicationProgram needed");
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
			const SSIZE_T delay = IFCALLRESULT(-1, instance->RetryDialog, instance, "arm-transport",
			                                   arm->gateway_retry, arm);
			arm->gateway_retry++;
			if (delay <= 0)
				break; /* error or no retry desired, abort loop */
			else
			{
				WLog_DBG(TAG, "Delay for %" PRIdz "ms before next attempt", delay);
				Sleep(delay);
			}
		}
		rc = arm_handle_request(arm, &retry, timeout);

	} while (retry && rc);
	arm_free(arm);
	return rc;
#endif
}
