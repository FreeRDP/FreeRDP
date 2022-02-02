/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Logging in with smartcards
 *
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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
#include <string.h>

#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/objects.h>

#include <winpr/error.h>
#include <winpr/ncrypt.h>
#include <winpr/string.h>
#include <winpr/wlog.h>
#include <winpr/smartcard.h>
#include <winpr/crypto.h>

#include <freerdp/log.h>

#include "smartcardlogon.h"

#define TAG FREERDP_TAG("smartcardlogon")

static BOOL getAtr(LPWSTR readerName, BYTE* atr, DWORD* atrLen)
{
	WCHAR atrName[256];
	DWORD cbLength;
	DWORD dwShareMode = SCARD_SHARE_SHARED;
	DWORD dwPreferredProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;
	SCARDHANDLE hCardHandle;
	DWORD dwActiveProtocol = 0;
	BOOL ret = FALSE;
	LONG status = 0;
	SCARDCONTEXT scContext;

	status = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &scContext);
	if (status != ERROR_SUCCESS || !scContext)
		return FALSE;

	status = SCardConnectW(scContext, readerName, dwShareMode, dwPreferredProtocols, &hCardHandle,
	                       &dwActiveProtocol);
	if (status != ERROR_SUCCESS)
		goto out_connect;

	*atrLen = 256;
	status = SCardGetAttrib(hCardHandle, SCARD_ATTR_ATR_STRING, atr, atrLen);
	if (status != ERROR_SUCCESS)
		goto out_get_attrib;

	cbLength = 256;
	status = SCardListCardsW(scContext, atr, NULL, 0, atrName, &cbLength);
	if (status != ERROR_SUCCESS)
		goto out_listCards;

	/* WLog_DBG(TAG, "ATR name: %ld -> %S\n", cbLength, atrName); */
	ret = TRUE;
out_listCards:
out_get_attrib:
	SCardDisconnect(scContext, SCARD_LEAVE_CARD);
out_connect:
	SCardReleaseContext(scContext);
	return ret;
}

void smartcardCert_Free(SmartcardCert* scCert)
{
	free(scCert->reader);
	crypto_cert_free(scCert->certificate);
	free(scCert->pkinitArgs);
	free(scCert->containerName);
	free(scCert->upn);
	free(scCert->userHint);
	free(scCert->domainHint);
	free(scCert->subject);
	free(scCert->issuer);

	ZeroMemory(scCert, sizeof(*scCert));
}

static BOOL treat_sc_cert(SmartcardCert* scCert)
{
	scCert->upn = crypto_cert_get_upn(scCert->certificate->px509);
	if (scCert->upn)
	{
		size_t userLen;
		const char* atPos = strchr(scCert->upn, '@');

		if (!atPos)
		{
			WLog_ERR(TAG, "invalid UPN, for key %s (no @)", scCert->containerName);
			return FALSE;
		}

		userLen = (atPos - scCert->upn);
		scCert->userHint = malloc(userLen + 1);
		scCert->domainHint = strdup(atPos + 1);

		if (!scCert->userHint || !scCert->domainHint)
		{
			WLog_ERR(TAG, "error allocating userHint or domainHint, for key %s",
			         scCert->containerName);
			return FALSE;
		}

		memcpy(scCert->userHint, scCert->upn, userLen);
		scCert->userHint[userLen] = 0;
	}

	scCert->subject = crypto_cert_subject(scCert->certificate->px509);
	scCert->issuer = crypto_cert_issuer(scCert->certificate->px509);
	return TRUE;
}

#ifndef _WIN32
static BOOL build_pkinit_args(rdpSettings* settings, SmartcardCert* scCert)
{
	/* pkinit args only under windows
	 * 		PKCS11:module_name=opensc-pkcs11.so
	 */
	size_t sz = strlen("PKCS11:module_name=:slotid=XXXXX");
	const char* pkModule = settings->Pkcs11Module ? settings->Pkcs11Module : "opensc-pkcs11.so";

	sz += strlen(pkModule) + 1;

	scCert->pkinitArgs = malloc(sz);
	if (!scCert->pkinitArgs)
		return FALSE;

	_snprintf(scCert->pkinitArgs, sz, "PKCS11:module_name=%s:slotid=%" PRIu16, pkModule,
	          (UINT16)scCert->slotId);

	return TRUE;
}
#endif /* _WIN32 */

static BOOL smartcard_hw_enumerateCerts(rdpSettings* settings, LPCWSTR csp, const char* reader,
                                        const char* userFilter, SmartcardCert* scCert, DWORD count,
                                        DWORD* retCount)
{
	BOOL ret = FALSE;
	LPWSTR scope = NULL;
	PVOID enumState = NULL;
	NCRYPT_PROV_HANDLE provider;
	NCryptKeyName* keyName = NULL;
	SECURITY_STATUS status;

	if (reader)
	{
		int res;
		size_t readerSz = strlen(reader);
		char* scopeStr = malloc(4 + readerSz + 1 + 1);
		if (!scopeStr)
			goto out;

		_snprintf(scopeStr, readerSz + 5, "\\\\.\\%s\\", reader);
		res = ConvertToUnicode(CP_UTF8, 0, scopeStr, -1, &scope, 0);
		free(scopeStr);

		if (res <= 0)
			goto out;
	}

	status = NCryptOpenStorageProvider(&provider, csp, 0);
	if (status != ERROR_SUCCESS)
	{
		WLog_ERR(TAG, "unable to open provider");
		goto out;
	}

	*retCount = 0;
	while ((*retCount < count) && (status = NCryptEnumKeys(provider, scope, &keyName, &enumState,
	                                                       NCRYPT_SILENT_FLAG)) == ERROR_SUCCESS)
	{
		NCRYPT_KEY_HANDLE phKey;
		PBYTE certBytes = NULL;
		DWORD cbOutput;

		if (ConvertFromUnicode(CP_UTF8, 0, keyName->pszName, -1, &scCert->containerName, 0, NULL,
		                       NULL) <= 0)
			continue;

		status = NCryptOpenKey(provider, &phKey, keyName->pszName, keyName->dwLegacyKeySpec,
		                       keyName->dwFlags);
		if (status != ERROR_SUCCESS)
		{
			smartcardCert_Free(scCert);
			continue;
		}

#ifndef _WIN32
		status = NCryptGetProperty(phKey, NCRYPT_WINPR_SLOTID, (PBYTE)&scCert->slotId, 4, &cbOutput,
		                           NCRYPT_SILENT_FLAG);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve slotId for key %s", scCert->containerName);
			smartcardCert_Free(scCert);
			goto endofloop;
		}
#endif /* _WIN32 */

		/* ====== retrieve key's reader ====== */
		status = NCryptGetProperty(phKey, NCRYPT_READER_PROPERTY, NULL, 0, &cbOutput,
		                           NCRYPT_SILENT_FLAG);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve reader's name length for key %s",
			         scCert->containerName);
			smartcardCert_Free(scCert);
			goto endofloop;
		}

		scCert->reader = calloc(1, cbOutput + 2);
		if (!scCert->reader)
		{
			WLog_ERR(TAG, "unable to allocate reader's name for key %s", scCert->containerName);
			smartcardCert_Free(scCert);
			goto endofloop;
		}

		status = NCryptGetProperty(phKey, NCRYPT_READER_PROPERTY, (PBYTE)scCert->reader,
		                           cbOutput + 2, &cbOutput, NCRYPT_SILENT_FLAG);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve reader's name for key %s", scCert->containerName);
			smartcardCert_Free(scCert);
			goto endofloop;
		}

		if (!getAtr(scCert->reader, scCert->atr, &scCert->atrLength))
		{
			WLog_ERR(TAG, "unable to retrieve card ATR for key %s", scCert->containerName);
			smartcardCert_Free(scCert);
			goto endofloop;
		}

		/* ========= retrieve the certificate ===============*/
		status = NCryptGetProperty(phKey, NCRYPT_CERTIFICATE_PROPERTY, NULL, 0, &cbOutput,
		                           NCRYPT_SILENT_FLAG);
		if (status != ERROR_SUCCESS)
		{
			/* can happen that key don't have certificates */
			smartcardCert_Free(scCert);
			goto endofloop;
		}

		certBytes = calloc(1, cbOutput);
		if (!certBytes)
		{
			WLog_ERR(TAG, "unable to allocate %d certBytes for key %s", cbOutput,
			         scCert->containerName);
			smartcardCert_Free(scCert);
			goto endofloop;
		}

		status = NCryptGetProperty(phKey, NCRYPT_CERTIFICATE_PROPERTY, certBytes, cbOutput,
		                           &cbOutput, NCRYPT_SILENT_FLAG);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve certificate for key %s", scCert->containerName);
			free(certBytes);
			smartcardCert_Free(scCert);
			goto endofloop;
		}

		if (!winpr_Digest(WINPR_MD_SHA1, certBytes, cbOutput, scCert->sha1Hash,
		                  sizeof(scCert->sha1Hash)))
		{
			WLog_ERR(TAG, "unable to compute certificate sha1 for key %s", scCert->containerName);
			free(certBytes);
			smartcardCert_Free(scCert);
			goto endofloop;
		}

		scCert->certificate = crypto_cert_read(certBytes, cbOutput);
		free(certBytes);

		if (!scCert->certificate)
		{
			WLog_ERR(TAG, "unable to parse X509 certificate for key %s", scCert->containerName);
			smartcardCert_Free(scCert);
			goto endofloop;
		}

		if (!treat_sc_cert(scCert))
		{
			smartcardCert_Free(scCert);
			goto endofloop;
		}

		if (userFilter && scCert->userHint && strcmp(scCert->userHint, userFilter) != 0)
		{
			smartcardCert_Free(scCert);
			goto endofloop;
		}

#ifndef _WIN32
		if (!build_pkinit_args(settings, scCert))
		{
			WLog_ERR(TAG, "error build pkinit args");
			smartcardCert_Free(scCert);
			goto endofloop;
		}
#endif

		++*retCount;
		scCert++;

	endofloop:
		NCryptFreeObject((NCRYPT_HANDLE)phKey);
	}

	ret = TRUE;

	NCryptFreeObject((NCRYPT_HANDLE)provider);
out:
	free(scope);
	return ret;
}

static BOOL smartcard_sw_enumerateCerts(rdpSettings* settings, SmartcardCert* scCert, DWORD count,
                                        DWORD* retCount)
{
	size_t sz;

	if (count < 1)
		return FALSE;

	if (!settings->SmartcardCertificateFile || !settings->SmartcardPrivateKeyFile)
	{
		WLog_ERR(TAG, "missing smartcard emulation cert or key");
		return FALSE;
	}

	/* compute PKINIT args FILE:<cert file>,<key file> */
	sz = strlen("FILE:") + strlen(settings->SmartcardCertificateFile) + 1 +
	     strlen(settings->SmartcardPrivateKeyFile) + 1;
	scCert->pkinitArgs = malloc(sz);
	_snprintf(scCert->pkinitArgs, sz, "FILE:%s,%s", settings->SmartcardCertificateFile,
	          settings->SmartcardPrivateKeyFile);

	scCert->certificate = crypto_cert_pem_read(settings->SmartcardCertificate);
	if (!scCert->certificate)
	{
		WLog_ERR(TAG, "unable to read smartcard certificate");
		goto out_error;
	}

	if (!treat_sc_cert(scCert))
	{
		WLog_ERR(TAG, "unable to treat smartcard certificate");
		goto out_error;
	}

	if (ConvertToUnicode(CP_UTF8, 0, "FreeRDP Emulator", -1, &scCert->reader, 0) < 0)
		goto out_error;

	scCert->containerName = strdup("Private Key 00");
	if (!scCert->containerName)
		goto out_error;

	*retCount = 1;
	return TRUE;

out_error:
	smartcardCert_Free(scCert);
	return FALSE;
}

BOOL smartcard_enumerateCerts(rdpSettings* settings, SmartcardCert* scCert, DWORD count,
                              DWORD* retCount)
{
	BOOL ret;
	LPWSTR csp;
	const char* asciiCsp = settings->CspName ? settings->CspName : MS_SCARD_PROV_A;

	if (settings->SmartcardEmulation)
		return smartcard_sw_enumerateCerts(settings, scCert, count, retCount);

	if (ConvertToUnicode(CP_UTF8, 0, asciiCsp, -1, &csp, 0) <= 0)
	{
		WLog_ERR(TAG, "error while converting CSP to WCHAR");
		return FALSE;
	}

	ret = smartcard_hw_enumerateCerts(settings, csp, settings->ReaderName, settings->Username,
	                                  scCert, count, retCount);
	free(csp);
	return ret;
}
