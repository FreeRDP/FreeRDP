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

#include <ctype.h>

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <winpr/path.h>

#include <freerdp/settings.h>

#include <freerdp/crypto/crypto.h>
#include <freerdp/crypto/certificate_data.h>

#include "certificate.h"

struct rdp_certificate_data
{
	char* hostname;
	UINT16 port;
	rdpCertificate* cert;

	char cached_hash[MAX_PATH + 10];
	char* cached_subject;
	char* cached_issuer;
	char* cached_fingerprint;
	char* cached_pem;
	char* cached_pem_chain;
};

/* ensure our hostnames (and therefore filenames) always use the same capitalization.
 * the user might have input random case, but we always need to have a sane
 * baseline to compare against. */
static char* ensure_lowercase(char* str, size_t length)
{
	const size_t len = strnlen(str, length);
	for (size_t x = 0; x < len; x++)
		str[x] = tolower(str[x]);
	return str;
}
static const char* freerdp_certificate_data_hash_(const char* hostname, UINT16 port, char* name,
                                                  size_t length)
{
	(void)_snprintf(name, length, "%s_%" PRIu16 ".pem", hostname, port);
	return ensure_lowercase(name, length);
}

static BOOL freerdp_certificate_data_load_cache(rdpCertificateData* data)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(data);

	freerdp_certificate_data_hash_(data->hostname, data->port, data->cached_hash,
	                               sizeof(data->cached_hash));
	if (strnlen(data->cached_hash, sizeof(data->cached_hash)) == 0)
		goto fail;

	data->cached_subject = freerdp_certificate_get_subject(data->cert);
	if (!data->cached_subject)
		data->cached_subject = calloc(1, 1);

	size_t pemlen = 0;
	data->cached_pem = freerdp_certificate_get_pem_ex(data->cert, &pemlen, FALSE);
	if (!data->cached_pem)
		goto fail;

	size_t pemchainlen = 0;
	data->cached_pem_chain = freerdp_certificate_get_pem_ex(data->cert, &pemchainlen, TRUE);
	if (!data->cached_pem_chain)
		goto fail;

	data->cached_fingerprint = freerdp_certificate_get_fingerprint(data->cert);
	if (!data->cached_fingerprint)
		goto fail;

	data->cached_issuer = freerdp_certificate_get_issuer(data->cert);
	if (!data->cached_issuer)
		data->cached_issuer = calloc(1, 1);

	rc = TRUE;
fail:
	return rc;
}

static rdpCertificateData* freerdp_certificate_data_new_nocopy(const char* hostname, UINT16 port,
                                                               rdpCertificate* xcert)
{
	rdpCertificateData* certdata = NULL;

	if (!hostname || !xcert)
		goto fail;

	certdata = (rdpCertificateData*)calloc(1, sizeof(rdpCertificateData));

	if (!certdata)
		goto fail;

	certdata->port = port;
	certdata->hostname = _strdup(hostname);
	if (!certdata->hostname)
		goto fail;
	ensure_lowercase(certdata->hostname, strlen(certdata->hostname));

	certdata->cert = xcert;
	if (!freerdp_certificate_data_load_cache(certdata))
	{
		certdata->cert = NULL;
		goto fail;
	}

	return certdata;
fail:
	freerdp_certificate_data_free(certdata);
	return NULL;
}

rdpCertificateData* freerdp_certificate_data_new(const char* hostname, UINT16 port,
                                                 const rdpCertificate* xcert)
{
	rdpCertificate* copy = freerdp_certificate_clone(xcert);
	rdpCertificateData* data = freerdp_certificate_data_new_nocopy(hostname, port, copy);
	if (!data)
		freerdp_certificate_free(copy);
	return data;
}

rdpCertificateData* freerdp_certificate_data_new_from_pem(const char* hostname, UINT16 port,
                                                          const char* pem, size_t length)
{
	if (!pem || (length == 0))
		return NULL;

	rdpCertificate* cert = freerdp_certificate_new_from_pem(pem);
	rdpCertificateData* data = freerdp_certificate_data_new_nocopy(hostname, port, cert);
	if (!data)
		freerdp_certificate_free(cert);
	return data;
}

rdpCertificateData* freerdp_certificate_data_new_from_file(const char* hostname, UINT16 port,
                                                           const char* file)
{
	if (!file)
		return NULL;

	rdpCertificate* cert = freerdp_certificate_new_from_file(file);
	rdpCertificateData* data = freerdp_certificate_data_new_nocopy(hostname, port, cert);
	if (!data)
		freerdp_certificate_free(cert);
	return data;
}

void freerdp_certificate_data_free(rdpCertificateData* data)
{
	if (data == NULL)
		return;

	free(data->hostname);
	freerdp_certificate_free(data->cert);
	free(data->cached_subject);
	free(data->cached_issuer);
	free(data->cached_fingerprint);
	free(data->cached_pem);
	free(data->cached_pem_chain);

	free(data);
}

const char* freerdp_certificate_data_get_host(const rdpCertificateData* cert)
{
	if (!cert)
		return NULL;
	return cert->hostname;
}

UINT16 freerdp_certificate_data_get_port(const rdpCertificateData* cert)
{
	if (!cert)
		return 0;
	return cert->port;
}

const char* freerdp_certificate_data_get_pem(const rdpCertificateData* cert)
{
	return freerdp_certificate_data_get_pem_ex(cert, TRUE);
}

const char* freerdp_certificate_data_get_pem_ex(const rdpCertificateData* cert, BOOL withFullChain)
{
	if (!cert)
		return NULL;
	if (withFullChain)
		return cert->cached_pem_chain;
	return cert->cached_pem;
}

const char* freerdp_certificate_data_get_subject(const rdpCertificateData* cert)
{
	if (!cert)
		return NULL;

	return cert->cached_subject;
}

const char* freerdp_certificate_data_get_issuer(const rdpCertificateData* cert)
{
	if (!cert)
		return NULL;

	return cert->cached_issuer;
}
const char* freerdp_certificate_data_get_fingerprint(const rdpCertificateData* cert)
{
	if (!cert)
		return NULL;

	return cert->cached_fingerprint;
}

BOOL freerdp_certificate_data_equal(const rdpCertificateData* a, const rdpCertificateData* b)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(a);
	WINPR_ASSERT(b);

	if (strcmp(a->hostname, b->hostname) != 0)
		return FALSE;
	if (a->port != b->port)
		return FALSE;

	const char* pem1 = freerdp_certificate_data_get_fingerprint(a);
	const char* pem2 = freerdp_certificate_data_get_fingerprint(b);
	if (pem1 && pem2)
		rc = strcmp(pem1, pem2) == 0;
	else
		rc = pem1 == pem2;

	return rc;
}

const char* freerdp_certificate_data_get_hash(const rdpCertificateData* cert)
{
	if (!cert)
		return NULL;

	return cert->cached_hash;
}

char* freerdp_certificate_data_hash(const char* hostname, UINT16 port)
{
	char name[MAX_PATH + 10] = { 0 };
	freerdp_certificate_data_hash_(hostname, port, name, sizeof(name));
	return _strdup(name);
}
