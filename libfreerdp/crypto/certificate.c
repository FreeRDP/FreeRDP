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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

static const char certificate_store_dir[] = "certs";
static const char certificate_server_dir[] = "server";
static const char certificate_known_hosts_file[] = "known_hosts2";
static const char certificate_legacy_hosts_file[] = "known_hosts";

#include <freerdp/log.h>
#include <freerdp/crypto/certificate.h>

#define TAG FREERDP_TAG("crypto")

static BOOL certificate_split_line(char* line, char** host, UINT16* port,
					 char**subject, char**issuer,
					 char** fingerprint);

BOOL certificate_store_init(rdpCertificateStore* certificate_store)
{
	char* server_path = NULL;
	rdpSettings* settings;

	settings = certificate_store->settings;

	if (!PathFileExistsA(settings->ConfigPath))
	{
		if (!PathMakePathA(settings->ConfigPath, 0))
		{
			WLog_ERR(TAG, "error creating directory '%s'", settings->ConfigPath);
			goto fail;
		}
		WLog_INFO(TAG, "creating directory %s", settings->ConfigPath);
	}

	if (!(certificate_store->path = GetCombinedPath(settings->ConfigPath, (char*) certificate_store_dir)))
		goto fail;

	if (!PathFileExistsA(certificate_store->path))
	{
		if (!PathMakePathA(certificate_store->path, 0))
		{
			WLog_ERR(TAG, "error creating directory [%s]", certificate_store->path);
			goto fail;
		}
		WLog_INFO(TAG, "creating directory [%s]", certificate_store->path);
	}

	if (!(server_path = GetCombinedPath(settings->ConfigPath, (char*) certificate_server_dir)))
		goto fail;

	if (!PathFileExistsA(server_path))
	{
		if (!PathMakePathA(server_path, 0))
		{
			WLog_ERR(TAG, "error creating directory [%s]", server_path);
			goto fail;
		}
		WLog_INFO(TAG, "created directory [%s]", server_path);
	}

	if (!(certificate_store->file = GetCombinedPath(settings->ConfigPath, (char*) certificate_known_hosts_file)))
		goto fail;

	if (!(certificate_store->legacy_file = GetCombinedPath(settings->ConfigPath,
								(char*) certificate_legacy_hosts_file)))
		goto fail;

	free(server_path);

	return TRUE;

fail:
	WLog_ERR(TAG, "certificate store initialization failed");
	free(server_path);
	free(certificate_store->path);
	free(certificate_store->file);
	certificate_store->path = NULL;
	certificate_store->file = NULL;
	return FALSE;
}

static int certificate_data_match_legacy(rdpCertificateStore* certificate_store,
					 rdpCertificateData* certificate_data)
{
	FILE* fp;
	int match = 1;
	char* data;
	char* mdata;
	char* pline;
	char* hostname;
	long size;
	size_t length;

	fp = fopen(certificate_store->legacy_file, "rb");
	if (!fp)
		return match;

	if (fseek(fp, 0, SEEK_END) < 0)
	{
		WLog_ERR(TAG, "fseek(%s) returned %s [%08X]",
			 certificate_store->legacy_file, strerror(errno), errno);
		fclose(fp);
		return match;
	}
	if ((size = ftell(fp)) < 0)
	{
		WLog_ERR(TAG, "ftell(%s) returned %s [%08X]",
			 certificate_store->legacy_file, strerror(errno), errno);
		fclose(fp);
		return match;
	}
	if (fseek(fp, 0, SEEK_SET) < 0)
	{
		WLog_ERR(TAG, "fseek(%s) returned %s [%08X]",
			 certificate_store->legacy_file, strerror(errno), errno);
		fclose(fp);
		return match;
	}

	if (size < 1)
	{
		fclose(fp);
		return match;
	}

	mdata = (char*) malloc(size + 2);
	if (!mdata)
	{
		fclose(fp);
		return match;
	}

	data = mdata;
	if (fread(data, size, 1, fp) != 1)
	{
		free(data);
		fclose(fp);
		return match;
	}

	fclose(fp);

	data[size] = '\n';
	data[size + 1] = '\0';
	pline = StrSep(&data, "\r\n");

	while (pline != NULL)
	{
		length = strlen(pline);

		if (length > 0)
		{
			hostname = StrSep(&pline, " \t");
			if (!hostname || !pline)
				WLog_WARN(TAG, "Invalid %s entry %s %s!", certificate_legacy_hosts_file,
					hostname, pline);
			else if (strcmp(hostname, certificate_data->hostname) == 0)
			{
				match = strcmp(pline, certificate_data->fingerprint);
				break;
			}
		}

		pline = StrSep(&data, "\r\n");
	}

	/* Found a valid fingerprint in legacy file,
	 * copy to new file in new format. */
	if (0 == match)
	{
		rdpCertificateData* data = certificate_data_new(
						   hostname,
						   certificate_data->port,
						   NULL, NULL,
						   certificate_data->fingerprint);
		if (data)
		{
			free (data->subject);
			free (data->issuer);

			data->subject = NULL;
			data->issuer = NULL;
			if (certificate_data->subject)
			{
				data->subject = _strdup(certificate_data->subject);
				if (!data->subject)
					goto out;
			}
			if (certificate_data->issuer)
			{
				data->issuer = _strdup(certificate_data->issuer);
				if (!data->issuer)
					goto out;
			}
			match = certificate_data_print(certificate_store, data) ? 0 : 1;
		}
out:
		certificate_data_free(data);
	}
	free(mdata);

	return match;

}

static int certificate_data_match_raw(rdpCertificateStore* certificate_store,
							rdpCertificateData* certificate_data,
							char** psubject, char** pissuer,
							char** fprint)
{
	BOOL found = FALSE;
	FILE* fp;
	size_t length;
	char* data;
	char* mdata;
	char* pline;
	int match = 1;
	long int size;
	char* hostname = NULL;
	char* subject = NULL;
	char* issuer = NULL;
	char* fingerprint = NULL;
	unsigned short port = 0;

	fp = fopen(certificate_store->file, "rb");

	if (!fp)
		return match;

	if (fseek(fp, 0, SEEK_END) < 0)
	{
		WLog_ERR(TAG, "fseek(%s) returned %s [%08X]",
			 certificate_store->file, strerror(errno), errno);
		fclose(fp);
		return match;
	}
	if ((size = ftell(fp)) < 0)
	{
		WLog_ERR(TAG, "ftell(%s) returned %s [%08X]",
			 certificate_store->file, strerror(errno), errno);
		fclose(fp);
		return match;
	}
	if (fseek(fp, 0, SEEK_SET) < 0)
	{
		WLog_ERR(TAG, "fseek(%s) returned %s [%08X]",
			 certificate_store->file, strerror(errno), errno);
		fclose(fp);
		return match;
	}

	if (size < 1)
	{
		fclose(fp);
		return match;
	}

	mdata = (char*) malloc(size + 2);
	if (!mdata)
	{
		fclose(fp);
		return match;
	}

	data = mdata;
	if (fread(data, size, 1, fp) != 1)
	{
		fclose(fp);
		free(data);
		return match;
	}
	fclose(fp);

	data[size] = '\n';
	data[size + 1] = '\0';
	pline = StrSep(&data, "\r\n");

	while (pline != NULL)
	{
		length = strlen(pline);

		if (length > 0)
		{
			if (!certificate_split_line(pline, &hostname, &port,
								&subject, &issuer, &fingerprint))
				WLog_WARN(TAG, "Invalid %s entry %s!",
						certificate_known_hosts_file, pline);
			else if (strcmp(pline, certificate_data->hostname) == 0)
			{
				int outLen;

				if (port == certificate_data->port)
				{
					found = TRUE;
					match = (strcmp(certificate_data->fingerprint, fingerprint) == 0) ? 0 : -1;
					if (fingerprint && fprint)
						*fprint = _strdup(fingerprint);
					if (subject && psubject)
						crypto_base64_decode(subject, strlen(subject), (BYTE**)psubject, &outLen);
					if (issuer && pissuer)
						crypto_base64_decode(issuer, strlen(issuer), (BYTE**)pissuer, &outLen);
					break;
				}
			}
		}

		pline = StrSep(&data, "\r\n");
	}
	free(mdata);

	if ((match != 0) && !found)
		match = certificate_data_match_legacy(certificate_store, certificate_data);

	return match;
}

BOOL certificate_get_stored_data(rdpCertificateStore* certificate_store,
				 rdpCertificateData* certificate_data,
				 char** subject, char** issuer,
				 char** fingerprint)
{
	int rc = certificate_data_match_raw(certificate_store, certificate_data,
							subject, issuer, fingerprint);

	if ((rc == 0) || (rc == -1))
		return TRUE;
	return FALSE;
}

int certificate_data_match(rdpCertificateStore* certificate_store,
				 rdpCertificateData* certificate_data)
{
	return certificate_data_match_raw(certificate_store, certificate_data,
						NULL, NULL, NULL);
}

BOOL certificate_data_replace(rdpCertificateStore* certificate_store,
						rdpCertificateData* certificate_data)
{
	FILE* fp;
	BOOL rc = FALSE;
	size_t length;
	char* data;
	char* sdata;
	char* pline;
	long int size;

	fp = fopen(certificate_store->file, "rb");

	if (!fp)
		return FALSE;
	/* Read the current contents of the file. */
	if (fseek(fp, 0, SEEK_END) < 0)
	{
		WLog_ERR(TAG, "fseek(%s) returned %s [%08X]",
			 certificate_store->file, strerror(errno), errno);
		fclose(fp);
		return FALSE;
	}

	if ((size = ftell(fp)) < 0)
	{
		WLog_ERR(TAG, "ftell(%s) returned %s [%08X]",
			 certificate_store->file, strerror(errno), errno);
		fclose(fp);
		return FALSE;
	}

	if (fseek(fp, 0, SEEK_SET) < 0)
	{
		WLog_ERR(TAG, "fseek(%s) returned %s [%08X]",
			 certificate_store->file, strerror(errno), errno);
		fclose(fp);
		return FALSE;
	}

	data = (char*) malloc(size + 2);
	if (!data)
	{
		fclose(fp);
		return FALSE;
	}

	if (fread(data, size, 1, fp) != 1)
	{
		fclose(fp);
		free(data);
		return FALSE;
	}

	fclose(fp);

	fp = fopen(certificate_store->file, "wb");

	if (fp == NULL)
	{
		WLog_ERR(TAG, "freopen(%s) returned %s [%08X]",
			 certificate_store->file, strerror(errno), errno);
		free(data);
		return FALSE;
	}

	/* Write the file back out, with appropriate fingerprint substitutions */
	data[size] = '\n';
	data[size + 1] = '\0';
	sdata = data;
	pline = StrSep(&sdata, "\r\n");

	while (pline != NULL)
	{
		length = strlen(pline);

		if (length > 0)
		{
			UINT16 port = 0;
			char* hostname = NULL;
			char* fingerprint = NULL;
			char* subject = NULL;
			char* issuer = NULL;

			if (!certificate_split_line(pline, &hostname, &port, &subject, &issuer, &fingerprint))
				WLog_WARN(TAG, "Skipping invalid %s entry %s!",
						certificate_known_hosts_file, pline);
			else
			{
				/* If this is the replaced hostname, use the updated fingerprint. */
				if ((strcmp(hostname, certificate_data->hostname) == 0) &&
						(port == certificate_data->port))
				{
					fingerprint = certificate_data->fingerprint;
					rc = TRUE;
				}
				if (fprintf(fp, "%s %hu %s %s %s\n", hostname, port, fingerprint, subject, issuer) < 0)
				{
					WLog_ERR(TAG, "fprintf(%s) returned %s [%08X]",
						 certificate_store->file, strerror(errno), errno);
					fclose(fp);
					return FALSE;
				}
			}
		}

		pline = StrSep(&sdata, "\r\n");
	}

	if (fflush(fp) != 0)
	{
		WLog_WARN(TAG, "fflush(%s) returned %s [%08X]",
			 certificate_store->file, strerror(errno), errno);
	}

	fclose(fp);
	free(data);

	return rc;
}

BOOL certificate_split_line(char* line, char** host, UINT16* port, char** subject,
					char** issuer, char** fingerprint)
{
		char* cur;
		size_t length = strlen(line);

		if (length <= 0)
			return FALSE;

		cur = StrSep(&line, " \t");
		if (!cur)
			return FALSE;
		*host = cur;

		cur = StrSep(&line, " \t");
		if (!cur)
			return FALSE;

		if(sscanf(cur, "%hu", port) != 1)
			return FALSE;

		cur = StrSep(&line, " \t");
		if (!cur)
			return FALSE;

		*fingerprint = cur;

		cur = StrSep(&line, " \t");
		if (!cur)
			return FALSE;

		*subject = cur;

		cur = StrSep(&line, " \t");
		if (!cur)
			return FALSE;

		*issuer = cur;

		return TRUE;
}

BOOL certificate_data_print(rdpCertificateStore* certificate_store, rdpCertificateData* certificate_data)
{
	FILE* fp;

	/* reopen in append mode */
	fp = fopen(certificate_store->file, "ab");

	if (!fp)
		return FALSE;

	if (fseek(fp, 0, SEEK_END) < 0)
	{
		WLog_ERR(TAG, "fseek(%s) returned %s [%08X]",
			 certificate_store->file, strerror(errno), errno);
		fclose(fp);
		return FALSE;
	}

	if (fprintf(fp, "%s %hu %s %s %s\n", certificate_data->hostname, certificate_data->port,
					 certificate_data->fingerprint, certificate_data->subject,
					 certificate_data->issuer) < 0)
	{
		WLog_ERR(TAG, "fprintf(%s) returned %s [%08X]",
			 certificate_store->file, strerror(errno), errno);
		fclose(fp);
		return FALSE;
	}

	if (fflush(fp) != 0)
	{
		WLog_WARN(TAG, "fflush(%s) returned %s [%08X]",
			 certificate_store->file, strerror(errno), errno);
	}

	fclose(fp);

	return TRUE;
}

rdpCertificateData* certificate_data_new(char* hostname, UINT16 port, char* subject, char* issuer, char* fingerprint)
{
	size_t i;
	rdpCertificateData* certdata;

	if (!hostname)
		return NULL;

	if (!fingerprint)
		return NULL;

	certdata = (rdpCertificateData *)calloc(1, sizeof(rdpCertificateData));
	if (!certdata)
		return NULL;

	certdata->port = port;
	certdata->hostname = _strdup(hostname);
	if (subject)
		certdata->subject = crypto_base64_encode((BYTE*)subject, strlen(subject));
	else
		certdata->subject = crypto_base64_encode((BYTE*)"", 0);
	if (issuer)
		certdata->issuer = crypto_base64_encode((BYTE*)issuer, strlen(issuer));
	else
		certdata->issuer = crypto_base64_encode((BYTE*)"", 0);
	certdata->fingerprint = _strdup(fingerprint);

	if (!certdata->hostname || !certdata->subject ||
			!certdata->issuer || !certdata->fingerprint)
		goto fail;

	for (i=0; i<strlen(hostname); i++)
		certdata->hostname[i] = tolower(certdata->hostname[i]);

	return certdata;

fail:
	free(certdata->hostname);
	free(certdata->subject);
	free(certdata->issuer);
	free(certdata->fingerprint);
	free(certdata);
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
		free(certificate_data);
	}
}

rdpCertificateStore* certificate_store_new(rdpSettings* settings)
{
	rdpCertificateStore* certificate_store;

	certificate_store = (rdpCertificateStore*) calloc(1, sizeof(rdpCertificateStore));

	if (!certificate_store)
		return NULL;

	certificate_store->settings = settings;

	if (!certificate_store_init(certificate_store))
	{
		free(certificate_store);
		return NULL;
	}

	return certificate_store;
}

void certificate_store_free(rdpCertificateStore* certstore)
{
	if (certstore != NULL)
	{
		free(certstore->path);
		free(certstore->file);
		free(certstore->legacy_file);
		free(certstore);
	}
}
