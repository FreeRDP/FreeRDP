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
 * Read a licensing preamble.\n
 * @msdn{cc240480}
 * @param s stream
 * @param bMsgType license message type
 * @param flags message flags
 * @param wMsgSize message size
 */

void license_read_preamble(STREAM* s, uint8* bMsgType, uint8* flags, uint16* wMsgSize)
{
	/* preamble (4 bytes) */
	stream_read_uint8(s, *bMsgType); /* bMsgType (1 byte) */
	stream_read_uint8(s, *flags); /* flags (1 byte) */
	stream_read_uint16(s, *wMsgSize); /* wMsgSize (2 bytes) */
}

/**
 * Write a licensing preamble.\n
 * @msdn{cc240480}
 * @param s stream
 * @param bMsgType license message type
 * @param flags message flags
 * @param wMsgSize message size
 */

void license_write_preamble(STREAM* s, uint8 bMsgType, uint8 flags, uint16 wMsgSize)
{
	/* preamble (4 bytes) */
	stream_write_uint8(s, bMsgType); /* bMsgType (1 byte) */
	stream_write_uint8(s, flags); /* flags (1 byte) */
	stream_write_uint16(s, wMsgSize); /* wMsgSize (2 bytes) */
}

/**
 * Initialize a license packet stream.\n
 * @param license license module
 * @return stream
 */

STREAM* license_send_stream_init(rdpLicense* license)
{
	STREAM* s;
	s = transport_send_stream_init(license->rdp->transport, 4096);
	stream_seek(s, LICENSE_PACKET_HEADER_LENGTH);
	return s;
}

/**
 * Send an RDP licensing packet.\n
 * @msdn{cc240479}
 * @param license license module
 * @param s stream
 */

void license_send(rdpLicense* license, STREAM* s, uint8 type)
{
	int length;
	uint16 wMsgSize;
	uint16 sec_flags;

	length = stream_get_length(s);
	stream_set_pos(s, 0);

	sec_flags = SEC_LICENSE_PKT;
	wMsgSize = length - LICENSE_PACKET_HEADER_LENGTH;

	rdp_write_header(license->rdp, s, length);
	rdp_write_security_header(s, sec_flags);
	license_write_preamble(s, type, PREAMBLE_VERSION_2_0, wMsgSize);

	stream_set_pos(s, length);
	transport_write(license->rdp->transport, s);
}

/**
 * Receive an RDP licensing packet.\n
 * @msdn{cc240479}
 * @param license license module
 * @param s stream
 */

void license_recv(rdpLicense* license, STREAM* s)
{
	uint8 flags;
	uint8 bMsgType;
	uint16 wMsgSize;

	printf("SEC_LICENSE_PKT\n");

	license_read_preamble(s, &bMsgType, &flags, &wMsgSize); /* preamble (4 bytes) */

	printf("bMsgType:%X, flags:%X, wMsgSize:%02x\n", bMsgType, flags, wMsgSize);

	switch (bMsgType)
	{
		case LICENSE_REQUEST:
			license_read_license_request_packet(license, s);
			license_send_new_license_request_packet(license);
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
 * Read Product Information (PRODUCT_INFO).\n
 * @msdn{cc241915}
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

/**
 * Allocate New Product Information (PRODUCT_INFO).\n
 * @msdn{cc241915}
 * @return new product information
 */

PRODUCT_INFO* license_new_product_info()
{
	PRODUCT_INFO* productInfo;

	productInfo = (PRODUCT_INFO*) xmalloc(sizeof(PRODUCT_INFO));

	productInfo->dwVersion = 0;
	productInfo->cbCompanyName = 0;
	productInfo->pbCompanyName = NULL;
	productInfo->cbProductId = 0;
	productInfo->pbProductId = NULL;

	return productInfo;
}

/**
 * Free Product Information (PRODUCT_INFO).\n
 * @msdn{cc241915}
 * @param productInfo product information
 */

void license_free_product_info(PRODUCT_INFO* productInfo)
{
	if (productInfo->pbCompanyName != NULL)
		xfree(productInfo->pbCompanyName);

	if (productInfo->pbProductId != NULL)
		xfree(productInfo->pbProductId);

	xfree(productInfo);
}

/**
 * Read License Binary Blob (LICENSE_BINARY_BLOB).\n
 * @msdn{cc240481}
 * @param s stream
 * @param blob license binary blob
 */

void license_read_binary_blob(STREAM* s, LICENSE_BLOB* blob)
{
	uint16 wBlobType;

	stream_read_uint16(s, wBlobType); /* wBlobType (2 bytes) */

	if (blob->type != wBlobType && blob->type != BB_ANY_BLOB)
	{
		printf("license binary blob type does not match expected type.\n");
		return;
	}

	blob->type = wBlobType;

	stream_read_uint16(s, blob->length); /* wBlobLen (2 bytes) */

	blob->data = (uint8*) xmalloc(blob->length);

	stream_read(s, blob->data, blob->length); /* blobData */
}

/**
 * Write License Binary Blob (LICENSE_BINARY_BLOB).\n
 * @msdn{cc240481}
 * @param s stream
 * @param blob license binary blob
 */

void license_write_binary_blob(STREAM* s, LICENSE_BLOB* blob)
{
	stream_write_uint16(s, blob->type); /* wBlobType (2 bytes) */
	stream_write_uint16(s, blob->length); /* wBlobLen (2 bytes) */

	if (blob->length > 0)
		stream_write(s, blob->data, blob->length); /* blobData */
}

/**
 * Allocate New License Binary Blob (LICENSE_BINARY_BLOB).\n
 * @msdn{cc240481}
 * @return new license binary blob
 */

LICENSE_BLOB* license_new_binary_blob(uint16 type)
{
	LICENSE_BLOB* blob;

	blob = (LICENSE_BLOB*) xmalloc(sizeof(LICENSE_BLOB));
	blob->type = type;
	blob->length = 0;
	blob->data = NULL;

	return blob;
}

/**
 * Free License Binary Blob (LICENSE_BINARY_BLOB).\n
 * @msdn{cc240481}
 * @param blob license binary blob
 */

void license_free_binary_blob(LICENSE_BLOB* blob)
{
	if (blob->data != NULL)
		xfree(blob->data);

	xfree(blob);
}

/**
 * Read License Scope List (SCOPE_LIST).\n
 * @msdn{cc241916}
 * @param s stream
 * @param scopeList scope list
 */

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
 * Allocate New License Scope List (SCOPE_LIST).\n
 * @msdn{cc241916}
 * @return new scope list
 */

SCOPE_LIST* license_new_scope_list()
{
	SCOPE_LIST* scopeList;

	scopeList = (SCOPE_LIST*) xmalloc(sizeof(SCOPE_LIST));
	scopeList->count = 0;
	scopeList->array = NULL;

	return scopeList;
}

/**
 * Free License Scope List (SCOPE_LIST).\n
 * @msdn{cc241916}
 * @param scopeList scope list
 */

void license_free_scope_list(SCOPE_LIST* scopeList)
{
	int i;

	for (i = 0; i < scopeList->count; i++)
	{
		license_free_binary_blob(&scopeList->array[i]);
	}

	xfree(scopeList);
}

/**
 * Read a LICENSE_REQUEST packet.\n
 * @msdn{cc241914}
 * @param license license module
 * @param s stream
 */

void license_read_license_request_packet(rdpLicense* license, STREAM* s)
{
	printf("LICENSE_REQUEST\n");

	/* ServerRandom (32 bytes) */
	stream_read(s, license->server_random, 32);

	/* ProductInfo */
	license_read_product_info(s, license->product_info);

	/* KeyExchangeList */
	license_read_binary_blob(s, license->key_exchange_list);

	/* ServerCertificate */
	license_read_binary_blob(s, license->server_certificate);

	/* ScopeList */
	license_read_scope_list(s, license->scope_list);
}

/**
 * Read a PLATFORM_CHALLENGE packet.\n
 * @msdn{cc241921}
 * @param license license module
 * @param s stream
 */

void license_read_platform_challenge_packet(rdpLicense* license, STREAM* s)
{

}

/**
 * Read a NEW_LICENSE packet.\n
 * @msdn{cc241926}
 * @param license license module
 * @param s stream
 */

void license_read_new_license_packet(rdpLicense* license, STREAM* s)
{

}

/**
 * Read an UPGRADE_LICENSE packet.\n
 * @msdn{cc241924}
 * @param license license module
 * @param s stream
 */

void license_read_upgrade_license_packet(rdpLicense* license, STREAM* s)
{

}

/**
 * Read an ERROR_ALERT packet.\n
 * @msdn{cc240482}
 * @param license license module
 * @param s stream
 */

void license_read_error_alert_packet(rdpLicense* license, STREAM* s)
{

}

/**
 * Write Platform ID.\n
 * @msdn{cc241918}
 * @param license license module
 * @param s stream
 */

void license_write_platform_id(rdpLicense* license, STREAM* s)
{
	stream_write_uint8(s, 0); /* Client Operating System Version */
	stream_write_uint8(s, 0); /* Independent Software Vendor (ISV) */
	stream_write_uint16(s, 0x0201); /* Client Software Build */
}

/**
 * Write a NEW_LICENSE_REQUEST packet.\n
 * @msdn{cc241918}
 * @param license license module
 * @param s stream
 */

void license_write_new_license_request_packet(rdpLicense* license, STREAM* s)
{
	stream_write_uint32(s, KEY_EXCHANGE_ALG_RSA); /* PreferredKeyExchangeAlg (4 bytes) */
	license_write_platform_id(license, s); /* PlatformId (4 bytes) */
	stream_write(s, license->client_random, 32); /* ClientRandom (32 bytes) */
	license_write_binary_blob(s, license->encrypted_pre_master_secret); /* EncryptedPreMasterSecret */
	license_write_binary_blob(s, license->client_user_name); /* ClientUserName */
	license_write_binary_blob(s, license->client_machine_name); /* ClientMachineName */
}

/**
 * Send a NEW_LICENSE_REQUEST packet.\n
 * @msdn{cc241918}
 * @param license license module
 */

void license_send_new_license_request_packet(rdpLicense* license)
{
	STREAM* s;

	s = license_send_stream_init(license);

	/* TODO: generate keys */

	license_write_new_license_request_packet(license, s);

	license_send(license, s, NEW_LICENSE_REQUEST);
}

/**
 * Instantiate new license module.
 * @return new license module
 */

rdpLicense* license_new(rdpRdp* rdp)
{
	rdpLicense* license;

	license = (rdpLicense*) xzalloc(sizeof(rdpLicense));

	if (license != NULL)
	{
		license->rdp = rdp;
		license->product_info = license_new_product_info();
		license->key_exchange_list = license_new_binary_blob(BB_KEY_EXCHG_ALG_BLOB);
		license->server_certificate = license_new_binary_blob(BB_CERTIFICATE_BLOB);
		license->encrypted_pre_master_secret = license_new_binary_blob(BB_RANDOM_BLOB);
		license->client_user_name = license_new_binary_blob(BB_CLIENT_USER_NAME_BLOB);
		license->client_machine_name = license_new_binary_blob(BB_CLIENT_MACHINE_NAME_BLOB);
		license->scope_list = license_new_scope_list();
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
		license_free_product_info(license->product_info);
		license_free_binary_blob(license->key_exchange_list);
		license_free_binary_blob(license->server_certificate);
		license_free_binary_blob(license->encrypted_pre_master_secret);
		license_free_binary_blob(license->client_user_name);
		license_free_binary_blob(license->client_machine_name);
		license_free_scope_list(license->scope_list);
		xfree(license);
	}
}

