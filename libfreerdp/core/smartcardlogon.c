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
#include <winpr/path.h>

#include <freerdp/log.h>

#include <freerdp/utils/smartcardlogon.h>

#define TAG FREERDP_TAG("smartcardlogon")

typedef struct
{
	SmartcardCertInfo info;
	char* certPath;
	char* keyPath;
} SmartcardCertInfoPrivate;

struct sSmartCardCerts
{
	size_t count;
	SmartcardCertInfoPrivate* certs;
};

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

static void smartcardCertInfo_Free(SmartcardCertInfo* scCert)
{
	const SmartcardCertInfo empty = { 0 };

	if (!scCert)
		return;
	free(scCert->reader);
	crypto_cert_free(scCert->certificate);
	free(scCert->pkinitArgs);
	free(scCert->containerName);
	free(scCert->upn);
	free(scCert->userHint);
	free(scCert->domainHint);
	free(scCert->subject);
	free(scCert->issuer);

	*scCert = empty;
}

static void delete_file(char* path)
{
	WCHAR* wpath = NULL;
	if (!path)
		return;

	/* Overwrite data in files before deletion */
	{
		FILE* fp = winpr_fopen(path, "r+");
		if (fp)
		{
			INT64 x, size = 0;
			int rs = _fseeki64(fp, 0, SEEK_END);
			if (rs == 0)
				size = _ftelli64(fp);
			_fseeki64(fp, 0, SEEK_SET);
			for (x = 0; x < size; x++)
				fputc(0, fp);
			fclose(fp);
		}
	}

	ConvertToUnicode(CP_UTF8, 0, path, -1, &wpath, 0);
	DeleteFileW(wpath);
	free(wpath);
	free(path);
}

static void smartcardCertInfoPrivate_Free(SmartcardCertInfoPrivate* scCert)
{
	const SmartcardCertInfoPrivate empty = { 0 };

	if (!scCert)
		return;
	smartcardCertInfo_Free(&scCert->info);
	delete_file(scCert->keyPath);
	delete_file(scCert->certPath);
	*scCert = empty;
}

void smartcardCerts_Free(SmartcardCerts* scCert)
{
	size_t x;
	if (!scCert)
		return;

	for (x = 0; x < scCert->count; x++)
		smartcardCertInfoPrivate_Free(&scCert->certs[x]);

	free(scCert);
}

static BOOL treat_sc_cert(SmartcardCertInfo* scCert)
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

		userLen = (size_t)(atPos - scCert->upn);
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

static int allocating_sprintf(char** dst, const char* fmt, ...)
{
	int rc;
	va_list ap;

	WINPR_ASSERT(dst);

	va_start(ap, fmt);
	rc = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	{
		char* tmp = realloc(*dst, rc + 1);
		if (!tmp)
			return -1;
		*dst = tmp;
	}
	va_start(ap, fmt);
	rc = vsnprintf(*dst, rc + 1, fmt, ap);
	va_end(ap);
	return rc;
}

#ifndef _WIN32
static BOOL build_pkinit_args(const rdpSettings* settings, SmartcardCertInfo* scCert)
{
	/* pkinit args only under windows
	 * 		PKCS11:module_name=opensc-pkcs11.so
	 */
	const char* Pkcs11Module = freerdp_settings_get_string(settings, FreeRDP_Pkcs11Module);
	const char* pkModule = Pkcs11Module ? Pkcs11Module : "opensc-pkcs11.so";

	if (allocating_sprintf(&scCert->pkinitArgs, "PKCS11:module_name=%s:slotid=%" PRIu16, pkModule,
	                       (UINT16)scCert->slotId) <= 0)
		return FALSE;
	return TRUE;
}
#endif /* _WIN32 */

static BOOL smartcard_hw_enumerateCerts(const rdpSettings* settings, LPCWSTR csp,
                                        const char* reader, const char* userFilter,
                                        SmartcardCerts** scCerts, DWORD* retCount)
{
	BOOL ret = FALSE;
	LPWSTR scope = NULL;
	PVOID enumState = NULL;
	NCRYPT_PROV_HANDLE provider;
	NCryptKeyName* keyName = NULL;
	SECURITY_STATUS status;
	size_t count = 0;
	SmartcardCerts* certs = NULL;
	const char* Pkcs11Module = freerdp_settings_get_string(settings, FreeRDP_Pkcs11Module);

	WINPR_ASSERT(csp);
	WINPR_ASSERT(scCerts);
	WINPR_ASSERT(retCount);

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

	if (Pkcs11Module)
	{
		LPCSTR paths[] = { Pkcs11Module, NULL };

		status = winpr_NCryptOpenStorageProviderEx(&provider, csp, 0, paths);
	}
	else
		status = NCryptOpenStorageProvider(&provider, csp, 0);

	if (status != ERROR_SUCCESS)
	{
		WLog_ERR(TAG, "unable to open provider");
		goto out;
	}

	while (NCryptEnumKeys(provider, scope, &keyName, &enumState, NCRYPT_SILENT_FLAG) ==
	       ERROR_SUCCESS)
	{
		NCRYPT_KEY_HANDLE phKey = 0;
		PBYTE certBytes = NULL;
		DWORD cbOutput;
		SmartcardCertInfoPrivate* cert;
		BOOL haveError = TRUE;

		count++;
		{
			SmartcardCerts* tmp =
			    realloc(certs, sizeof(SmartcardCerts) + (sizeof(SmartcardCertInfoPrivate) * count));
			if (!tmp)
				goto out;
			certs = tmp;
			certs->count = count;
			certs->certs = (SmartcardCertInfoPrivate*)(certs + 1);
		}

		cert = &certs->certs[count - 1];
		ZeroMemory(cert, sizeof(*cert));

		if (ConvertFromUnicode(CP_UTF8, 0, keyName->pszName, -1, &cert->info.containerName, 0, NULL,
		                       NULL) <= 0)
			goto endofloop;

		status = NCryptOpenKey(provider, &phKey, keyName->pszName, keyName->dwLegacyKeySpec,
		                       keyName->dwFlags);
		if (status != ERROR_SUCCESS)
			goto endofloop;

#ifndef _WIN32
		status = NCryptGetProperty(phKey, NCRYPT_WINPR_SLOTID, (PBYTE)&cert->info.slotId, 4,
		                           &cbOutput, NCRYPT_SILENT_FLAG);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve slotId for key %s", cert->info.containerName);
			goto endofloop;
		}
#endif /* _WIN32 */

		/* ====== retrieve key's reader ====== */
		status = NCryptGetProperty(phKey, NCRYPT_READER_PROPERTY, NULL, 0, &cbOutput,
		                           NCRYPT_SILENT_FLAG);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve reader's name length for key %s",
			         cert->info.containerName);
			goto endofloop;
		}

		cert->info.reader = calloc(1, cbOutput + 2);
		if (!cert->info.reader)
		{
			WLog_ERR(TAG, "unable to allocate reader's name for key %s", cert->info.containerName);
			goto endofloop;
		}

		status = NCryptGetProperty(phKey, NCRYPT_READER_PROPERTY, (PBYTE)cert->info.reader,
		                           cbOutput + 2, &cbOutput, NCRYPT_SILENT_FLAG);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve reader's name for key %s", cert->info.containerName);
			goto endofloop;
		}

		if (!getAtr(cert->info.reader, cert->info.atr, &cert->info.atrLength))
		{
			WLog_ERR(TAG, "unable to retrieve card ATR for key %s", cert->info.containerName);
			goto endofloop;
		}

		/* ========= retrieve the certificate ===============*/
		status = NCryptGetProperty(phKey, NCRYPT_CERTIFICATE_PROPERTY, NULL, 0, &cbOutput,
		                           NCRYPT_SILENT_FLAG);
		if (status != ERROR_SUCCESS)
		{
			/* can happen that key don't have certificates */
			goto endofloop;
		}

		certBytes = calloc(1, cbOutput);
		if (!certBytes)
		{
			WLog_ERR(TAG, "unable to allocate %d certBytes for key %s", cbOutput,
			         cert->info.containerName);
			goto endofloop;
		}

		status = NCryptGetProperty(phKey, NCRYPT_CERTIFICATE_PROPERTY, certBytes, cbOutput,
		                           &cbOutput, NCRYPT_SILENT_FLAG);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve certificate for key %s", cert->info.containerName);
			goto endofloop;
		}

		if (!winpr_Digest(WINPR_MD_SHA1, certBytes, cbOutput, cert->info.sha1Hash,
		                  sizeof(cert->info.sha1Hash)))
		{
			WLog_ERR(TAG, "unable to compute certificate sha1 for key %s",
			         cert->info.containerName);
			goto endofloop;
		}

		cert->info.certificate = crypto_cert_read(certBytes, cbOutput);

		if (!cert->info.certificate)
		{
			WLog_ERR(TAG, "unable to parse X509 certificate for key %s", cert->info.containerName);
			goto endofloop;
		}

		if (!treat_sc_cert(&cert->info))
			goto endofloop;

		if (userFilter && cert->info.userHint && strcmp(cert->info.userHint, userFilter) != 0)
		{
			goto endofloop;
		}

#ifndef _WIN32
		if (!build_pkinit_args(settings, &cert->info))
		{
			WLog_ERR(TAG, "error build pkinit args");
			goto endofloop;
		}
#endif
		haveError = FALSE;

	endofloop:
		free(certBytes);
		NCryptFreeBuffer(keyName);
		if (phKey)
			NCryptFreeObject((NCRYPT_HANDLE)phKey);

		if (haveError)
		{
			smartcardCertInfoPrivate_Free(cert);
			count--;
		}
	}

	*scCerts = certs;
	*retCount = (DWORD)count;
	ret = TRUE;

	NCryptFreeBuffer(enumState);
	NCryptFreeObject((NCRYPT_HANDLE)provider);
out:
	if (!ret)
		smartcardCerts_Free(certs);
	free(scope);
	return ret;
}

static BOOL write_pem(const char* file, const char* pem)
{
	size_t rc, size = strlen(pem) + 1;
	FILE* fp = winpr_fopen(file, "w");
	if (!fp)
		return FALSE;
	rc = fwrite(pem, 1, size, fp);
	fclose(fp);
	return rc == size;
}

static char* create_temporary_file(void)
{
	BYTE buffer[32];
	char* hex;
	char* path;

	winpr_RAND(buffer, sizeof(buffer));
	hex = winpr_BinToHexString(buffer, sizeof(buffer), FALSE);
	path = GetKnownSubPath(KNOWN_PATH_TEMP, hex);
	free(hex);
	return path;
}

static BOOL smartcard_sw_enumerateCerts(const rdpSettings* settings, SmartcardCerts** scCerts,
                                        DWORD* retCount)
{
	BOOL rc = FALSE;
	int res;
	SmartcardCerts* certs = NULL;
	SmartcardCertInfoPrivate* cert;
	const size_t count = 1;
	char* keyPath = create_temporary_file();
	char* certPath = create_temporary_file();

	WINPR_ASSERT(settings);
	WINPR_ASSERT(scCerts);
	WINPR_ASSERT(retCount);

	certs = calloc(count, sizeof(SmartcardCertInfoPrivate) + sizeof(SmartcardCerts));
	if (!certs)
		goto out_error;

	certs->count = count;
	cert = certs->certs = (SmartcardCertInfoPrivate*)(certs + 1);

	cert->info.certificate =
	    crypto_cert_pem_read(freerdp_settings_get_string(settings, FreeRDP_SmartcardCertificate));
	if (!cert->info.certificate)
	{
		WLog_ERR(TAG, "unable to read smartcard certificate");
		goto out_error;
	}

	if (!treat_sc_cert(&cert->info))
	{
		WLog_ERR(TAG, "unable to treat smartcard certificate");
		goto out_error;
	}

	if (ConvertToUnicode(CP_UTF8, 0, "FreeRDP Emulator", -1, &cert->info.reader, 0) < 0)
		goto out_error;

	cert->info.containerName = strdup("Private Key 00");
	if (!cert->info.containerName)
		goto out_error;

	/* compute PKINIT args FILE:<cert file>,<key file>
	 *
	 * We need files for PKINIT to read, so write the certificate to some
	 * temporary location and use that.
	 */
	if (!write_pem(keyPath, freerdp_settings_get_string(settings, FreeRDP_SmartcardPrivateKey)))
		goto out_error;
	if (!write_pem(certPath, freerdp_settings_get_string(settings, FreeRDP_SmartcardCertificate)))
		goto out_error;
	res = allocating_sprintf(&cert->info.pkinitArgs, "FILE:%s,%s", certPath, keyPath);
	if (res <= 0)
		goto out_error;

	cert->certPath = certPath;
	cert->keyPath = keyPath;

	rc = TRUE;
	*scCerts = certs;
	*retCount = (DWORD)certs->count;

out_error:
	if (!rc)
		smartcardCerts_Free(certs);
	return rc;
}

BOOL smartcard_enumerateCerts(const rdpSettings* settings, SmartcardCerts** scCerts,
                              DWORD* retCount)
{
	BOOL ret;
	LPWSTR csp;
	const char* asciiCsp;
	const char* ReaderName = freerdp_settings_get_string(settings, FreeRDP_ReaderName);
	const char* Username = freerdp_settings_get_string(settings, FreeRDP_Username);
	const char* CspName = freerdp_settings_get_string(settings, FreeRDP_CspName);

	WINPR_ASSERT(settings);
	WINPR_ASSERT(scCerts);
	WINPR_ASSERT(retCount);

	asciiCsp = CspName ? CspName : MS_SCARD_PROV_A;

	if (freerdp_settings_get_bool(settings, FreeRDP_SmartcardEmulation))
		return smartcard_sw_enumerateCerts(settings, scCerts, retCount);

	if (ConvertToUnicode(CP_UTF8, 0, asciiCsp, -1, &csp, 0) <= 0)
	{
		WLog_ERR(TAG, "error while converting CSP to WCHAR");
		return FALSE;
	}

	ret = smartcard_hw_enumerateCerts(settings, csp, ReaderName, Username, scCerts, retCount);
	free(csp);
	return ret;
}

const SmartcardCertInfo* smartcard_getCertInfo(SmartcardCerts* scCerts, DWORD index)
{
	WINPR_ASSERT(scCerts);
	if (index >= scCerts->count)
		return NULL;

	return &scCerts->certs[index].info;
}
