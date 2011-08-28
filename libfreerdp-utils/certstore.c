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

#include <freerdp/utils/certstore.h>

#ifdef _WIN32
#include <windows.h>
#endif

static char cert_dir[] = "freerdp";
static char cert_loc[] = "cacert";
static char certstore_file[] = "known_hosts";

void certstore_create(rdpCertstore* certstore)
{
	certstore->fp = fopen((char*)certstore->file, "w+");

	if (certstore->fp == NULL)
	{
		printf("certstore_create: error opening [%s] for writing\n", certstore->file);
		return;
	}
	fflush(certstore->fp);
}

void certstore_load(rdpCertstore* certstore)
{
	certstore->fp = fopen((char*)certstore->file, "r+");
}

void certstore_open(rdpCertstore* certstore)
{
	struct stat stat_info;

	if (stat((char*)certstore->file, &stat_info) != 0)
		certstore_create(certstore);
	else
		certstore_load(certstore);
}

void certstore_close(rdpCertstore* certstore)
{
	if (certstore->fp != NULL)
		fclose(certstore->fp);
}

char* get_local_certloc()
{
	char *home_path;
	char *certloc;
	struct stat stat_info;
	home_path=getenv("HOME");
	certloc=(char*)xmalloc(strlen(home_path)+strlen("/.")+strlen(cert_dir)+strlen("/")+strlen(cert_loc)+1);
	sprintf(certloc,"%s/.%s/%s",home_path,cert_dir,cert_loc);
	if(stat((char*)certloc, &stat_info) != 0)
		mkdir(certloc, S_IRUSR | S_IWUSR | S_IXUSR);
	return certloc;
}

void certstore_init(rdpCertstore* certstore)
{
	int length;
	char* home_path;
	struct stat stat_info;
	
	certstore->match=1;
	home_path = getenv("HOME");

	if (home_path == NULL)
	{
		printf("could not get home path\n");
		return;
	}

	certstore->home = (char*) xstrdup(home_path);
	printf("home path: %s\n", certstore->home);

	certstore->path = (char*) xmalloc(strlen(certstore->home) + strlen("/.") + strlen(cert_dir) + 1);
	sprintf(certstore->path, "%s/.%s", certstore->home, cert_dir);
	printf("certstore path: %s\n", certstore->path);

	if (stat(certstore->path, &stat_info) != 0)
	{
#ifndef _WIN32
		mkdir(certstore->path, S_IRUSR | S_IWUSR | S_IXUSR);
#else
		CreateDirectory(certstore->path, 0);
#endif
		printf("creating directory %s\n", certstore->path);
	}

	length = strlen(certstore->path);
	certstore->file = (char*) xmalloc(strlen(certstore->path) + strlen("/") + strlen(certstore_file) + 1);
	sprintf(certstore->file, "%s/%s", certstore->path, certstore_file);
	printf("certstore file: %s\n", certstore->file);

	certstore_open(certstore);
}

rdpCertdata* certdata_new(char* host_name,char* fingerprint)
{
	rdpCertdata* certdata=(rdpCertdata*)xzalloc(sizeof(rdpCertdata));
	if(certdata !=NULL)
	{
		certdata->hostname=xzalloc(strlen(host_name)+1);
		certdata->thumbprint=xzalloc(strlen(fingerprint)+1);
		sprintf(certdata->hostname,"%s",host_name);
		sprintf(certdata->thumbprint,"%s",fingerprint);
	}
	return certdata;
}

void certdata_free(rdpCertdata* certdata)
{
	if(certdata != NULL)
	{
		xfree(certdata->hostname);
		xfree(certdata->thumbprint);
		xfree(certdata);
	}
}

rdpCertstore* certstore_new(rdpCertdata* certdata)
{
	rdpCertstore* certstore = (rdpCertstore*) xzalloc(sizeof(rdpCertstore));

	if (certstore != NULL)
	{
		certstore->certdata = certdata;
		certstore_init(certstore);
	}

	return certstore;
}

void certstore_free(rdpCertstore* certstore)
{
	if (certstore != NULL)
	{
		certstore_close(certstore);
		xfree(certstore->path);
		xfree(certstore->file);
		xfree(certstore->home);
		certdata_free(certstore->certdata);
		xfree(certstore);
	}
}

int match_certdata(rdpCertstore* certstore)
{
	char *host;
	char *p;
	char *thumb_print;
	int length;
	unsigned char c;
	FILE* cfp;
	cfp=certstore->fp;
	rdpCertdata* cert_data=certstore->certdata;
	length=strlen(cert_data->thumbprint);
	host=xzalloc(strlen(cert_data->hostname)+1);
	for(;;)
	{
		if((int)fread(host,sizeof(char),strlen(cert_data->hostname),cfp) < strlen(cert_data->hostname))
			break;
		if((!strcmp(host,cert_data->hostname)) && ((c=fgetc(cfp))==' ' || c=='\t') )
		{
			ungetc(c,cfp);
			while((c=fgetc(cfp))==' ' || c=='\t');
			if(c==EOF)
				break;
			ungetc(c,cfp);
			thumb_print=xzalloc(length+1);
			p=thumb_print;
			while((p-thumb_print) < length && (*p=fgetc(cfp))!=EOF && *p!='\n' && *p==*(cert_data->thumbprint+(p-thumb_print)))
				p++;
			if(p-thumb_print==length)
				certstore->match=0;
			else
				certstore->match=-1;
			break;
		}
		else
			while(c!='\n'||c!=EOF)
				c=fgetc(cfp);
			if(c==EOF)
				break;
			else
				ungetc(c,cfp);
	}
	xfree(host);
	return certstore->match;
}
void print_certdata(rdpCertstore* certstore)
{
	fseek(certstore->fp,0,SEEK_END);
	fprintf(certstore->fp,"%s %s\n",certstore->certdata->hostname,certstore->certdata->thumbprint);
}
