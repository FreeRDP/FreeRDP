/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Certificate Handling
 *
 * Copyright 2011 Jiten Pathy
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include <freerdp/settings.h>

#include <freerdp/crypto/crypto.h>
#include <freerdp/crypto/certificate_store.h>
#include <freerdp/log.h>
#define TAG FREERDP_TAG("crypto")

struct rdp_certificate_store
{
	char* certs_path;
	char* server_path;
};

static const char certificate_store_dir[] = "certs";
static const char certificate_server_dir[] = "server";

static char* freerdp_certificate_store_file_path(rdpCertificateStore* store, const char* hash)
{
	const char* hosts = freerdp_certificate_store_get_hosts_path(store);

	if (!hosts || !hash)
		return NULL;

	return GetCombinedPath(hosts, hash);
}

static char* freerdp_certificate_store_file_path_raw(rdpCertificateStore* store, const char* host,
                                                     UINT16 port)
{
	char* hash = freerdp_certificate_data_hash(host, port);
	char* path = freerdp_certificate_store_file_path(store, hash);
	free(hash);
	return path;
}

freerdp_certificate_store_result
freerdp_certificate_store_contains_data(rdpCertificateStore* store, const rdpCertificateData* data)
{
	freerdp_certificate_store_result rc = CERT_STORE_NOT_FOUND;
	const char* host = freerdp_certificate_data_get_host(data);
	const UINT16 port = freerdp_certificate_data_get_port(data);

	rdpCertificateData* loaded = freerdp_certificate_store_load_data(store, host, port);
	if (!loaded)
		goto fail;

	rc = freerdp_certificate_data_equal(data, loaded) ? CERT_STORE_MATCH : CERT_STORE_MISMATCH;

fail:
	freerdp_certificate_data_free(loaded);
	return rc;
}

BOOL freerdp_certificate_store_remove_data(rdpCertificateStore* store,
                                           const rdpCertificateData* data)
{
	BOOL rc = TRUE;
	const char* hash = freerdp_certificate_data_get_hash(data);
	char* path = freerdp_certificate_store_file_path(store, hash);

	if (!path)
		return FALSE;

	if (winpr_PathFileExists(path))
		rc = winpr_DeleteFile(path);
	free(path);
	return rc;
}

BOOL freerdp_certificate_store_save_data(rdpCertificateStore* store, const rdpCertificateData* data)
{
	BOOL rc = FALSE;
	const char* base = freerdp_certificate_store_get_hosts_path(store);
	const char* hash = freerdp_certificate_data_get_hash(data);
	char* path = freerdp_certificate_store_file_path(store, hash);
	FILE* fp = NULL;

	if (!winpr_PathFileExists(base))
	{
		if (!winpr_PathMakePath(base, NULL))
			goto fail;
	}

	fp = winpr_fopen(path, "w");
	if (!fp)
		goto fail;

	fprintf(fp, "%s", freerdp_certificate_data_get_pem(data));

	rc = TRUE;
fail:
	fclose(fp);
	free(path);
	return rc;
}

rdpCertificateData* freerdp_certificate_store_load_data(rdpCertificateStore* store,
                                                        const char* host, UINT16 port)
{
	char* path = NULL;
	rdpCertificateData* data = NULL;

	WINPR_ASSERT(store);

	path = freerdp_certificate_store_file_path_raw(store, host, port);
	if (!path)
		goto fail;

	data = freerdp_certificate_data_new_from_file(host, port, path);

fail:
	free(path);
	return data;
}

rdpCertificateStore* freerdp_certificate_store_new(const rdpSettings* settings)
{
	rdpCertificateStore* store = (rdpCertificateStore*)calloc(1, sizeof(rdpCertificateStore));

	if (!store)
		return NULL;

	const char* base = freerdp_settings_get_string(settings, FreeRDP_ConfigPath);
	if (!base)
		goto fail;

	store->certs_path = GetCombinedPath(base, certificate_store_dir);
	store->server_path = GetCombinedPath(base, certificate_server_dir);
	if (!store->certs_path || !store->server_path)
		goto fail;

	return store;

fail:
	freerdp_certificate_store_free(store);
	return NULL;
}

void freerdp_certificate_store_free(rdpCertificateStore* store)
{
	if (!store)
		return;

	free(store->certs_path);
	free(store->server_path);
	free(store);
}

const char* freerdp_certificate_store_get_certs_path(const rdpCertificateStore* store)
{
	WINPR_ASSERT(store);
	return store->certs_path;
}

const char* freerdp_certificate_store_get_hosts_path(const rdpCertificateStore* store)
{
	WINPR_ASSERT(store);
	return store->server_path;
}
