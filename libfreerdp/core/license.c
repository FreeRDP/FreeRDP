/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include "redirection.h"
#include "certificate.h"

#include "license.h"

#ifdef WITH_DEBUG_LICENSE
static const char* const LICENSE_MESSAGE_STRINGS[] =
{
		"",
		"License Request",
		"Platform Challenge",
		"New License",
		"Upgrade License",
		"", "", "", "", "", "",
		"", "", "", "", "", "",
		"",
		"License Info",
		"New License Request",
		"",
		"Platform Challenge Response",
		"", "", "", "", "", "", "", "", "",
		"Error Alert"
};

static const char* const error_codes[] =
{
	"ERR_UNKNOWN",
	"ERR_INVALID_SERVER_CERTIFICATE",
	"ERR_NO_LICENSE",
	"ERR_INVALID_MAC",
	"ERR_INVALID_SCOPE",
	"ERR_UNKNOWN",
	"ERR_NO_LICENSE_SERVER",
	"STATUS_VALID_CLIENT",
	"ERR_INVALID_CLIENT",
	"ERR_UNKNOWN",
	"ERR_UNKNOWN",
	"ERR_INVALID_PRODUCT_ID",
	"ERR_INVALID_MESSAGE_LENGTH"
};

static const char* const  state_transitions[] =
{
	"ST_UNKNOWN",
	"ST_TOTAL_ABORT",
	"ST_NO_TRANSITION",
	"ST_RESET_PHASE_TO_START",
	"ST_RESEND_LAST_MESSAGE"
};
#endif

/**
 * Read a licensing preamble.\n
 * @msdn{cc240480}
 * @param s stream
 * @param bMsgType license message type
 * @param flags message flags
 * @param wMsgSize message size
 * @return if the operation completed successfully
 */

BOOL license_read_preamble(STREAM* s, BYTE* bMsgType, BYTE* flags, UINT16* wMsgSize)
{
	/* preamble (4 bytes) */
	if(stream_get_left(s) < 4)
		return FALSE;
	stream_read_BYTE(s, *bMsgType); /* bMsgType (1 byte) */
	stream_read_BYTE(s, *flags); /* flags (1 byte) */
	stream_read_UINT16(s, *wMsgSize); /* wMsgSize (2 bytes) */
	return TRUE;
}

/**
 * Write a licensing preamble.\n
 * @msdn{cc240480}
 * @param s stream
 * @param bMsgType license message type
 * @param flags message flags
 * @param wMsgSize message size
 */

void license_write_preamble(STREAM* s, BYTE bMsgType, BYTE flags, UINT16 wMsgSize)
{
	/* preamble (4 bytes) */
	stream_write_BYTE(s, bMsgType); /* bMsgType (1 byte) */
	stream_write_BYTE(s, flags); /* flags (1 byte) */
	stream_write_UINT16(s, wMsgSize); /* wMsgSize (2 bytes) */
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
	stream_seek(s, LICENSE_PACKET_HEADER_MAX_LENGTH);
	return s;
}

/**
 * Send an RDP licensing packet.\n
 * @msdn{cc240479}
 * @param license license module
 * @param s stream
 */

BOOL license_send(rdpLicense* license, STREAM* s, BYTE type)
{
	int length;
	BYTE flags;
	UINT16 wMsgSize;
	UINT16 sec_flags;

	DEBUG_LICENSE("Sending %s Packet", LICENSE_MESSAGE_STRINGS[type & 0x1F]);

	length = stream_get_length(s);
	stream_set_pos(s, 0);

	sec_flags = SEC_LICENSE_PKT;
	wMsgSize = length - LICENSE_PACKET_HEADER_MAX_LENGTH + 4;
	/**
	 * Using EXTENDED_ERROR_MSG_SUPPORTED here would cause mstsc to crash when
	 * running in server mode! This flag seems to be incorrectly documented.
	 */
	flags = PREAMBLE_VERSION_3_0;

	rdp_write_header(license->rdp, s, length, MCS_GLOBAL_CHANNEL_ID);
	rdp_write_security_header(s, sec_flags);
	license_write_preamble(s, type, flags, wMsgSize);

#ifdef WITH_DEBUG_LICENSE
	printf("Sending %s Packet, length %d\n", LICENSE_MESSAGE_STRINGS[type & 0x1F], wMsgSize);
	winpr_HexDump(s->p - 4, wMsgSize);
#endif

	stream_set_pos(s, length);
	if (transport_write(license->rdp->transport, s) < 0)
		return FALSE;

	return TRUE;
}

/**
 * Receive an RDP licensing packet.\n
 * @msdn{cc240479}
 * @param license license module
 * @param s stream
 * @return if the operation completed successfully
 */

BOOL license_recv(rdpLicense* license, STREAM* s)
{
	UINT16 length;
	UINT16 channelId;
	UINT16 sec_flags;
	BYTE flags;
	BYTE bMsgType;
	UINT16 wMsgSize;

	if (!rdp_read_header(license->rdp, s, &length, &channelId))
	{
		printf("Incorrect RDP header.\n");
		return FALSE;
	}

	if (!rdp_read_security_header(s, &sec_flags))
		return FALSE;

	if (!(sec_flags & SEC_LICENSE_PKT))
	{
		stream_rewind(s, RDP_SECURITY_HEADER_LENGTH);
		if (rdp_recv_out_of_sequence_pdu(license->rdp, s) != TRUE)
		{
			printf("Unexpected license packet.\n");
			return FALSE;
		}
		return TRUE;
	}

	if (!license_read_preamble(s, &bMsgType, &flags, &wMsgSize)) /* preamble (4 bytes) */
		return FALSE;

	DEBUG_LICENSE("Receiving %s Packet", LICENSE_MESSAGE_STRINGS[bMsgType & 0x1F]);

	switch (bMsgType)
	{
		case LICENSE_REQUEST:
			if (!license_read_license_request_packet(license, s))
				return FALSE;
			license_send_new_license_request_packet(license);
			break;

		case PLATFORM_CHALLENGE:
			if (!license_read_platform_challenge_packet(license, s))
				return FALSE;
			license_send_platform_challenge_response_packet(license);
			break;

		case NEW_LICENSE:
			license_read_new_license_packet(license, s);
			break;

		case UPGRADE_LICENSE:
			license_read_upgrade_license_packet(license, s);
			break;

		case ERROR_ALERT:
			if (!license_read_error_alert_packet(license, s))
				return FALSE;
			break;

		default:
			printf("invalid bMsgType:%d\n", bMsgType);
			return FALSE;
	}

	return TRUE;
}

void license_generate_randoms(rdpLicense* license)
{
#if 0
	crypto_nonce(license->client_random, CLIENT_RANDOM_LENGTH); /* ClientRandom */
	crypto_nonce(license->premaster_secret, PREMASTER_SECRET_LENGTH); /* PremasterSecret */
#else
	memset(license->client_random, 0, CLIENT_RANDOM_LENGTH); /* ClientRandom */
	memset(license->premaster_secret, 0, PREMASTER_SECRET_LENGTH); /* PremasterSecret */
#endif
}

/**
 * Generate License Cryptographic Keys.
 * @param license license module
 */

void license_generate_keys(rdpLicense* license)
{
	security_master_secret(license->premaster_secret, license->client_random,
			license->server_random, license->master_secret); /* MasterSecret */

	security_session_key_blob(license->master_secret, license->client_random,
			license->server_random, license->session_key_blob); /* SessionKeyBlob */

	security_mac_salt_key(license->session_key_blob, license->client_random,
			license->server_random, license->mac_salt_key); /* MacSaltKey */

	security_licensing_encryption_key(license->session_key_blob, license->client_random,
			license->server_random, license->licensing_encryption_key); /* LicensingEncryptionKey */

#ifdef WITH_DEBUG_LICENSE
	printf("ClientRandom:\n");
	winpr_HexDump(license->client_random, CLIENT_RANDOM_LENGTH);

	printf("ServerRandom:\n");
	winpr_HexDump(license->server_random, SERVER_RANDOM_LENGTH);

	printf("PremasterSecret:\n");
	winpr_HexDump(license->premaster_secret, PREMASTER_SECRET_LENGTH);

	printf("MasterSecret:\n");
	winpr_HexDump(license->master_secret, MASTER_SECRET_LENGTH);

	printf("SessionKeyBlob:\n");
	winpr_HexDump(license->session_key_blob, SESSION_KEY_BLOB_LENGTH);

	printf("MacSaltKey:\n");
	winpr_HexDump(license->mac_salt_key, MAC_SALT_KEY_LENGTH);

	printf("LicensingEncryptionKey:\n");
	winpr_HexDump(license->licensing_encryption_key, LICENSING_ENCRYPTION_KEY_LENGTH);
#endif
}

/**
 * Generate Unique Hardware Identifier (CLIENT_HARDWARE_ID).\n
 * @param license license module
 */

void license_generate_hwid(rdpLicense* license)
{
	CryptoMd5 md5;
	BYTE* mac_address;

	memset(license->hwid, 0, HWID_LENGTH);
	mac_address = license->rdp->transport->TcpIn->mac_address;

	md5 = crypto_md5_init();
	crypto_md5_update(md5, mac_address, 6);
	crypto_md5_final(md5, &license->hwid[HWID_PLATFORM_ID_LENGTH]);
}

void license_encrypt_premaster_secret(rdpLicense* license)
{
	BYTE* encrypted_premaster_secret;
#if 0
	int key_length;
	BYTE* modulus;
	BYTE* exponent;
	rdpCertificate *certificate;

	if (license->server_certificate->length)
		certificate = license->certificate;
	else
		certificate = license->rdp->settings->server_cert;

	exponent = certificate->cert_info.exponent;
	modulus = certificate->cert_info.modulus.data;
	key_length = certificate->cert_info.modulus.length;

#ifdef WITH_DEBUG_LICENSE
	printf("modulus (%d bits):\n", key_length * 8);
	winpr_HexDump(modulus, key_length);

	printf("exponent:\n");
	winpr_HexDump(exponent, 4);
#endif

	encrypted_premaster_secret = (BYTE*) malloc(MODULUS_MAX_SIZE);
	memset(encrypted_premaster_secret, 0, MODULUS_MAX_SIZE);

	crypto_rsa_public_encrypt(license->premaster_secret, PREMASTER_SECRET_LENGTH,
			key_length, modulus, exponent, encrypted_premaster_secret);

	license->encrypted_premaster_secret->type = BB_RANDOM_BLOB;
	license->encrypted_premaster_secret->length = PREMASTER_SECRET_LENGTH;
	license->encrypted_premaster_secret->data = encrypted_premaster_secret;
#else
	encrypted_premaster_secret = (BYTE*) malloc(MODULUS_MAX_SIZE);
	memset(encrypted_premaster_secret, 0, MODULUS_MAX_SIZE);

	license->encrypted_premaster_secret->type = BB_RANDOM_BLOB;
	license->encrypted_premaster_secret->length = PREMASTER_SECRET_LENGTH;
	license->encrypted_premaster_secret->data = encrypted_premaster_secret;
#endif
}

void license_decrypt_platform_challenge(rdpLicense* license)
{
	CryptoRc4 rc4;

	license->platform_challenge->data =
			(BYTE*) malloc(license->encrypted_platform_challenge->length);
	license->platform_challenge->length =
			license->encrypted_platform_challenge->length;

	rc4 = crypto_rc4_init(license->licensing_encryption_key, LICENSING_ENCRYPTION_KEY_LENGTH);

	crypto_rc4(rc4, license->encrypted_platform_challenge->length,
			license->encrypted_platform_challenge->data,
			license->platform_challenge->data);

#ifdef WITH_DEBUG_LICENSE
	printf("encrypted_platform challenge:\n");
	winpr_HexDump(license->encrypted_platform_challenge->data,
			license->encrypted_platform_challenge->length);

	printf("platform challenge:\n");
	winpr_HexDump(license->platform_challenge->data, license->platform_challenge->length);
#endif

	crypto_rc4_free(rc4);
}

/**
 * Read Product Information (PRODUCT_INFO).\n
 * @msdn{cc241915}
 * @param s stream
 * @param productInfo product information
 */

BOOL license_read_product_info(STREAM* s, PRODUCT_INFO* productInfo)
{
	if(stream_get_left(s) < 8)
		return FALSE;
	stream_read_UINT32(s, productInfo->dwVersion); /* dwVersion (4 bytes) */

	stream_read_UINT32(s, productInfo->cbCompanyName); /* cbCompanyName (4 bytes) */
	if(stream_get_left(s) < productInfo->cbCompanyName + 4)
		return FALSE;

	productInfo->pbCompanyName = (BYTE*) malloc(productInfo->cbCompanyName);
	stream_read(s, productInfo->pbCompanyName, productInfo->cbCompanyName);

	stream_read_UINT32(s, productInfo->cbProductId); /* cbProductId (4 bytes) */
	if(stream_get_left(s) < productInfo->cbProductId)
	{
		free(productInfo->pbCompanyName);
		productInfo->pbCompanyName = NULL;
		return FALSE;
	}

	productInfo->pbProductId = (BYTE*) malloc(productInfo->cbProductId);
	stream_read(s, productInfo->pbProductId, productInfo->cbProductId);
	return TRUE;
}

/**
 * Allocate New Product Information (PRODUCT_INFO).\n
 * @msdn{cc241915}
 * @return new product information
 */

PRODUCT_INFO* license_new_product_info()
{
	PRODUCT_INFO* productInfo;

	productInfo = (PRODUCT_INFO*) malloc(sizeof(PRODUCT_INFO));

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
		free(productInfo->pbCompanyName);

	if (productInfo->pbProductId != NULL)
		free(productInfo->pbProductId);

	free(productInfo);
}

/**
 * Read License Binary Blob (LICENSE_BINARY_BLOB).\n
 * @msdn{cc240481}
 * @param s stream
 * @param blob license binary blob
 */

BOOL license_read_binary_blob(STREAM* s, LICENSE_BLOB* blob)
{
	UINT16 wBlobType;

	if(stream_get_left(s) < 4)
		return FALSE;
	stream_read_UINT16(s, wBlobType); /* wBlobType (2 bytes) */
	stream_read_UINT16(s, blob->length); /* wBlobLen (2 bytes) */

	if(stream_get_left(s) < blob->length)
		return FALSE;

	/*
 	 * Server can choose to not send data by setting len to 0.
 	 * If so, it may not bother to set the type, so shortcut the warning
 	 */
	if (blob->type != BB_ANY_BLOB && blob->length == 0)
		return TRUE;

	if (blob->type != wBlobType && blob->type != BB_ANY_BLOB)
	{
		printf("license binary blob type (%x) does not match expected type (%x).\n", wBlobType, blob->type);
	}

	blob->type = wBlobType;
	blob->data = (BYTE*) malloc(blob->length);

	stream_read(s, blob->data, blob->length); /* blobData */
	return TRUE;
}

/**
 * Write License Binary Blob (LICENSE_BINARY_BLOB).\n
 * @msdn{cc240481}
 * @param s stream
 * @param blob license binary blob
 */

void license_write_binary_blob(STREAM* s, LICENSE_BLOB* blob)
{
	stream_write_UINT16(s, blob->type); /* wBlobType (2 bytes) */
	stream_write_UINT16(s, blob->length); /* wBlobLen (2 bytes) */

	if (blob->length > 0)
		stream_write(s, blob->data, blob->length); /* blobData */
}

void license_write_padded_binary_blob(STREAM* s, LICENSE_BLOB* blob)
{
	UINT16 pad_len;

	pad_len = 72 % blob->length;
	stream_write_UINT16(s, blob->type); /* wBlobType (2 bytes) */
	stream_write_UINT16(s, blob->length + pad_len); /* wBlobLen (2 bytes) */

	if (blob->length > 0)
		stream_write(s, blob->data, blob->length); /* blobData */

	stream_write_zero(s, pad_len);
}

/**
 * Allocate New License Binary Blob (LICENSE_BINARY_BLOB).\n
 * @msdn{cc240481}
 * @return new license binary blob
 */

LICENSE_BLOB* license_new_binary_blob(UINT16 type)
{
	LICENSE_BLOB* blob;

	blob = (LICENSE_BLOB*) malloc(sizeof(LICENSE_BLOB));
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
		free(blob->data);

	free(blob);
}

/**
 * Read License Scope List (SCOPE_LIST).\n
 * @msdn{cc241916}
 * @param s stream
 * @param scopeList scope list
 */

BOOL license_read_scope_list(STREAM* s, SCOPE_LIST* scopeList)
{
	UINT32 i;
	UINT32 scopeCount;

	if(stream_get_left(s) < 4)
		return FALSE;
	stream_read_UINT32(s, scopeCount); /* ScopeCount (4 bytes) */

	scopeList->count = scopeCount;
	scopeList->array = (LICENSE_BLOB*) malloc(sizeof(LICENSE_BLOB) * scopeCount);

	/* ScopeArray */
	for (i = 0; i < scopeCount; i++)
	{
		scopeList->array[i].type = BB_SCOPE_BLOB;
		if(!license_read_binary_blob(s, &scopeList->array[i]))
			return FALSE;
	}
	return TRUE;
}

/**
 * Allocate New License Scope List (SCOPE_LIST).\n
 * @msdn{cc241916}
 * @return new scope list
 */

SCOPE_LIST* license_new_scope_list()
{
	SCOPE_LIST* scopeList;

	scopeList = (SCOPE_LIST*) malloc(sizeof(SCOPE_LIST));
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
	UINT32 i;

	/*
	 * We must NOT call license_free_binary_blob() on each scopelist->array[i] element,
	 * because scopelist->array was allocated at once, by a single call to malloc. The elements
	 * it contains cannot be deallocated separately then.
	 * To make things clean, we must deallocate each scopelist->array[].data,
	 * and finish by deallocating scopelist->array with a single call to free().
	 */
	for (i = 0; i < scopeList->count; i++)
	{
		free(scopeList->array[i].data);
	}

	free(scopeList->array) ;
	free(scopeList);
}

/**
 * Read a LICENSE_REQUEST packet.\n
 * @msdn{cc241914}
 * @param license license module
 * @param s stream
 */

BOOL license_read_license_request_packet(rdpLicense* license, STREAM* s)
{
	/* ServerRandom (32 bytes) */
	if (stream_get_left(s) < 32)
		return FALSE;

	stream_read(s, license->server_random, 32);

	/* ProductInfo */
	if (!license_read_product_info(s, license->product_info))
		return FALSE;

	/* KeyExchangeList */
	if (!license_read_binary_blob(s, license->key_exchange_list))
		return FALSE;

	/* ServerCertificate */
	if (!license_read_binary_blob(s, license->server_certificate))
		return FALSE;

	/* ScopeList */
	if (!license_read_scope_list(s, license->scope_list))
		return FALSE;

	/* Parse Server Certificate */
	if (!certificate_read_server_certificate(license->certificate,
			license->server_certificate->data, license->server_certificate->length) < 0)
		return FALSE;

	license_generate_keys(license);
	license_generate_hwid(license);
	license_encrypt_premaster_secret(license);

	return TRUE;
}

/**
 * Read a PLATFORM_CHALLENGE packet.\n
 * @msdn{cc241921}
 * @param license license module
 * @param s stream
 */

BOOL license_read_platform_challenge_packet(rdpLicense* license, STREAM* s)
{
	DEBUG_LICENSE("Receiving Platform Challenge Packet");
	if(stream_get_left(s) < 4)
		return FALSE;
	stream_seek(s, 4); /* ConnectFlags, Reserved (4 bytes) */

	/* EncryptedPlatformChallenge */
	license->encrypted_platform_challenge->type = BB_ANY_BLOB;
	license_read_binary_blob(s, license->encrypted_platform_challenge);
	license->encrypted_platform_challenge->type = BB_ENCRYPTED_DATA_BLOB;

	/* MACData (16 bytes) */
	if(!stream_skip(s, 16))
		return FALSE;

	license_decrypt_platform_challenge(license);
	return TRUE;
}

/**
 * Read a NEW_LICENSE packet.\n
 * @msdn{cc241926}
 * @param license license module
 * @param s stream
 */

void license_read_new_license_packet(rdpLicense* license, STREAM* s)
{
	DEBUG_LICENSE("Receiving New License Packet");
	license->state = LICENSE_STATE_COMPLETED;
}

/**
 * Read an UPGRADE_LICENSE packet.\n
 * @msdn{cc241924}
 * @param license license module
 * @param s stream
 */

void license_read_upgrade_license_packet(rdpLicense* license, STREAM* s)
{
	DEBUG_LICENSE("Receiving Upgrade License Packet");
	license->state = LICENSE_STATE_COMPLETED;
}

/**
 * Read an ERROR_ALERT packet.\n
 * @msdn{cc240482}
 * @param license license module
 * @param s stream
 */

BOOL license_read_error_alert_packet(rdpLicense* license, STREAM* s)
{
	UINT32 dwErrorCode;
	UINT32 dwStateTransition;

	if(stream_get_left(s) < 8)
		return FALSE;
	stream_read_UINT32(s, dwErrorCode); /* dwErrorCode (4 bytes) */
	stream_read_UINT32(s, dwStateTransition); /* dwStateTransition (4 bytes) */
	if(!license_read_binary_blob(s, license->error_info)) /* bbErrorInfo */
		return FALSE;

#ifdef WITH_DEBUG_LICENSE
	printf("dwErrorCode: %s, dwStateTransition: %s\n",
			error_codes[dwErrorCode], state_transitions[dwStateTransition]);
#endif

	if (dwErrorCode == STATUS_VALID_CLIENT)
	{
		license->state = LICENSE_STATE_COMPLETED;
		return TRUE;
	}

	switch (dwStateTransition)
	{
		case ST_TOTAL_ABORT:
			license->state = LICENSE_STATE_ABORTED;
			break;

		case ST_NO_TRANSITION:
			license->state = LICENSE_STATE_COMPLETED;
			break;

		case ST_RESET_PHASE_TO_START:
			license->state = LICENSE_STATE_AWAIT;
			break;

		case ST_RESEND_LAST_MESSAGE:
			break;

		default:
			break;
	}
	return TRUE;
}

/**
 * Write Platform ID.\n
 * @msdn{cc241918}
 * @param license license module
 * @param s stream
 */

void license_write_platform_id(rdpLicense* license, STREAM* s)
{
	stream_write_BYTE(s, 0); /* Client Operating System Version */
	stream_write_BYTE(s, 0); /* Independent Software Vendor (ISV) */
	stream_write_UINT16(s, 0); /* Client Software Build */
}

/**
 * Write a NEW_LICENSE_REQUEST packet.\n
 * @msdn{cc241918}
 * @param license license module
 * @param s stream
 */

void license_write_new_license_request_packet(rdpLicense* license, STREAM* s)
{
	stream_write_UINT32(s, KEY_EXCHANGE_ALG_RSA); /* PreferredKeyExchangeAlg (4 bytes) */
	license_write_platform_id(license, s); /* PlatformId (4 bytes) */
	stream_write(s, license->client_random, 32); /* ClientRandom (32 bytes) */
	license_write_padded_binary_blob(s, license->encrypted_premaster_secret); /* EncryptedPremasterSecret */
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
	char* username;

	s = license_send_stream_init(license);

	if (license->rdp->settings->Username != NULL)
		username = license->rdp->settings->Username;
	else
		username = "username";

	license->client_user_name->data = (BYTE*) username;
	license->client_user_name->length = strlen(username) + 1;

	license->client_machine_name->data = (BYTE*) license->rdp->settings->ClientHostname;
	license->client_machine_name->length = strlen(license->rdp->settings->ClientHostname) + 1;

	license_write_new_license_request_packet(license, s);

	license_send(license, s, NEW_LICENSE_REQUEST);

	license->client_user_name->data = NULL;
	license->client_user_name->length = 0;

	license->client_machine_name->data = NULL;
	license->client_machine_name->length = 0;
}

/**
 * Write Client Challenge Response Packet.\n
 * @msdn{cc241922}
 * @param license license module
 * @param s stream
 * @param mac_data signature
 */

void license_write_platform_challenge_response_packet(rdpLicense* license, STREAM* s, BYTE* mac_data)
{
	/* EncryptedPlatformChallengeResponse */
	license_write_binary_blob(s, license->encrypted_platform_challenge);

	/* EncryptedHWID */
	license_write_binary_blob(s, license->encrypted_hwid);

	/* MACData */
	stream_write(s, mac_data, 16);
}

/**
 * Send Client Challenge Response Packet.\n
 * @msdn{cc241922}
 * @param license license module
 */

void license_send_platform_challenge_response_packet(rdpLicense* license)
{
	STREAM* s;
	int length;
	BYTE* buffer;
	CryptoRc4 rc4;
	BYTE mac_data[16];

	s = license_send_stream_init(license);
	DEBUG_LICENSE("Sending Platform Challenge Response Packet");

	license->encrypted_platform_challenge->type = BB_DATA_BLOB;
	length = license->platform_challenge->length + HWID_LENGTH;
	buffer = (BYTE*) malloc(length);
	memcpy(buffer, license->platform_challenge->data, license->platform_challenge->length);
	memcpy(&buffer[license->platform_challenge->length], license->hwid, HWID_LENGTH);
	security_mac_data(license->mac_salt_key, buffer, length, mac_data);
	free(buffer);

	buffer = (BYTE*) malloc(HWID_LENGTH);
	rc4 = crypto_rc4_init(license->licensing_encryption_key, LICENSING_ENCRYPTION_KEY_LENGTH);
	crypto_rc4(rc4, HWID_LENGTH, license->hwid, buffer);
	crypto_rc4_free(rc4);

#ifdef WITH_DEBUG_LICENSE
	printf("Licensing Encryption Key:\n");
	winpr_HexDump(license->licensing_encryption_key, 16);

	printf("HardwareID:\n");
	winpr_HexDump(license->hwid, 20);

	printf("Encrypted HardwareID:\n");
	winpr_HexDump(buffer, 20);
#endif

	license->encrypted_hwid->type = BB_DATA_BLOB;
	license->encrypted_hwid->data = buffer;
	license->encrypted_hwid->length = HWID_LENGTH;

	license_write_platform_challenge_response_packet(license, s, mac_data);

	license_send(license, s, PLATFORM_CHALLENGE_RESPONSE);
}

/**
 * Send Server License Error - Valid Client Packet.\n
 * @msdn{cc241922}
 * @param license license module
 */

BOOL license_send_valid_client_error_packet(rdpLicense* license)
{
	STREAM* s;

	s = license_send_stream_init(license);

	stream_write_UINT32(s, STATUS_VALID_CLIENT); /* dwErrorCode */
	stream_write_UINT32(s, ST_NO_TRANSITION); /* dwStateTransition */

	license_write_binary_blob(s, license->error_info);

	return license_send(license, s, ERROR_ALERT);
}

/**
 * Instantiate new license module.
 * @param rdp RDP module
 * @return new license module
 */

rdpLicense* license_new(rdpRdp* rdp)
{
	rdpLicense* license;

	license = (rdpLicense*) malloc(sizeof(rdpLicense));

	if (license != NULL)
	{
		ZeroMemory(license, sizeof(rdpLicense));

		license->rdp = rdp;
		license->state = LICENSE_STATE_AWAIT;
		//license->certificate = certificate_new(rdp);
		license->certificate = certificate_new();
		license->product_info = license_new_product_info();
		license->error_info = license_new_binary_blob(BB_ERROR_BLOB);
		license->key_exchange_list = license_new_binary_blob(BB_KEY_EXCHG_ALG_BLOB);
		license->server_certificate = license_new_binary_blob(BB_CERTIFICATE_BLOB);
		license->client_user_name = license_new_binary_blob(BB_CLIENT_USER_NAME_BLOB);
		license->client_machine_name = license_new_binary_blob(BB_CLIENT_MACHINE_NAME_BLOB);
		license->platform_challenge = license_new_binary_blob(BB_ANY_BLOB);
		license->encrypted_platform_challenge = license_new_binary_blob(BB_ANY_BLOB);
		license->encrypted_premaster_secret = license_new_binary_blob(BB_ANY_BLOB);
		license->encrypted_hwid = license_new_binary_blob(BB_ENCRYPTED_DATA_BLOB);
		license->scope_list = license_new_scope_list();
		license_generate_randoms(license);
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
		certificate_free(license->certificate);
		license_free_product_info(license->product_info);
		license_free_binary_blob(license->error_info);
		license_free_binary_blob(license->key_exchange_list);
		license_free_binary_blob(license->server_certificate);
		license_free_binary_blob(license->client_user_name);
		license_free_binary_blob(license->client_machine_name);
		license_free_binary_blob(license->platform_challenge);
		license_free_binary_blob(license->encrypted_platform_challenge);
		license_free_binary_blob(license->encrypted_premaster_secret);
		license_free_binary_blob(license->encrypted_hwid);
		license_free_scope_list(license->scope_list);
		free(license);
	}
}

