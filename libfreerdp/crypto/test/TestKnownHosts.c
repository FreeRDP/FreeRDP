/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 Armin Novak <armin.novak@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <winpr/path.h>
#include <winpr/file.h>
#include <freerdp/crypto/certificate.h>

static int prepare(const char* currentFileV2, const char* legacyFileV2, const char* legacyFile)
{
	char buffer[1024];
	HANDLE fp = NULL;
	HANDLE fl = NULL;
	HANDLE fc = NULL;
	DWORD read = 0;
	char* srcV2 = _strdup("known_hosts/known_hosts.v2");
	char* src = _strdup("known_hosts/known_hosts");

	if (!src || !srcV2)
		goto finish;

	PathCchConvertStyleA(srcV2, strlen(srcV2), PATH_STYLE_NATIVE);
	PathCchConvertStyleA(src, strlen(src), PATH_STYLE_NATIVE);

	fp = CreateFileA(srcV2, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!fp)
		goto finish;

	fl = CreateFileA(currentFileV2, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!fc)
		goto finish;

	fl = CreateFileA(legacyFileV2, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!fl)
		goto finish;

	while(ReadFile(fp, buffer, sizeof(buffer), &read, NULL))
	{
		WriteFile(fc, buffer, read, NULL, NULL);
		WriteFile(fl, buffer, read, NULL, NULL);
	}

	CloseHandle(fp);
	fp = NULL;

	CloseHandle(fc);
	fc = NULL;

	CloseHandle(fl);
	fl = NULL;

	fp = CreateFileA(src, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!fp)
		goto finish;

	fl = CreateFileA(legacyFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!fl)
		goto finish;

	while(ReadFile(fp, buffer, sizeof(buffer), &read, NULL))
		WriteFile(fl, buffer, read, NULL, NULL);

	CloseHandle(fp);
	CloseHandle(fl);
	free(src);
	free(srcV2);
	return 0;

finish:
	free(src);
	free(srcV2);
	CloseHandle(fp);
	CloseHandle(fl);
	CloseHandle(fc);

	return -1;
}

int TestKnownHosts(int argc, char* argv[])
{
	int rc = -1;
	rdpSettings current;
	rdpSettings legacy;
	rdpCertificateData* data = NULL;
	rdpCertificateStore* store = NULL;
	char* currentFileV2 = NULL;
	char* legacyFileV2 = NULL;
	char* legacyFile = NULL;

	current.ConfigPath = GetKnownSubPath(KNOWN_PATH_TEMP, "TestKnownHostsCurrent");
	legacy.ConfigPath = GetKnownSubPath(KNOWN_PATH_TEMP, "TestKnownHostsLegacy");

	if (!CreateDirectoryA(current.ConfigPath, NULL))
	{
		fprintf(stderr, "Could not create %s!\n", current.ConfigPath);
	//	goto finish;
	}

	if (!CreateDirectoryA(legacy.ConfigPath, NULL))
	{
		fprintf(stderr, "Could not create %s!\n", legacy.ConfigPath);
	//	goto finish;
	}

	currentFileV2 = GetCombinedPath(current.ConfigPath, "known_hosts.v2");
	if (!currentFileV2)
	{
		fprintf(stderr, "Could not get file path!\n");
		goto finish;
	}

	legacyFileV2 = GetCombinedPath(legacy.ConfigPath, "known_hosts.v2");
	if (!legacyFileV2)
	{
		fprintf(stderr, "Could not get file path!\n");
		goto finish;
	}

	legacyFile = GetCombinedPath(legacy.ConfigPath, "known_hosts");
	if (!legacyFile)
	{
		fprintf(stderr, "Could not get file path!\n");
		goto finish;
	}

 	store = certificate_store_new(&current);
	if (!store)
	{
		fprintf(stderr, "Could not create certificate store!\n");
		goto finish;
	}

	if (prepare(currentFileV2, legacyFileV2, legacyFile))
		goto finish;

	/* Test if host is found in current file. */
	data = certificate_data_new("someurl", 3389, "ff:11:22:dd");
	if (!data)
	{
		fprintf(stderr, "Could not create certificate data!\n");
		goto finish;
	}

	if (0 != certificate_data_match(store, data))
	{
		fprintf(stderr, "Could not find data in v2 file!\n");
		goto finish;
	}

	certificate_data_free(data);

	/* Test if host not found in current file. */
	data = certificate_data_new("somehost", 1234, "ff:aa:bb:cc");
	if (!data)
	{
		fprintf(stderr, "Could not create certificate data!\n");
		goto finish;
	}

	if (0 == certificate_data_match(store, data))
	{
		fprintf(stderr, "Invalid host found in v2 file!\n");
		goto finish;
	}

	certificate_data_free(data);

	/* Test host add current file. */
	data = certificate_data_new("somehost", 1234, "ff:aa:bb:cc");
	if (!data)
	{
		fprintf(stderr, "Could not create certificate data!\n");
		goto finish;
	}

	if (!certificate_data_print(store, data))
	{
		fprintf(stderr, "Could not add host to file!\n");
		goto finish;
	}

	if (0 == certificate_data_match(store, data))
	{
		fprintf(stderr, "Invalid host found in v2 file!\n");
		goto finish;
	}

	certificate_data_free(data);

	/* Test host replace current file. */
	data = certificate_data_new("somehost", 1234, "ff:aa:bb:dd:ee");
	if (!data)
	{
		fprintf(stderr, "Could not create certificate data!\n");
		goto finish;
	}

	if (!certificate_data_replace(store, data))
	{
		fprintf(stderr, "Could not replace data!\n");
		goto finish;
	}

	if (0 == certificate_data_match(store, data))
	{
		fprintf(stderr, "Invalid host found in v2 file!\n");
		goto finish;
	}

	certificate_data_free(data);

	certificate_store_free(store);

	store = certificate_store_new(&legacy);
	if (!store)
	{
		fprintf(stderr, "could not create certificate store!\n");
		goto finish;
	}

	/* test if host found in legacy file. */
	data = certificate_data_new("someurl", 1234, "ff:11:22:dd");
	if (!data)
	{
		fprintf(stderr, "Could not create certificate data!\n");
		goto finish;
	}

	if (0 != certificate_data_match(store, data))
	{
		fprintf(stderr, "Could not find host in file!\n");
		goto finish;
	}

	certificate_data_free(data);

	/* test if host not found. */
	data = certificate_data_new("somehost-not-in-file", 1234, "ff:aa:bb:cc");
	if (!data)
	{
		fprintf(stderr, "Could not create certificate data!\n");
		goto finish;
	}

	if (0 == certificate_data_match(store, data))
	{
		fprintf(stderr, "Invalid host found in file!\n");
		goto finish;
	}

	rc = 0;

finish:
	if (store)
		certificate_store_free(store);
	if (data)
		certificate_data_free(data);
	DeleteFileA(currentFileV2);
	//RemoveDirectoryA(current.ConfigPath);

	DeleteFileA(legacyFileV2);
	DeleteFileA(legacyFile);
	//RemoveDirectoryA(legacy.ConfigPath);

	free (currentFileV2);
	free (legacyFileV2);
	free (legacyFile);

	return rc;
}
