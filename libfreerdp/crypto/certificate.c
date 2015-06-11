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

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>

#include <openssl/pem.h>
#include <openssl/rsa.h>

static const char certificate_store_dir[] = "certs";
static const char certificate_server_dir[] = "server";
static const char certificate_known_hosts_file[] = "known_hosts.v2";
static const char certificate_legacy_hosts_file[] = "known_hosts";

#include <freerdp/log.h>
#include <freerdp/crypto/certificate.h>

#define TAG FREERDP_TAG("crypto")

static BOOL certificate_split_line(char* line, char** host,
																	 UINT16* port, char** fingerprint);

BOOL certificate_store_init(rdpCertificateStore* certificate_store)
{
	char* server_path = NULL;
	rdpSettings* settings;

	settings = certificate_store->settings;

	if (!PathFileExistsA(settings->ConfigPath))
	{
		if (!CreateDirectoryA(settings->ConfigPath, 0))
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
		if (!CreateDirectoryA(certificate_store->path, 0))
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
		if (!CreateDirectoryA(server_path, 0))
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

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

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
	free(mdata);

	/* Found a valid fingerprint in legacy file,
	 * copy to new file in new format. */
	if (0 == match)
	{
		rdpCertificateData* data = certificate_data_new(hostname,
						certificate_data->port,
						pline);
		if (data)
			match = certificate_data_print(certificate_store, data) ? 0 : 1;
		certificate_data_free(data);
	}

	return match;

}

static int certificate_data_match_raw(rdpCertificateStore* certificate_store,
	rdpCertificateData* certificate_data, char** fprint)
{
	BOOL found = FALSE;
	FILE* fp;
	int length;
	char* data;
	char* mdata;
	char* pline;
	int match = 1;
	long int size;
	char* hostname = NULL;
	char* fingerprint = NULL;
	unsigned short port = 0;

	fp = fopen(certificate_store->file, "rb");

	if (!fp)
		return match;

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

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
			if (!certificate_split_line(pline, &hostname, &port, &fingerprint))
				WLog_WARN(TAG, "Invalid %s entry %s!", certificate_known_hosts_file, pline);
			else if (strcmp(pline, certificate_data->hostname) == 0)
			{
				if (port == certificate_data->port)
				{
					found = TRUE;
					match = strcmp(certificate_data->fingerprint, fingerprint);
					if (fingerprint && fprint)
						*fprint = _strdup(fingerprint);
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

BOOL certificate_get_fingerprint(rdpCertificateStore* certificate_store,
	rdpCertificateData* certificate_data, char** fingerprint)
{
	int rc = certificate_data_match_raw(certificate_store, certificate_data, fingerprint);

	if ((rc == 0) || (rc == -1))
		return TRUE;
	return FALSE;
}

int certificate_data_match(rdpCertificateStore* certificate_store, rdpCertificateData* certificate_data)
{
	return certificate_data_match_raw(certificate_store, certificate_data, NULL);
}

BOOL certificate_data_replace(rdpCertificateStore* certificate_store, rdpCertificateData* certificate_data)
{
	FILE* fp;
	BOOL rc = FALSE;
	int length;
	char* data;
	char* sdata;
	char* pline;
	long int size;

	fp = fopen(certificate_store->file, "rb");

	if (!fp)
		return FALSE;

	/* Read the current contents of the file. */
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (size < 1)
	{
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

	if (!fp)
	{
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
			char* hostname = NULL, *fingerprint;

			if (!certificate_split_line(pline, &hostname, &port, &fingerprint))
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
				fprintf(fp, "%s %hu %s\n", hostname, port, fingerprint);
			}
		}

		pline = StrSep(&sdata, "\r\n");
	}

	fclose(fp);
	free(data);

	return rc;
}

BOOL certificate_split_line(char* line, char** host, UINT16* port, char** fingerprint)
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

		return TRUE;
}

BOOL certificate_data_print(rdpCertificateStore* certificate_store, rdpCertificateData* certificate_data)
{
	FILE* fp;

	/* reopen in append mode */
	fp = fopen(certificate_store->file, "ab");

	if (!fp)
		return FALSE;

	fprintf(fp, "%s %hu %s\n", certificate_data->hostname, certificate_data->port,
					certificate_data->fingerprint);

	fclose(fp);

	return TRUE;
}

rdpCertificateData* certificate_data_new(char* hostname, UINT16 port, char* fingerprint)
{
	rdpCertificateData* certdata;

	certdata = (rdpCertificateData *)calloc(1, sizeof(rdpCertificateData));
	if (!certdata)
		return NULL;

	certdata->port = port;
	certdata->hostname = _strdup(hostname);
	if (!certdata->hostname)
		goto out_free;
	certdata->fingerprint = _strdup(fingerprint);
	if (!certdata->fingerprint)
		goto out_free_hostname;
	return certdata;

out_free_hostname:
	free(certdata->hostname);
out_free:
	free(certdata);
	return NULL;
}

void certificate_data_free(rdpCertificateData* certificate_data)
{
	if (certificate_data != NULL)
	{
		free(certificate_data->hostname);
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
		free(certstore);
	}
}
