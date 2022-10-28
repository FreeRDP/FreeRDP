/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Certificate Handling
 *
 * Copyright 2011 Jiten Pathy
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <winpr/crypto.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>

#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <freerdp/settings.h>

#include <freerdp/crypto/certificate.h>

struct rdp_certificate_store
{
	char* file;
	char* certs_path;
	char* server_path;
	const rdpSettings* settings;
};

struct rdp_certificate_data
{
	char* hostname;
	UINT16 port;
	char* subject;
	char* issuer;
	char* fingerprint;
	char* pem;
};

static const char certificate_store_dir[] = "certs";
static const char certificate_server_dir[] = "server";
static const char certificate_known_hosts_file[] = "known_hosts2";

#include <freerdp/log.h>
#include <freerdp/crypto/certificate.h>

#define TAG FREERDP_TAG("crypto")

static BOOL certificate_get_file_data(rdpCertificateStore* store, rdpCertificateData* data);
static BOOL duplicate(char** data, const char* value);

static HANDLE open_file(const char* name, DWORD dwDesiredAccess, DWORD dwShareMode,
                        DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes)
{
	HANDLE fp;
	if (!name)
		return INVALID_HANDLE_VALUE;
	WCHAR* wfile = ConvertUtf8ToWCharAlloc(name, NULL);
	if (!wfile)
		return INVALID_HANDLE_VALUE;

	fp = CreateFileW(wfile, dwDesiredAccess, 0, NULL, dwCreationDisposition, dwFlagsAndAttributes,
	                 NULL);
	free(wfile);
	return fp;
}
static rdpCertificateData* certificate_split_line(char* line);
static BOOL certificate_line_is_comment(const char* line, size_t length)
{
	while (length > 0)
	{
		switch (*line)
		{
			case ' ':
			case '\t':
				line++;
				length--;
				break;

			case '#':
				return TRUE;

			default:
				return FALSE;
		}
	}

	return TRUE;
}

static void certificate_store_uninit(rdpCertificateStore* certificate_store)
{
	if (certificate_store)
	{
		free(certificate_store->certs_path);
		certificate_store->certs_path = NULL;

		free(certificate_store->file);
		certificate_store->file = NULL;

		free(certificate_store->server_path);
		certificate_store->server_path = NULL;
	}
}

static BOOL ensure_path_exists(const char* path)
{
	BOOL res = FALSE;
	if (!path)
		return FALSE;
	/* Use wide character functions to allow proper unicode handling on windows */
	WCHAR* wpath = ConvertUtf8ToWCharAlloc(path, NULL);

	if (!wpath)
		return FALSE;

	if (!PathFileExistsW(wpath))
	{
		if (!PathMakePathW(wpath, 0))
		{
			WLog_ERR(TAG, "error creating directory '%s'", path);
			goto fail;
		}

		WLog_INFO(TAG, "creating directory %s", path);
	}
	res = TRUE;
fail:
	free(wpath);
	return res;
}

static BOOL certificate_store_init(rdpCertificateStore* certificate_store)
{
	const rdpSettings* settings;
	const char* ConfigPath;

	if (!certificate_store)
		return FALSE;

	settings = certificate_store->settings;
	if (!settings)
		return FALSE;

	ConfigPath = freerdp_settings_get_string(settings, FreeRDP_ConfigPath);
	if (!ConfigPath)
		return FALSE;

	WINPR_ASSERT(!certificate_store->certs_path);
	if (!(certificate_store->certs_path =
	          GetCombinedPath(ConfigPath, (const char*)certificate_store_dir)))
		goto fail;

	WINPR_ASSERT(!certificate_store->server_path);
	certificate_store->server_path =
	    GetCombinedPath(ConfigPath, (const char*)certificate_server_dir);
	if (!certificate_store->server_path)
		goto fail;

	WINPR_ASSERT(!certificate_store->file);
	if (!(certificate_store->file =
	          GetCombinedPath(ConfigPath, (const char*)certificate_known_hosts_file)))
		goto fail;
	PathCchConvertStyleA(certificate_store->file, strlen(certificate_store->file), PATH_STYLE_UNIX);

	if (!ensure_path_exists(ConfigPath) || !ensure_path_exists(certificate_store->certs_path) ||
	    !ensure_path_exists(certificate_store->server_path))
		goto fail;

	return TRUE;
fail:
	WLog_ERR(TAG, "certificate store initialization failed");
	certificate_store_uninit(certificate_store);
	return FALSE;
}

static int compare_pem(const char* current, const char* stored)
{
	int rc = 1;
	X509* xcur = NULL;
	X509* xstore = NULL;
	char* fpcur = NULL;
	char* fpstore = NULL;
	if (!current || !stored)
		goto fail;

	xcur = crypto_cert_from_pem(current, strlen(current), FALSE);
	xstore = crypto_cert_from_pem(stored, strlen(stored), FALSE);
	if (!xcur || !xstore)
		goto fail;

	fpcur = crypto_cert_fingerprint(xcur);
	fpstore = crypto_cert_fingerprint(xstore);
	if (!fpcur || !fpstore)
		goto fail;

	rc = strcmp(fpcur, fpstore);

fail:
	free(fpcur);
	free(fpstore);
	X509_free(xcur);
	X509_free(xstore);
	return rc;
}

static int certificate_data_match_raw(rdpCertificateStore* certificate_store,
                                      const rdpCertificateData* certificate_data, char** psubject,
                                      char** pissuer, char** fprint, char** ppem)
{
	BOOL found = FALSE;
	HANDLE fp;
	char* data = NULL;
	char* mdata = NULL;
	char* pline = NULL;
	int match = 1;
	DWORD lowSize, highSize;
	UINT64 size;

	DWORD read;

	fp = open_file(certificate_store->file, GENERIC_READ, FILE_SHARE_READ, OPEN_ALWAYS,
	               FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NORMAL);

	if (fp == INVALID_HANDLE_VALUE)
		return match;

	if ((lowSize = GetFileSize(fp, &highSize)) == INVALID_FILE_SIZE)
	{
		WLog_ERR(TAG, "GetFileSize(%s) returned %s [0x%08" PRIX32 "]", certificate_store->file,
		         strerror(errno), GetLastError());
		goto fail;
	}

	size = (UINT64)lowSize | ((UINT64)highSize << 32);

	if (size < 1)
		goto fail;

	mdata = (char*)malloc(size + 2);

	if (!mdata)
		goto fail;

	data = mdata;

	if (!ReadFile(fp, data, size, &read, NULL) || (read != size))
		goto fail;

	data[size] = '\n';
	data[size + 1] = '\0';
	pline = StrSep(&data, "\r\n");

	while ((pline != NULL) && !found)
	{
		size_t length = strlen(pline);
		rdpCertificateData* cert = NULL;

		if (length == 0)
			goto next;

		if (certificate_line_is_comment(pline, length))
			goto next;

		cert = certificate_split_line(pline);
		if (!cert)
		{
			WLog_WARN(TAG, "Invalid %s entry %s!", certificate_known_hosts_file, pline);
			goto next;
		}

		{
			const char* old = certificate_data_get_host(cert);
			const char* cur = certificate_data_get_host(certificate_data);

			if (strcmp(old, cur) != 0)
				goto next;
		}

		if (certificate_data_get_port(cert) != certificate_data_get_port(certificate_data))
			goto next;

		{
			const char* fingerprint = certificate_data_get_fingerprint(cert);
			const char* subject = certificate_data_get_subject(cert);
			const char* issuer = certificate_data_get_issuer(cert);
			const char* pem = certificate_data_get_pem(cert);
			const char* cur_fp = certificate_data_get_fingerprint(certificate_data);
			const char* cur_pem = certificate_data_get_pem(certificate_data);

			duplicate(psubject, subject);
			duplicate(pissuer, issuer);
			duplicate(fprint, fingerprint);
			duplicate(ppem, pem);

			match = -1;
			if (fingerprint && cur_fp)
				match = (strcmp(cur_fp, fingerprint) == 0) ? 0 : -1;

			if (cur_pem && pem)
				match = compare_pem(cur_pem, pem);

			found = TRUE;
		}

	next:
		certificate_data_free(cert);
		pline = StrSep(&data, "\r\n");
	}

fail:
	free(mdata);
	CloseHandle(fp);

	return match;
}

static WCHAR* certificate_get_cert_file_name(rdpCertificateStore* store,
                                             const rdpCertificateData* data)
{
	size_t x, offset = 0;
	char* pem = NULL;
	WCHAR* wpem = NULL;
	WINPR_DIGEST_CTX* ctx = NULL;
	BYTE hash[WINPR_SHA3_256_DIGEST_LENGTH] = { 0 };
	char fname[WINPR_SHA3_256_DIGEST_LENGTH * 2 + 6] = { 0 };

	if (!store || !store->server_path || !data || !data->hostname)
		goto fail;

	ctx = winpr_Digest_New();
	if (!ctx)
		goto fail;
	if (!winpr_Digest_Init(ctx, WINPR_MD_SHA256))
		goto fail;
	if (!winpr_Digest_Update(ctx, (const BYTE*)data->hostname, strlen(data->hostname)))
		goto fail;
	if (!winpr_Digest_Update(ctx, (const BYTE*)&data->port, sizeof(data->port)))
		goto fail;
	if (!winpr_Digest_Final(ctx, hash, sizeof(hash)))
		goto fail;

	for (x = 0; x < sizeof(hash); x++)
	{
		int rc = _snprintf(&fname[offset], sizeof(fname) - offset, "%02x", hash[x]);
		if (rc != 2)
			goto fail;
		offset += (size_t)rc;
	}
	_snprintf(&fname[offset], sizeof(fname) - offset, ".pem");
	pem = GetCombinedPath(store->server_path, fname);
	if (!pem)
		goto fail;

	wpem = ConvertUtf8ToWCharAlloc(pem, NULL);
fail:
	free(pem);
	winpr_Digest_Free(ctx);
	return wpem;
}

static BOOL duplicate(char** data, const char* value)
{
	char* tmp = NULL;
	if (!data)
		return FALSE;
	if (value)
		tmp = _strdup(value);

	free(*data);
	*data = tmp;
	if ((value == tmp) && (tmp == NULL))
		return TRUE;
	return tmp != NULL;
}

static rdpCertificateData* load_from_file(rdpCertificateStore* store, const char* hostname,
                                          UINT16 port)
{
	rdpCertificateData* loaded;
	if (!store || !hostname)
		return NULL;

	loaded = certificate_data_new(hostname, port);
	if (!loaded)
		return NULL;
	if (!certificate_get_file_data(store, loaded))
	{
		certificate_data_free(loaded);
		return NULL;
	}
	return loaded;
}

static BOOL certificate_get_file_data(rdpCertificateStore* store, rdpCertificateData* data)
{
	DWORD read;
	BOOL rc = FALSE;
	HANDLE fp = INVALID_HANDLE_VALUE;

	X509* certificate = NULL;
	LARGE_INTEGER position = { 0 };
	const LARGE_INTEGER offset = { 0 };
	WCHAR* fname = certificate_get_cert_file_name(store, data);
	if (!fname)
		goto fail;

	fp = CreateFileW(fname, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fp == INVALID_HANDLE_VALUE)
		goto fail;

	if (!SetFilePointerEx(fp, offset, &position, FILE_END))
		goto fail;
	if (!SetFilePointerEx(fp, offset, NULL, FILE_BEGIN))
		goto fail;

	{
		char* pem = realloc(data->pem, position.QuadPart + 1);
		if (!pem)
			goto fail;

		data->pem = pem;
	}

	if (!ReadFile(fp, data->pem, position.QuadPart, &read, NULL))
		goto fail;
	data->pem[read] = '\0';

	certificate = crypto_cert_from_pem(data->pem, read, FALSE);
	if (!certificate)
		goto fail;

	free(data->fingerprint);
	data->fingerprint = crypto_cert_fingerprint(certificate);
	if (!data->fingerprint)
		goto fail;
	free(data->issuer);
	data->issuer = crypto_cert_issuer(certificate);
	if (!data->issuer)
		goto fail;
	free(data->subject);
	data->subject = crypto_cert_subject(certificate);
	if (!data->subject)
		goto fail;

	rc = TRUE;

fail:
	CloseHandle(fp);
	X509_free(certificate);
	free(fname);
	return rc;
}

static int certificate_match_data_file(rdpCertificateStore* certificate_store,
                                       const rdpCertificateData* certificate_data)
{
	int rc = 1;
	const char* pem;
	const char* loaded_pem;

	rdpCertificateData* loaded =
	    load_from_file(certificate_store, certificate_data->hostname, certificate_data->port);
	if (!loaded)
		return 1;

	pem = certificate_data_get_pem(certificate_data);
	loaded_pem = certificate_data_get_pem(loaded);
	if (!pem)
	{
		const char* fp = certificate_data_get_fingerprint(certificate_data);
		const char* load_fp = certificate_data_get_fingerprint(loaded);
		if (fp)
		{
			if (strcmp(fp, load_fp) == 0)
				rc = 0;
			else
				rc = -1;
		}
	}
	else
	{
		rc = compare_pem(pem, loaded_pem);
	}

	certificate_data_free(loaded);
	return rc;
}

static BOOL useKnownHosts(rdpCertificateStore* certificate_store)
{
	BOOL use;
	WINPR_ASSERT(certificate_store);

	use = freerdp_settings_get_bool(certificate_store->settings, FreeRDP_CertificateUseKnownHosts);
	WLog_DBG(TAG, "known_hosts=%d", use);
	return use;
}

#define check_certificate_store_and_data(store, data) \
	check_certificate_store_and_data_((store), (data), __FILE__, __FUNCTION__, __LINE__)
static BOOL check_certificate_store_and_data_(const rdpCertificateStore* certificate_store,
                                              const rdpCertificateData* certificate_data,
                                              const char* file, const char* fkt, size_t line)
{
	if (!certificate_store)
	{
		WLog_ERR(TAG, "[%s, %s:%" PRIuz "] certificate_store=NULL", file, fkt, line);
		return FALSE;
	}
	if (!certificate_data)
	{
		WLog_ERR(TAG, "[%s, %s:%" PRIuz "] certificate_data=NULL", file, fkt, line);
		return FALSE;
	}
	return TRUE;
}

int certificate_store_contains_data(rdpCertificateStore* certificate_store,
                                    const rdpCertificateData* certificate_data)
{
	if (!check_certificate_store_and_data(certificate_store, certificate_data))
		return -1;
	if (useKnownHosts(certificate_store))
	{
		char* pem = NULL;
		int rc =
		    certificate_data_match_raw(certificate_store, certificate_data, NULL, NULL, NULL, &pem);

		/* Upgrade entry, append PEM */
		if ((rc == 0) && !pem && certificate_data->pem)
		{
			certificate_store_save_data(certificate_store, certificate_data);
		}
		free(pem);
		return rc;
	}
	else
		return certificate_match_data_file(certificate_store, certificate_data);
}

static char* decode(const char* value)
{
	size_t len, length;
	char* converted = NULL;
	if (!value)
		return NULL;
	len = strlen(value);
	crypto_base64_decode(value, len, (BYTE**)&converted, &length);
	return converted;
}

static char* encode(const char* value)
{
	size_t len;
	if (!value)
		return NULL;
	len = strlen(value);
	return (char*)crypto_base64_encode((const BYTE*)value, len);
}

static char* allocated_printf(const char* fmt, ...)
{
	int rc1, rc2;
	size_t size;
	va_list ap;
	char* buffer = NULL;

	va_start(ap, fmt);
	rc1 = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (rc1 <= 0)
		return NULL;

	size = (size_t)rc1;
	buffer = calloc(size + 2, sizeof(char));
	if (!buffer)
		return NULL;

	va_start(ap, fmt);
	rc2 = vsnprintf(buffer, size + 1, fmt, ap);
	va_end(ap);
	if (rc2 != rc1)
	{
		free(buffer);
		return NULL;
	}
	return buffer;
}

static char* certificate_data_get_host_file_entry(const rdpCertificateData* data)
{
	char* buffer = NULL;
	const char* hostname = certificate_data_get_host(data);
	const UINT16 port = certificate_data_get_port(data);
	char* subject = encode(certificate_data_get_subject(data));
	char* issuer = encode(certificate_data_get_issuer(data));
	const char* fingerprint = certificate_data_get_fingerprint(data);
	char* pem = encode(certificate_data_get_pem(data));

	/* Issuer and subject may be NULL */
	if (!data || !hostname || !fingerprint)
		goto fail;

	if (!subject)
		subject = _strdup("");
	if (!issuer)
		issuer = _strdup("");

	if (pem)
		buffer = allocated_printf("%s %" PRIu16 " %s %s %s %s\n", hostname, port, fingerprint,
		                          subject, issuer, pem);
	else
		buffer = allocated_printf("%s %" PRIu16 " %s %s %s\n", hostname, port, fingerprint, subject,
		                          issuer);
fail:
	free(subject);
	free(issuer);
	free(pem);
	return buffer;
}

static BOOL write_line_and_free(const char* filename, HANDLE fp, char* line)
{
	BOOL rc = FALSE;
	DWORD size, written;
	if ((fp == INVALID_HANDLE_VALUE) || !line)
		return FALSE;
	size = strlen(line);
	rc = WriteFile(fp, line, size, &written, NULL);

	if (!rc || (written != size))
	{
		WLog_ERR(TAG, "WriteFile(%s) returned %s [0x%08X]", filename, strerror(errno), errno);
		rc = FALSE;
	}

	free(line);
	return rc;
}

static BOOL certificate_data_replace_hosts_file(rdpCertificateStore* certificate_store,
                                                const rdpCertificateData* certificate_data,
                                                BOOL remove, BOOL append)
{
	BOOL newfile = TRUE;
	HANDLE fp = INVALID_HANDLE_VALUE;
	BOOL rc = FALSE;
	size_t length;
	char* data = NULL;
	char* sdata = NULL;
	char* pline = NULL;

	UINT64 size;
	DWORD read;
	DWORD lowSize, highSize;
	rdpCertificateData* copy = NULL;

	fp = open_file(certificate_store->file, GENERIC_READ | GENERIC_WRITE, 0, OPEN_ALWAYS,
	               FILE_ATTRIBUTE_NORMAL);

	if (fp == INVALID_HANDLE_VALUE)
	{
		WLog_ERR(TAG, "failed to open known_hosts file %s, aborting", certificate_store->file);
		return FALSE;
	}

	/* Create a copy, if a PEM was provided it will replace subject, issuer, fingerprint */
	copy = certificate_data_new(certificate_data->hostname, certificate_data->port);
	if (!copy)
		goto fail;

	if (certificate_data->pem)
	{
		if (!certificate_data_set_pem(copy, certificate_data->pem))
			goto fail;
	}
	else
	{
		if (!certificate_data_set_subject(copy, certificate_data->subject) ||
		    !certificate_data_set_issuer(copy, certificate_data->issuer) ||
		    !certificate_data_set_fingerprint(copy, certificate_data->fingerprint))
			goto fail;
	}

	if ((lowSize = GetFileSize(fp, &highSize)) == INVALID_FILE_SIZE)
	{
		WLog_ERR(TAG, "GetFileSize(%s) returned %s [0x%08" PRIX32 "]", certificate_store->file,
		         strerror(errno), GetLastError());
		goto fail;
	}

	size = (UINT64)lowSize | ((UINT64)highSize << 32);

	/* Newly created file, just write the new entry */
	if (size > 0)
		newfile = FALSE;

	if (newfile)
		goto fail;

	data = (char*)malloc(size + 2);

	if (!data)
		goto fail;

	if (!ReadFile(fp, data, size, &read, NULL) || (read != size))
		goto fail;

	if (SetFilePointer(fp, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
	{
		WLog_ERR(TAG, "SetFilePointer(%s) returned %s [0x%08" PRIX32 "]", certificate_store->file,
		         strerror(errno), GetLastError());
		goto fail;
	}

	if (!SetEndOfFile(fp))
	{
		WLog_ERR(TAG, "SetEndOfFile(%s) returned %s [0x%08" PRIX32 "]", certificate_store->file,
		         strerror(errno), GetLastError());
		goto fail;
	}

	/* Write the file back out, with appropriate fingerprint substitutions */
	data[size] = '\n';
	data[size + 1] = '\0';
	sdata = data;
	pline = StrSep(&sdata, "\r\n");

	while (pline != NULL)
	{
		rdpCertificateData* cert = NULL;
		length = strlen(pline);

		if (length > 0)
		{
			if (certificate_line_is_comment(pline, length))
			{
			}
			else if (!(cert = certificate_split_line(pline)))
				WLog_WARN(TAG, "Skipping invalid %s entry %s!", certificate_known_hosts_file,
				          pline);
			else
			{
				char* line;

				/* If this is the replaced hostname, use the updated fingerprint. */
				if ((strcmp(certificate_data_get_host(cert), certificate_data_get_host(copy)) ==
				     0) &&
				    (certificate_data_get_port(cert) == certificate_data_get_port(copy)))
				{
					rc = TRUE;
					if (remove && !append)
						goto next;

					line = certificate_data_get_host_file_entry(copy);
				}
				else
					line = certificate_data_get_host_file_entry(cert);

				if (!line)
					goto next;

				if (!write_line_and_free(certificate_store->file, fp, line))
					goto fail;
			}
		}
	next:
		certificate_data_free(cert);
		pline = StrSep(&sdata, "\r\n");
	}

fail:
	if (!rc && append)
	{
		if (fp != INVALID_HANDLE_VALUE)
		{
			char* line = certificate_data_get_host_file_entry(copy);
			if (line)
				rc = write_line_and_free(certificate_store->file, fp, line);
		}
	}

	CloseHandle(fp);
	free(data);
	certificate_data_free(copy);
	return rc;
}

static BOOL certificate_data_write_to_file(rdpCertificateStore* certificate_store,
                                           const rdpCertificateData* certificate_data)
{
	BOOL rc = FALSE;
	size_t len;
	HANDLE handle;
	DWORD numberOfBytesWritten;
	WCHAR* fname = certificate_get_cert_file_name(certificate_store, certificate_data);
	if (!fname)
		return FALSE;

	handle = CreateFileW(fname, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		goto fail;
	len = strlen(certificate_data->pem);
	rc = WriteFile(handle, certificate_data->pem, len, &numberOfBytesWritten, NULL);
	CloseHandle(handle);
fail:
	free(fname);
	return rc;
}

static BOOL certificate_data_remove_file(rdpCertificateStore* certificate_store,
                                         const rdpCertificateData* certificate_data)
{
	BOOL rc = FALSE;
	WCHAR* fname = certificate_get_cert_file_name(certificate_store, certificate_data);
	if (!fname)
		return FALSE;
	if (PathFileExistsW(fname))
		rc = DeleteFileW(fname);
	else
		rc = TRUE;
	free(fname);
	return rc;
}

BOOL certificate_store_remove_data(rdpCertificateStore* certificate_store,
                                   const rdpCertificateData* certificate_data)
{
	if (!check_certificate_store_and_data(certificate_store, certificate_data))
		return FALSE;
	if (useKnownHosts(certificate_store))
	{
		/* Ignore return, if the entry was invalid just continue */
		certificate_data_replace_hosts_file(certificate_store, certificate_data, TRUE, FALSE);
		return TRUE;
	}
	else
		return certificate_data_remove_file(certificate_store, certificate_data);
}

rdpCertificateData* certificate_split_line(char* line)
{
	char* cur;
	char* host = NULL;
	char* subject = NULL;
	char* issuer = NULL;
	char* fingerprint = NULL;
	char* pem = NULL;
	UINT16 port = 0;
	rdpCertificateData* data = NULL;

	size_t length = strlen(line);

	if (length <= 0)
		goto fail;

	cur = StrSep(&line, " \t");

	if (!cur)
		goto fail;

	host = cur;
	cur = StrSep(&line, " \t");

	if (!cur)
		goto fail;

	if (sscanf(cur, "%hu", &port) != 1)
		goto fail;

	cur = StrSep(&line, " \t");

	if (!cur)
		goto fail;

	fingerprint = cur;
	cur = StrSep(&line, " \t");

	if (!cur)
		goto fail;

	subject = cur;
	cur = StrSep(&line, " \t");

	if (!cur)
		goto fail;

	issuer = cur;

	/* Optional field */
	cur = StrSep(&line, " \t");
	if (cur)
		pem = cur;

	data = certificate_data_new(host, port);
	if (!data)
		goto fail;
	if (pem)
	{
		BOOL rc;
		char* dpem = NULL;
		size_t clength;
		crypto_base64_decode(pem, strlen(pem), (BYTE**)&dpem, &clength);
		rc = certificate_data_set_pem(data, dpem);
		free(dpem);
		if (!rc)
			goto fail;
	}
	else
	{
		BOOL rc;
		size_t clength;
		char* dsubject = NULL;
		char* dissuer = NULL;
		crypto_base64_decode(subject, strlen(subject), (BYTE**)&dsubject, &clength);
		crypto_base64_decode(issuer, strlen(issuer), (BYTE**)&dissuer, &clength);

		rc = certificate_data_set_subject(data, dsubject) &&
		     certificate_data_set_issuer(data, dissuer) &&
		     certificate_data_set_fingerprint(data, fingerprint);
		free(dsubject);
		free(dissuer);
		if (!rc)
			goto fail;
	}
	return data;
fail:
	certificate_data_free(data);
	return NULL;
}

static BOOL update_from_pem(rdpCertificateData* data)
{
	BOOL rc = FALSE;
	char* subject = NULL;
	char* issuer = NULL;
	char* fingerprint = NULL;
	X509* x1 = NULL;

	if (!data || !data->pem)
		return FALSE;
	x1 = crypto_cert_from_pem(data->pem, strlen(data->pem), FALSE);
	if (!x1)
		goto fail;

	/* Subject and issuer might be NULL */
	subject = crypto_cert_subject(x1);
	issuer = crypto_cert_issuer(x1);

	fingerprint = crypto_cert_fingerprint(x1);
	if (!fingerprint)
		goto fail;
	duplicate(&data->subject, subject);
	duplicate(&data->issuer, issuer);
	duplicate(&data->fingerprint, fingerprint);
	rc = TRUE;
fail:
	free(subject);
	free(issuer);
	free(fingerprint);
	X509_free(x1);
	return rc;
}

BOOL certificate_store_save_data(rdpCertificateStore* certificate_store,
                                 const rdpCertificateData* certificate_data)
{
	if (!check_certificate_store_and_data(certificate_store, certificate_data))
		return FALSE;
	if (useKnownHosts(certificate_store))
		return certificate_data_replace_hosts_file(certificate_store, certificate_data, TRUE, TRUE);
	else
		return certificate_data_write_to_file(certificate_store, certificate_data);
}

rdpCertificateData* certificate_store_load_data(rdpCertificateStore* certificate_store,
                                                const char* host, UINT16 port)
{
	if (useKnownHosts(certificate_store))
	{
		int rc;
		rdpCertificateData* data = certificate_data_new(host, port);
		if (!data)
			return NULL;

		rc = certificate_data_match_raw(certificate_store, data, &data->subject, &data->issuer,
		                                &data->fingerprint, &data->pem);
		if ((rc == 0) || (rc == -1))
			return data;
		certificate_data_free(data);
		return NULL;
	}
	else
	{
		return load_from_file(certificate_store, host, port);
	}
}

rdpCertificateData* certificate_data_new(const char* hostname, UINT16 port)
{
	size_t i;
	rdpCertificateData* certdata = NULL;

	if (!hostname)
		goto fail;

	certdata = (rdpCertificateData*)calloc(1, sizeof(rdpCertificateData));

	if (!certdata)
		goto fail;

	certdata->port = port;
	certdata->hostname = _strdup(hostname);
	if (!certdata->hostname)
		goto fail;
	for (i = 0; i < strlen(hostname); i++)
		certdata->hostname[i] = tolower(certdata->hostname[i]);

	return certdata;
fail:
	certificate_data_free(certdata);
	return NULL;
}

void certificate_data_free(rdpCertificateData* certificate_data)
{
	if (certificate_data != NULL)
	{
		free(certificate_data->hostname);
		free(certificate_data->subject);
		free(certificate_data->issuer);
		free(certificate_data->fingerprint);
		free(certificate_data->pem);
		free(certificate_data);
	}
}

const char* certificate_data_get_host(const rdpCertificateData* cert)
{
	if (!cert)
		return NULL;
	return cert->hostname;
}

UINT16 certificate_data_get_port(const rdpCertificateData* cert)
{
	if (!cert)
		return 0;
	return cert->port;
}

BOOL certificate_data_set_pem(rdpCertificateData* cert, const char* pem)
{
	if (!cert)
	{
		return FALSE;
	}
	if (!duplicate(&cert->pem, pem))
		return FALSE;
	if (!pem)
		return TRUE;

	return update_from_pem(cert);
}

BOOL certificate_data_set_subject(rdpCertificateData* cert, const char* subject)
{
	if (!cert)
		return FALSE;
	return duplicate(&cert->subject, subject);
}
BOOL certificate_data_set_issuer(rdpCertificateData* cert, const char* issuer)
{
	if (!cert)
		return FALSE;
	return duplicate(&cert->issuer, issuer);
}
BOOL certificate_data_set_fingerprint(rdpCertificateData* cert, const char* fingerprint)
{
	if (!cert)
		return FALSE;
	return duplicate(&cert->fingerprint, fingerprint);
}
const char* certificate_data_get_pem(const rdpCertificateData* cert)
{
	if (!cert)
		return NULL;
	return cert->pem;
}

const char* certificate_data_get_subject(const rdpCertificateData* cert)
{
	if (!cert)
		return NULL;
	return cert->subject;
}
const char* certificate_data_get_issuer(const rdpCertificateData* cert)
{
	if (!cert)
		return NULL;
	return cert->issuer;
}
const char* certificate_data_get_fingerprint(const rdpCertificateData* cert)
{
	if (!cert)
		return NULL;
	return cert->fingerprint;
}

rdpCertificateStore* certificate_store_new(const rdpSettings* settings)
{
	rdpCertificateStore* certificate_store =
	    (rdpCertificateStore*)calloc(1, sizeof(rdpCertificateStore));

	if (!certificate_store)
		return NULL;

	certificate_store->settings = settings;

	if (!certificate_store_init(certificate_store))
	{
		certificate_store_free(certificate_store);
		return NULL;
	}

	return certificate_store;
}

void certificate_store_free(rdpCertificateStore* certstore)
{
	certificate_store_uninit(certstore);
	free(certstore);
}

const char* certificate_store_get_hosts_file(const rdpCertificateStore* certificate_store)
{
	if (!certificate_store)
		return NULL;
	return certificate_store->file;
}

const char* certificate_store_get_certs_path(const rdpCertificateStore* certificate_store)
{
	if (!certificate_store)
		return NULL;
	return certificate_store->certs_path;
}

const char* certificate_store_get_hosts_path(const rdpCertificateStore* certificate_store)
{

	if (!certificate_store)
		return NULL;
	return certificate_store->server_path;
}
