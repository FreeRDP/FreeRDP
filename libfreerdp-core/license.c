/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Licensing
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

#include "license.h"

/**
 * Receive an RDP licensing packet.\n
 * @msdn{cc240479}
 * @param rdp RDP module
 * @param s stream
 * @param sec_flags security flags
 */

void license_recv(rdpLicense* license, STREAM* s, uint16 sec_flags)
{
	uint8 flags;
	uint8 bMsgType;
	uint16 wMsgSize;

	printf("SEC_LICENSE_PKT\n");

	/* preamble (4 bytes) */
	stream_read_uint8(s, bMsgType); /* bMsgType (1 byte) */
	stream_read_uint8(s, flags); /* flags (1 byte) */
	stream_read_uint16(s, wMsgSize); /* wMsgSize (2 bytes) */

	printf("bMsgType:%X, flags:%X, wMsgSize:%02x\n", bMsgType, flags, wMsgSize);

	switch (bMsgType)
	{
		case LICENSE_REQUEST:
			license_read_license_request_packet(license, s);
			break;

		case PLATFORM_CHALLENGE:
			license_read_platform_challenge_packet(license, s);
			break;

		case NEW_LICENSE:
			license_read_new_license_packet(license, s);
			break;

		case UPGRADE_LICENSE:
			license_read_upgrade_license_packet(license, s);
			break;

		case ERROR_ALERT:
			license_read_error_alert_packet(license, s);
			break;

		default:
			printf("invalid bMsgType:%d\n", bMsgType);
			break;
	}
}

/**
 * Read PRODUCT_INFO.\n
 * @param s stream
 * @param productInfo product information
 */

void license_read_product_info(STREAM* s, PRODUCT_INFO* productInfo)
{
	stream_read_uint32(s, productInfo->dwVersion); /* dwVersion (4 bytes) */

	stream_read_uint32(s, productInfo->cbCompanyName); /* cbCompanyName (4 bytes) */

	productInfo->pbCompanyName = (uint8*) xmalloc(productInfo->cbCompanyName);
	stream_read(s, productInfo->pbCompanyName, productInfo->cbCompanyName);

	stream_read_uint32(s, productInfo->cbProductId); /* cbProductId (4 bytes) */

	productInfo->pbProductId = (uint8*) xmalloc(productInfo->cbProductId);
	stream_read(s, productInfo->pbProductId, productInfo->cbProductId);
}

void license_read_binary_blob(STREAM* s, LICENSE_BLOB* blob)
{
	uint16 wBlobType;

	stream_read_uint16(s, wBlobType); /* wBlobType (2 bytes) */

	if (blob->type != wBlobType && blob->type != 0)
	{
		printf("license binary blob type does not match expected type.\n");
		return;
	}

	blob->type = wBlobType;

	stream_read_uint16(s, blob->length); /* wBlobLen (2 bytes) */

	blob->data = (uint8*) xmalloc(blob->length);

	stream_read(s, blob->data, blob->length); /* blobData */
}

void license_read_scope_list(STREAM* s, SCOPE_LIST* scopeList)
{
	int i;
	uint32 scopeCount;

	stream_read_uint32(s, scopeCount); /* ScopeCount (4 bytes) */

	scopeList->count = scopeCount;
	scopeList->array = (LICENSE_BLOB*) xmalloc(sizeof(LICENSE_BLOB) * scopeCount);

	/* ScopeArray */
	for (i = 0; i < scopeCount; i++)
	{
		scopeList->array[i].type = BB_SCOPE_BLOB;
		license_read_binary_blob(s, &scopeList->array[i]);
	}
}

/**
 * Read a LICENSE_REQUEST packet.\n
 * @msdn{cc241914}
 * @param rdp RDP module
 * @param s stream
 */

void license_read_license_request_packet(rdpLicense* license, STREAM* s)
{
	printf("LICENSE_REQUEST\n");

	/* ServerRandom (32 bytes) */
	stream_read(s, license->server_random, 32);

	/* ProductInfo */
	license_read_product_info(s, &(license->product_info));

	/* KeyExchangeList */
	license_read_binary_blob(s, &(license->key_exchange_list));

	/* ServerCertificate */
	license_read_binary_blob(s, &(license->server_certificate));

	/* ScopeList */
	license_read_scope_list(s, &(license->scope_list));
}

/**
 * Read a PLATFORM_CHALLENGE packet.\n
 * @msdn{cc241921}
 * @param rdp RDP module
 * @param s stream
 */

void license_read_platform_challenge_packet(rdpLicense* license, STREAM* s)
{

}

/**
 * Read a NEW_LICENSE packet.\n
 * @msdn{cc241926}
 * @param rdp RDP module
 * @param s stream
 */

void license_read_new_license_packet(rdpLicense* license, STREAM* s)
{

}

/**
 * Read an UPGRADE_LICENSE packet.\n
 * @msdn{cc241924}
 * @param rdp RDP module
 * @param s stream
 */

void license_read_upgrade_license_packet(rdpLicense* license, STREAM* s)
{

}

/**
 * Read an ERROR_ALERT packet.\n
 * @msdn{cc240482}
 * @param rdp RDP module
 * @param s stream
 */

void license_read_error_alert_packet(rdpLicense* license, STREAM* s)
{

}

/**
 * Instantiate new license module.
 * @return new license module
 */

rdpLicense* license_new()
{
	rdpLicense* license;

	license = (rdpLicense*) xzalloc(sizeof(rdpLicense));

	if (license != NULL)
	{
		license->key_exchange_list.type = BB_KEY_EXCHG_ALG_BLOB;
		license->server_certificate.type = BB_CERTIFICATE_BLOB;
	}

	return license;
}

/**
 * Free license module.
 * @param license license module to be freed
 */

void license_free(rdpLicense* license)
{
	if (license != NULL)
	{
		xfree(license);
	}
}

