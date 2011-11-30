/**
 * FreeRDP: A Remote Desktop Protocol Client
 * certstore Utils
 *
 * Copyright 2011 Jiten Pathy
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/utils/file.h>
#include <freerdp/utils/certstore.h>

static const char cert_dir[] = "freerdp";
static const char cert_loc[] = "cacert";
static const char certstore_file[] = "known_hosts";

void certstore_create(rdpCertStore* certstore)
{
	certstore->fp = fopen((char*) certstore->file, "w+");

	if (certstore->fp == NULL)
	{
		printf("certstore_create: error opening [%s] for writing\n", certstore->file);
		return;
	}

	fflush(certstore->fp);
}

void certstore_load(rdpCertStore* certstore)
{
	certstore->fp = fopen((char*) certstore->file, "r+");
}

void certstore_open(rdpCertStore* certstore)
{
	struct stat stat_info;

	if (stat((char*) certstore->file, &stat_info) != 0)
		certstore_create(certstore);
	else
		certstore_load(certstore);
}

void certstore_close(rdpCertStore* certstore)
{
	if (certstore->fp != NULL)
		fclose(certstore->fp);
}

char* get_local_certloc(char* home_path)
{
	char* certloc;
	struct stat stat_info;

	if (home_path == NULL)
		home_path = getenv("HOME");

	certloc = (char*) xmalloc(strlen(home_path) + 2 + strlen(cert_dir) + 1 + strlen(cert_loc) + 1);
	sprintf(certloc, "%s/.%s/%s", home_path, cert_dir, cert_loc);

	if(stat((char*) certloc, &stat_info) != 0)
		freerdp_mkdir(certloc);
	
	return certloc;
}

void certstore_init(rdpCertStore* certstore)
{
	int length;
	char* home_path;
	struct stat stat_info;
	
	certstore->match = 1;

	if (certstore->home_path == NULL)
		home_path = getenv("HOME");
	else
		home_path = certstore->home_path;

	if (home_path == NULL)
	{
		printf("could not get home path\n");
		return;
	}

	certstore->home_path = (char*) xstrdup(home_path);

	certstore->path = (char*) xmalloc(strlen(certstore->home_path) + 2 + strlen(cert_dir) + 1);
	sprintf(certstore->path, "%s/.%s", certstore->home_path, cert_dir);

	if (stat(certstore->path, &stat_info) != 0)
	{
		freerdp_mkdir(certstore->path);
		printf("creating directory %s\n", certstore->path);
	}

	length = strlen(certstore->path);
	certstore->file = (char*) xmalloc(strlen(certstore->path) + 1 + strlen(certstore_file) + 1);
	sprintf(certstore->file, "%s/%s", certstore->path, certstore_file);

	certstore_open(certstore);
}

rdpCertData* certdata_new(char* hostname, char* fingerprint)
{
	rdpCertData* certdata;

	certdata = (rdpCertData*) xzalloc(sizeof(rdpCertData));

	if (certdata != NULL)
	{
		certdata->hostname = xzalloc(strlen(hostname) + 1);
		certdata->fingerprint = xzalloc(strlen(fingerprint) + 1);
		sprintf(certdata->hostname, "%s", hostname);
		sprintf(certdata->fingerprint, "%s", fingerprint);
	}

	return certdata;
}

void certdata_free(rdpCertData* certdata)
{
	if(certdata != NULL)
	{
		xfree(certdata->hostname);
		xfree(certdata->fingerprint);
		xfree(certdata);
	}
}

rdpCertStore* certstore_new(rdpCertData* certdata, char* home_path)
{
	rdpCertStore* certstore;

	certstore = (rdpCertStore*) xzalloc(sizeof(rdpCertStore));

	if (certstore != NULL)
	{
		certstore->home_path = home_path;
		certstore->certdata = certdata;
		certstore_init(certstore);
	}

	return certstore;
}

void certstore_free(rdpCertStore* certstore)
{
	if (certstore != NULL)
	{
		certstore_close(certstore);
		xfree(certstore->path);
		xfree(certstore->file);
		xfree(certstore->home_path);
		certdata_free(certstore->certdata);
		xfree(certstore);
	}
}

int cert_data_match(rdpCertStore* certstore)
{
	FILE* fp;
	int length;
	char* data;
	char* pline;
	long int size;
	rdpCertData* cert_data;

	fp = certstore->fp;
	cert_data = certstore->certdata;

	if (!fp)
		return certstore->match;

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	data = (char*) xmalloc(size + 1);
	length = fread(data, size, 1, fp);

	if (size < 1)
		return certstore->match;

	data[size] = '\n';
	pline = strtok(data, "\n");

	while (pline != NULL)
	{
		length = strlen(pline);

		if (length > 0)
		{
			length = strcspn(pline, " \t");
			pline[length] = '\0';

			if (strcmp(pline, cert_data->hostname) == 0)
			{
				pline = &pline[length + 1];

				if (strcmp(pline, cert_data->fingerprint) == 0)
					certstore->match = 0;
				else
					certstore->match = -1;
				break;
			}
		}

		pline = strtok(NULL, "\n");
	}
	xfree(data);

	return certstore->match;
}

void cert_data_print(rdpCertStore* certstore)
{
	FILE* fp;

	/* reopen in append mode */
	fp = fopen(certstore->file, "a");

	if (!fp)
		return;

	fprintf(certstore->fp,"%s %s\n", certstore->certdata->hostname, certstore->certdata->fingerprint);
	fclose(fp);
}
