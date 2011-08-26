/**
 * FreeRDP: A Remote Desktop Protocol Client
 * certstore Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/utils/certstore.h>

#ifdef _WIN32
#include <windows.h>
#endif

static char cert_dir[] = "freerdp";
static char certstore_file[] = "known_hosts";

void certstore_init(rdpCertstore* certstore)
{
	int length;
	char* home_path;
	struct stat stat_info;

	home_path = getenv("HOME");
	certstore->available = True;

	if (home_path == NULL)
	{
		printf("could not get home path\n");
		certstore->available = False;
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
