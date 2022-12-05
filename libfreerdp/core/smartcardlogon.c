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
#include <winpr/crypto.h>
#include <winpr/path.h>

#include <freerdp/log.h>

#include <freerdp/utils/smartcardlogon.h>

#define TAG FREERDP_TAG("smartcardlogon")

struct SmartcardKeyInfo_st
{
	char* certPath;
	char* keyPath;
};

static void delete_file(char* path)
{
	if (!path)
		return;

	/* Overwrite data in files before deletion */
	{
		FILE* fp = winpr_fopen(path, "r+");
		if (fp)
		{
			const char buffer[8192] = { 0 };
			INT64 size = 0;
			int rs = _fseeki64(fp, 0, SEEK_END);
			if (rs == 0)
				size = _ftelli64(fp);
			_fseeki64(fp, 0, SEEK_SET);

			for (INT64 x = 0; x < size; x += sizeof(buffer))
			{
				fwrite(buffer, MIN(sizeof(buffer), (size_t)size - x), 1, fp);
			}

			fclose(fp);
		}
	}

	winpr_DeleteFile(path);
	free(path);
}

static void smartcardKeyInfo_Free(SmartcardKeyInfo* key_info)
{
	if (!key_info)
		return;

	delete_file(key_info->certPath);
	delete_file(key_info->keyPath);

	free(key_info);
}

void smartcardCertInfo_Free(SmartcardCertInfo* scCert)
{
	if (!scCert)
		return;

	free(scCert->csp);
	free(scCert->reader);
	crypto_cert_free(scCert->certificate);
	free(scCert->pkinitArgs);
	free(scCert->keyName);
	free(scCert->containerName);
	free(scCert->upn);
	free(scCert->userHint);
	free(scCert->domainHint);
	free(scCert->subject);
	free(scCert->issuer);
	smartcardKeyInfo_Free(scCert->key_info);

	free(scCert);
}

void smartcardCertList_Free(SmartcardCertInfo** cert_list, DWORD count)
{
	if (!cert_list)
		return;

	for (DWORD i = 0; i < count; i++)
	{
		SmartcardCertInfo* cert = cert_list[i];
		smartcardCertInfo_Free(cert);
	}

	free(cert_list);
}

static BOOL treat_sc_cert(SmartcardCertInfo* scCert)
{
	WINPR_ASSERT(scCert);

	scCert->upn = crypto_cert_get_upn(scCert->certificate->px509);
	if (!scCert->upn)
	{
		WLog_DBG(TAG, "%s has no UPN, trying emailAddress", scCert->keyName);
		scCert->upn = crypto_cert_get_email(scCert->certificate->px509);
	}

	if (scCert->upn)
	{
		size_t userLen;
		const char* atPos = strchr(scCert->upn, '@');

		if (!atPos)
		{
			WLog_ERR(TAG, "invalid UPN, for key %s (no @)", scCert->keyName);
			return FALSE;
		}

		userLen = (size_t)(atPos - scCert->upn);
		scCert->userHint = malloc(userLen + 1);
		scCert->domainHint = _strdup(atPos + 1);

		if (!scCert->userHint || !scCert->domainHint)
		{
			WLog_ERR(TAG, "error allocating userHint or domainHint, for key %s", scCert->keyName);
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

static BOOL list_provider_keys(const rdpSettings* settings, NCRYPT_PROV_HANDLE provider,
                               LPCWSTR csp, LPCWSTR scope, const char* userFilter,
                               const char* domainFilter, SmartcardCertInfo*** pcerts,
                               size_t* pcount)
{
	BOOL ret = FALSE;
	NCryptKeyName* keyName = NULL;
	PVOID enumState = NULL;
	SmartcardCertInfo** cert_list = *pcerts;
	size_t count = *pcount;

	while (NCryptEnumKeys(provider, scope, &keyName, &enumState, NCRYPT_SILENT_FLAG) ==
	       ERROR_SUCCESS)
	{
		NCRYPT_KEY_HANDLE phKey = 0;
		PBYTE certBytes = NULL;
		DWORD dwFlags = NCRYPT_SILENT_FLAG;
		DWORD cbOutput;
		SmartcardCertInfo* cert = NULL;
		BOOL haveError = TRUE;
		SECURITY_STATUS status;

		cert = calloc(1, sizeof(SmartcardCertInfo));
		if (!cert)
			goto out;

		cert->keyName = ConvertWCharToUtf8Alloc(keyName->pszName, NULL);
		if (!cert->keyName)
			goto endofloop;

		WLog_DBG(TAG, "opening key %s", cert->keyName);

		status =
		    NCryptOpenKey(provider, &phKey, keyName->pszName, keyName->dwLegacyKeySpec, dwFlags);
		if (status != ERROR_SUCCESS)
		{
			WLog_DBG(TAG,
			         "unable to NCryptOpenKey(dwLegacyKeySpec=0x%" PRIx32 " dwFlags=0x%" PRIx32
			         "), status=%s, skipping",
			         status, keyName->dwLegacyKeySpec, keyName->dwFlags,
			         winpr_NCryptSecurityStatusError(status));
			goto endofloop;
		}

		cert->csp = _wcsdup(csp);
		if (!cert->csp)
			goto endofloop;

#ifndef _WIN32
		status = NCryptGetProperty(phKey, NCRYPT_WINPR_SLOTID, (PBYTE)&cert->slotId, 4, &cbOutput,
		                           dwFlags);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve slotId for key %s, status=%s", cert->keyName,
			         winpr_NCryptSecurityStatusError(status));
			goto endofloop;
		}
#endif /* _WIN32 */

		/* ====== retrieve key's reader ====== */
		cbOutput = 0;
		status = NCryptGetProperty(phKey, NCRYPT_READER_PROPERTY, NULL, 0, &cbOutput, dwFlags);
		if (status != ERROR_SUCCESS)
		{
			WLog_DBG(TAG, "unable to retrieve reader's name length for key %s", cert->keyName);
			goto endofloop;
		}

		cert->reader = calloc(1, cbOutput + 2);
		if (!cert->reader)
		{
			WLog_ERR(TAG, "unable to allocate reader's name for key %s", cert->keyName);
			goto endofloop;
		}

		status = NCryptGetProperty(phKey, NCRYPT_READER_PROPERTY, (PBYTE)cert->reader, cbOutput + 2,
		                           &cbOutput, dwFlags);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve reader's name for key %s", cert->keyName);
			goto endofloop;
		}

		/* ====== retrieve key container name ====== */
		/* When using PKCS11, this will try to return what Windows would use for the key's name */
		cbOutput = 0;
		status = NCryptGetProperty(phKey, NCRYPT_NAME_PROPERTY, NULL, 0, &cbOutput, dwFlags);
		if (status == ERROR_SUCCESS)
		{
			cert->containerName = calloc(1, cbOutput + sizeof(WCHAR));
			if (!cert->containerName)
			{
				WLog_ERR(TAG, "unable to allocate key container name for key %s", cert->keyName);
				goto endofloop;
			}

			status = NCryptGetProperty(phKey, NCRYPT_NAME_PROPERTY, (BYTE*)cert->containerName,
			                           cbOutput, &cbOutput, dwFlags);
		}

		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve key container name for key %s", cert->keyName);
			goto endofloop;
		}

		/* ========= retrieve the certificate ===============*/
		cbOutput = 0;
		status = NCryptGetProperty(phKey, NCRYPT_CERTIFICATE_PROPERTY, NULL, 0, &cbOutput, dwFlags);
		if (status != ERROR_SUCCESS)
		{
			/* can happen that key don't have certificates */
			WLog_DBG(TAG, "unable to retrieve certificate property len, status=0x%lx, skipping",
			         status);
			goto endofloop;
		}

		certBytes = calloc(1, cbOutput);
		if (!certBytes)
		{
			WLog_ERR(TAG, "unable to allocate %" PRIu32 " certBytes for key %s", cbOutput,
			         cert->keyName);
			goto endofloop;
		}

		status = NCryptGetProperty(phKey, NCRYPT_CERTIFICATE_PROPERTY, certBytes, cbOutput,
		                           &cbOutput, dwFlags);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to retrieve certificate for key %s", cert->keyName);
			goto endofloop;
		}

		if (!winpr_Digest(WINPR_MD_SHA1, certBytes, cbOutput, cert->sha1Hash,
		                  sizeof(cert->sha1Hash)))
		{
			WLog_ERR(TAG, "unable to compute certificate sha1 for key %s", cert->keyName);
			goto endofloop;
		}

		cert->certificate = crypto_cert_read(certBytes, cbOutput);

		if (!cert->certificate)
		{
			WLog_ERR(TAG, "unable to parse X509 certificate for key %s", cert->keyName);
			goto endofloop;
		}

		if (!crypto_check_eku(cert->certificate->px509, NID_ms_smartcard_login))
		{
			WLog_DBG(TAG, "discarding certificate without Smartcard Login EKU for key %s",
			         cert->keyName);
			goto endofloop;
		}

		if (!treat_sc_cert(cert))
		{
			WLog_DBG(TAG, "error treating cert");
			goto endofloop;
		}

		if (userFilter && cert->userHint && strcmp(cert->userHint, userFilter) != 0)
		{
			WLog_DBG(TAG, "discarding non matching cert by user %s@%s", cert->userHint,
			         cert->domainHint);
			goto endofloop;
		}

		if (domainFilter && cert->domainHint && strcmp(cert->domainHint, domainFilter) != 0)
		{
			WLog_DBG(TAG, "discarding non matching cert by domain(%s) %s@%s", domainFilter,
			         cert->userHint, cert->domainHint);
			goto endofloop;
		}

#ifndef _WIN32
		if (!build_pkinit_args(settings, cert))
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
			smartcardCertInfo_Free(cert);
		else
		{
			SmartcardCertInfo** tmp;

			tmp = realloc(cert_list, sizeof(SmartcardCertInfo*) * (count + 1));
			if (!tmp)
			{
				WLog_ERR(TAG, "unable to reallocate certs");
				goto out;
			}
			cert_list = tmp;
			cert_list[count++] = cert;
		}
	}

	ret = TRUE;
out:
	*pcount = count;
	*pcerts = cert_list;
	NCryptFreeBuffer(enumState);
	return ret;
}

static BOOL smartcard_hw_enumerateCerts(const rdpSettings* settings, LPCWSTR csp,
                                        const char* reader, const char* userFilter,
                                        const char* domainFilter, SmartcardCertInfo*** scCerts,
                                        DWORD* retCount)
{
	BOOL ret = FALSE;
	LPWSTR scope = NULL;
	NCRYPT_PROV_HANDLE provider;
	SECURITY_STATUS status;
	size_t count = 0;
	SmartcardCertInfo** cert_list = NULL;
	const char* Pkcs11Module = freerdp_settings_get_string(settings, FreeRDP_Pkcs11Module);

	WINPR_ASSERT(scCerts);
	WINPR_ASSERT(retCount);

	if (reader)
	{
		size_t readerSz = strlen(reader);
		char* scopeStr = malloc(4 + readerSz + 1 + 1);
		if (!scopeStr)
			goto out;

		_snprintf(scopeStr, readerSz + 5, "\\\\.\\%s\\", reader);
		scope = ConvertUtf8NToWCharAlloc(scopeStr, readerSz + 5, NULL);
		free(scopeStr);

		if (!scope)
			goto out;
	}

	if (Pkcs11Module)
	{
		/* load a unique CSP by pkcs11 module path */
		LPCSTR paths[] = { Pkcs11Module, NULL };

		status = winpr_NCryptOpenStorageProviderEx(&provider, csp, 0, paths);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "unable to open provider given by pkcs11 module");
			goto out;
		}

		status = list_provider_keys(settings, provider, csp, scope, userFilter, domainFilter,
		                            &cert_list, &count);
		NCryptFreeObject((NCRYPT_HANDLE)provider);
		if (!status)
		{
			WLog_ERR(TAG, "error listing keys from CSP loaded from %s", Pkcs11Module);
			goto out;
		}
	}
	else
	{
		NCryptProviderName* names = NULL;
		DWORD nproviders, i;

		status = NCryptEnumStorageProviders(&nproviders, &names, NCRYPT_SILENT_FLAG);
		if (status != ERROR_SUCCESS)
		{
			WLog_ERR(TAG, "error listing providers");
			goto out;
		}

		for (i = 0; i < nproviders; i++)
		{
			char providerNameStr[256] = { 0 };
			const NCryptProviderName* name = &names[i];

			if (ConvertWCharToUtf8(name->pszName, providerNameStr, ARRAYSIZE(providerNameStr)) < 0)
			{
				_snprintf(providerNameStr, sizeof(providerNameStr), "<unknown>");
				WLog_ERR(TAG, "unable to convert provider name to char*, will show it as '%s'",
				         providerNameStr);
			}

			WLog_DBG(TAG, "exploring CSP '%s'", providerNameStr);
			if (csp && _wcscmp(name->pszName, csp) != 0)
			{
				WLog_DBG(TAG, "CSP '%s' filtered out", providerNameStr);
				continue;
			}

			status = NCryptOpenStorageProvider(&provider, name->pszName, 0);
			if (status != ERROR_SUCCESS)
				continue;

			if (!list_provider_keys(settings, provider, name->pszName, scope, userFilter,
			                        domainFilter, &cert_list, &count))
				WLog_INFO(TAG, "error when retrieving keys in CSP '%s'", providerNameStr);

			NCryptFreeObject((NCRYPT_HANDLE)provider);
		}

		NCryptFreeBuffer(names);
	}

	*scCerts = cert_list;
	*retCount = (DWORD)count;
	ret = TRUE;

out:
	if (!ret)
		smartcardCertList_Free(cert_list, count);
	free(scope);
	return ret;
}

static BOOL write_pem(const char* file, const char* pem)
{
	WINPR_ASSERT(file);
	WINPR_ASSERT(pem);

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

static SmartcardCertInfo* smartcardCertInfo_New(const char* privKeyPEM, const char* certPEM)
{
	WINPR_ASSERT(privKeyPEM);
	WINPR_ASSERT(certPEM);

	SmartcardCertInfo* cert = calloc(1, sizeof(SmartcardCertInfo));
	if (!cert)
		goto fail;

	SmartcardKeyInfo* info = cert->key_info = calloc(1, sizeof(SmartcardKeyInfo));
	if (!info)
		goto fail;

	cert->certificate = crypto_cert_pem_read(certPEM);
	if (!cert->certificate)
	{
		WLog_ERR(TAG, "unable to read smartcard certificate");
		goto fail;
	}

	if (!treat_sc_cert(cert))
	{
		WLog_ERR(TAG, "unable to treat smartcard certificate");
		goto fail;
	}

	cert->reader = ConvertUtf8ToWCharAlloc("FreeRDP Emulator", NULL);
	if (!cert->reader)
		goto fail;

	cert->containerName = ConvertUtf8ToWCharAlloc("Private Key 00", NULL);
	if (!cert->containerName)
		goto fail;

	/* compute PKINIT args FILE:<cert file>,<key file>
	 *
	 * We need files for PKINIT to read, so write the certificate to some
	 * temporary location and use that.
	 */
	info->keyPath = create_temporary_file();
	WLog_DBG(TAG, "writing PKINIT key to %s", info->keyPath);
	if (!write_pem(info->keyPath, privKeyPEM))
		goto fail;

	info->certPath = create_temporary_file();
	WLog_DBG(TAG, "writing PKINIT cert to %s", info->certPath);
	if (!write_pem(info->certPath, certPEM))
		goto fail;

	int res = allocating_sprintf(&cert->pkinitArgs, "FILE:%s,%s", info->certPath, info->keyPath);
	if (res <= 0)
		goto fail;

	return cert;
fail:
	smartcardCertInfo_Free(cert);
	return NULL;
}

static BOOL smartcard_sw_enumerateCerts(const rdpSettings* settings, SmartcardCertInfo*** scCerts,
                                        DWORD* retCount)
{
	BOOL rc = FALSE;
	SmartcardCertInfo** cert_list = NULL;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(scCerts);
	WINPR_ASSERT(retCount);

	const char* privKeyPEM = freerdp_settings_get_string(settings, FreeRDP_SmartcardPrivateKey);
	const char* certPEM = freerdp_settings_get_string(settings, FreeRDP_SmartcardCertificate);
	if (!privKeyPEM)
	{
		WLog_ERR(TAG, "Invalid smartcard private key PEM, aborting");
		goto out_error;
	}
	if (!certPEM)
	{
		WLog_ERR(TAG, "Invalid smartcard certificate PEM, aborting");
		goto out_error;
	}

	cert_list = calloc(1, sizeof(SmartcardCertInfo*));
	if (!cert_list)
		goto out_error;

	{
		SmartcardCertInfo* cert = smartcardCertInfo_New(privKeyPEM, certPEM);
		if (!cert)
			goto out_error;
		cert_list[0] = cert;
	}

	rc = TRUE;
	*scCerts = cert_list;
	*retCount = 1;

out_error:
	if (!rc)
		smartcardCertList_Free(cert_list, 1);
	return rc;
}

BOOL smartcard_enumerateCerts(const rdpSettings* settings, SmartcardCertInfo*** scCerts,
                              DWORD* retCount, BOOL gateway)
{
	BOOL ret;
	LPWSTR csp = NULL;
	const char* ReaderName = freerdp_settings_get_string(settings, FreeRDP_ReaderName);
	const char* CspName = freerdp_settings_get_string(settings, FreeRDP_CspName);
	const char* Username;
	const char* Domain;

	if (gateway)
	{
		Username = freerdp_settings_get_string(settings, FreeRDP_GatewayUsername);
		Domain = freerdp_settings_get_string(settings, FreeRDP_GatewayDomain);
	}
	else
	{
		Username = freerdp_settings_get_string(settings, FreeRDP_Username);
		Domain = freerdp_settings_get_string(settings, FreeRDP_Domain);
	}

	WINPR_ASSERT(settings);
	WINPR_ASSERT(scCerts);
	WINPR_ASSERT(retCount);

	if (Domain && !strlen(Domain))
		Domain = NULL;

	if (freerdp_settings_get_bool(settings, FreeRDP_SmartcardEmulation))
		return smartcard_sw_enumerateCerts(settings, scCerts, retCount);

	if (CspName && (!(csp = ConvertUtf8ToWCharAlloc(CspName, NULL))))
	{
		WLog_ERR(TAG, "error while converting CSP to WCHAR");
		return FALSE;
	}

	ret =
	    smartcard_hw_enumerateCerts(settings, csp, ReaderName, Username, Domain, scCerts, retCount);
	free(csp);
	return ret;
}

static BOOL set_settings_from_smartcard(rdpSettings* settings, size_t id, const char* value)
{
	WINPR_ASSERT(settings);

	if (!freerdp_settings_get_string(settings, id) && value)
		if (!freerdp_settings_set_string(settings, id, value))
			return FALSE;

	return TRUE;
}

BOOL smartcard_getCert(const rdpContext* context, SmartcardCertInfo** cert, BOOL gateway)
{
	WINPR_ASSERT(context);

	const freerdp* instance = context->instance;
	rdpSettings* settings = context->settings;
	SmartcardCertInfo** cert_list;
	DWORD count;
	size_t username_setting;
	size_t domain_setting;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(settings);

	if (!smartcard_enumerateCerts(settings, &cert_list, &count, gateway))
		return FALSE;

	if (count < 1)
	{
		WLog_ERR(TAG, "no suitable smartcard certificates were found");
		return FALSE;
	}

	if (count > 1)
	{
		DWORD index;

		if (!instance->ChooseSmartcard ||
		    !instance->ChooseSmartcard(cert_list, count, &index, gateway))
		{
			WLog_ERR(TAG, "more than one suitable smartcard certificate was found");
			smartcardCertList_Free(cert_list, count);
			return FALSE;
		}
		*cert = cert_list[index];

		for (DWORD i = 0; i < index; i++)
			smartcardCertInfo_Free(cert_list[i]);
		for (DWORD i = index + 1; i < count; i++)
			smartcardCertInfo_Free(cert_list[i]);
	}
	else
		*cert = cert_list[0];

	username_setting = gateway ? FreeRDP_GatewayUsername : FreeRDP_Username;
	domain_setting = gateway ? FreeRDP_GatewayDomain : FreeRDP_Domain;

	free(cert_list);

	if (!set_settings_from_smartcard(settings, username_setting, (*cert)->userHint) ||
	    !set_settings_from_smartcard(settings, domain_setting, (*cert)->domainHint))
	{
		WLog_ERR(TAG, "unable to set settings from smartcard!");
		smartcardCertInfo_Free(*cert);
		return FALSE;
	}

	return TRUE;
}
