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
	char* legacy[] = {
	    "someurl ff:11:22:dd\r\n",
	    "otherurl aa:bb:cc:dd\r",
	    "legacyurl aa:bb:cc:dd\n"
	};
	char* hosts[] = {
	    "someurl 3389 ff:11:22:dd subject issuer\r\n",
	    "otherurl\t3389\taa:bb:cc:dd\tsubject2\tissuer2\r",
	};
	FILE* fl = NULL;
	FILE* fc = NULL;
	size_t i;

	fc = fopen(currentFileV2, "w+");
	if (!fc)
		goto finish;

	fl = fopen(legacyFileV2, "w+");
	if (!fl)
		goto finish;

	for (i=0; i<sizeof(hosts)/sizeof(hosts[0]); i++)
	{
		if (fwrite(hosts[i], strlen(hosts[i]), 1, fl) != 1 ||
		    fwrite(hosts[i], strlen(hosts[i]), 1, fc) != 1)
			goto finish;
	}

	fclose(fc);
	fc = NULL;

	fclose(fl);
	fl = NULL;

	fl = fopen(legacyFile, "w+");
	if (!fl)
		goto finish;

	for (i=0; i<sizeof(legacy)/sizeof(legacy[0]); i++)
	{
		if (fwrite(legacy[i], strlen(legacy[i]), 1, fl) != 1)
			goto finish;
	}

	fclose(fl);
	return 0;

finish:
	if (fl)
		fclose(fl);
	if (fc)
		fclose(fc);

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
	char* subject = NULL;
	char* issuer = NULL;
	char* fp = NULL;

	current.ConfigPath = GetKnownSubPath(KNOWN_PATH_TEMP, "TestKnownHostsCurrent");
	legacy.ConfigPath = GetKnownSubPath(KNOWN_PATH_TEMP, "TestKnownHostsLegacy");

	if (!PathFileExistsA(current.ConfigPath))
	{
		if (!CreateDirectoryA(current.ConfigPath, NULL))
		{
			fprintf(stderr, "Could not create %s!\n", current.ConfigPath);
			goto finish;
		}
	}

	if (!PathFileExistsA(legacy.ConfigPath))
	{
		if (!CreateDirectoryA(legacy.ConfigPath, NULL))
		{
			fprintf(stderr, "Could not create %s!\n", legacy.ConfigPath);
			goto finish;
		}
	}

	currentFileV2 = GetCombinedPath(current.ConfigPath, "known_hosts2");
	if (!currentFileV2)
	{
		fprintf(stderr, "Could not get file path!\n");
		goto finish;
	}

	legacyFileV2 = GetCombinedPath(legacy.ConfigPath, "known_hosts2");
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
	data = certificate_data_new("someurl", 3389, "subject", "issuer", "ff:11:22:dd");
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

	/* Test if we can read out the old fingerprint. */
	if (!certificate_get_stored_data(store, data, &subject, &issuer, &fp))
	{
		fprintf(stderr, "Could not read old fingerprint!\n");
		goto finish;
	}
	printf("Got %s, %s '%s'\n", subject, issuer, fp);
	free(subject);
	free(issuer);
	free(fp);
	subject = NULL;
	issuer = NULL;
	fp = NULL;
	certificate_data_free(data);

	/* Test if host not found in current file. */
	data = certificate_data_new("somehost", 1234, "", "", "ff:aa:bb:cc");
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

	/* Test if we read out the old fingerprint fails. */
	if (certificate_get_stored_data(store, data, &subject, &issuer, &fp))
	{
		fprintf(stderr, "Read out not existing old fingerprint succeeded?!\n");
		goto finish;
	}

	certificate_data_free(data);

	/* Test host add current file. */
	data = certificate_data_new("somehost", 1234, "", "", "ff:aa:bb:cc");
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

	if (0 != certificate_data_match(store, data))
	{
		fprintf(stderr, "Could not find host written in v2 file!\n");
		goto finish;
	}

	certificate_data_free(data);

	/* Test host replace current file. */
	data = certificate_data_new("somehost", 1234, "", "", "ff:aa:bb:dd:ee");
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

	if (0 != certificate_data_match(store, data))
	{
		fprintf(stderr, "Invalid host found in v2 file!\n");
		goto finish;
	}

	certificate_data_free(data);

	/* Test host replace invalid entry in current file. */
	data = certificate_data_new("somehostXXXX", 1234, "", "", "ff:aa:bb:dd:ee");
	if (!data)
	{
		fprintf(stderr, "Could not create certificate data!\n");
		goto finish;
	}

	if (certificate_data_replace(store, data))
	{
		fprintf(stderr, "Invalid return for replace invalid entry!\n");
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
	data = certificate_data_new("legacyurl", 1234, "", "", "aa:bb:cc:dd");
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
	data = certificate_data_new("somehost-not-in-file", 1234, "", "", "ff:aa:bb:cc");
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
	free(current.ConfigPath);
	free(legacy.ConfigPath);
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
	free(subject);
	free(issuer);
	free(fp);

	return rc;
}
