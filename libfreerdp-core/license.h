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

#ifndef __LICENSE_H
#define __LICENSE_H

typedef struct rdp_license rdpLicense;

#include "rdp.h"
#include "crypto.h"
#include "certificate.h"

#include <freerdp/freerdp.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>

/* Licensing Packet Types */
#define LICENSE_REQUEST				0x01
#define PLATFORM_CHALLENGE			0x02
#define NEW_LICENSE				0x03
#define UPGRADE_LICENSE				0x04
#define LICENSE_INFO				0x12
#define NEW_LICENSE_REQUEST			0x13
#define PLATFORM_CHALLENGE_RESPONSE		0x15
#define ERROR_ALERT				0xFF

#define LICENSE_PKT_CS_MASK	(LICENSE_INFO | NEW_LICENSE_REQUEST | PLATFORM_CHALLENGE_RESPONSE | ERROR_ALERT)
#define LICENSE_PKT_SC_MASK	(LICENSE_REQUEST | PLATFORM_CHALLENGE | NEW_LICENSE | UPGRADE_LICENSE | ERROR_ALERT)
#define LICENSE_PKT_MASK	(LICENSE_PKT_CS_MASK | LICENSE_PKT_SC_MASK)

#define LICENSE_PREAMBLE_LENGTH			4
#define LICENSE_PACKET_HEADER_MAX_LENGTH	(RDP_PACKET_HEADER_MAX_LENGTH + RDP_SECURITY_HEADER_LENGTH + LICENSE_PREAMBLE_LENGTH)

/* Cryptographic Lengths */
#define CLIENT_RANDOM_LENGTH			32
#define SERVER_RANDOM_LENGTH			32
#define MASTER_SECRET_LENGTH			48
#define PREMASTER_SECRET_LENGTH			48
#define SESSION_KEY_BLOB_LENGTH			48
#define MAC_SALT_KEY_LENGTH			16
#define LICENSING_ENCRYPTION_KEY_LENGTH		16
#define HWID_PLATFORM_ID_LENGTH			4
#define HWID_UNIQUE_DATA_LENGTH			16
#define HWID_LENGTH				20
#define LICENSING_PADDING_SIZE			8

/* Licensing Preamble Flags */
#define PREAMBLE_VERSION_2_0			0x02
#define PREAMBLE_VERSION_3_0			0x03
#define LicenseProtocolVersionMask		0x0F
#define EXTENDED_ERROR_MSG_SUPPORTED		0x80

/* Licensing Binary Blob Types */
#define BB_ANY_BLOB				0x0000
#define BB_DATA_BLOB				0x0001
#define BB_RANDOM_BLOB				0x0002
#define BB_CERTIFICATE_BLOB			0x0003
#define BB_ERROR_BLOB				0x0004
#define BB_ENCRYPTED_DATA_BLOB			0x0009
#define BB_KEY_EXCHG_ALG_BLOB			0x000D
#define BB_SCOPE_BLOB				0x000E
#define BB_CLIENT_USER_NAME_BLOB		0x000F
#define BB_CLIENT_MACHINE_NAME_BLOB		0x0010

/* Key Exchange Algorithms */
#define KEY_EXCHANGE_ALG_RSA			0x00000001

/* Licensing Error Codes */
#define ERR_INVALID_SERVER_CERTIFICATE		0x00000001
#define ERR_NO_LICENSE				0x00000002
#define ERR_INVALID_MAC				0x00000003
#define ERR_INVALID_SCOPE			0x00000004
#define ERR_NO_LICENSE_SERVER			0x00000006
#define STATUS_VALID_CLIENT			0x00000007
#define ERR_INVALID_CLIENT			0x00000008
#define ERR_INVALID_PRODUCT_ID			0x0000000B
#define ERR_INVALID_MESSAGE_LENGTH		0x0000000C

/* Licensing State Transition Codes */
#define ST_TOTAL_ABORT				0x00000001
#define ST_NO_TRANSITION			0x00000002
#define ST_RESET_PHASE_TO_START			0x00000003
#define ST_RESEND_LAST_MESSAGE			0x00000004

typedef struct
{
	uint32 dwVersion;
	uint32 cbCompanyName;
	uint8* pbCompanyName;
	uint32 cbProductId;
	uint8* pbProductId;
} PRODUCT_INFO;

typedef struct
{
	uint16 type;
	uint16 length;
	uint8* data;
} LICENSE_BLOB;

typedef struct
{
	uint32 count;
	LICENSE_BLOB* array;
} SCOPE_LIST;

typedef enum
{
	LICENSE_STATE_AWAIT,
	LICENSE_STATE_PROCESS,
	LICENSE_STATE_ABORTED,
	LICENSE_STATE_COMPLETED
} LICENSE_STATE;

struct rdp_license
{
	LICENSE_STATE state;
	struct rdp_rdp* rdp;
	struct rdp_certificate* certificate;
	uint8 hwid[HWID_LENGTH];
	uint8 modulus[MODULUS_MAX_SIZE];
	uint8 exponent[EXPONENT_MAX_SIZE];
	uint8 client_random[CLIENT_RANDOM_LENGTH];
	uint8 server_random[SERVER_RANDOM_LENGTH];
	uint8 master_secret[MASTER_SECRET_LENGTH];
	uint8 premaster_secret[PREMASTER_SECRET_LENGTH];
	uint8 session_key_blob[SESSION_KEY_BLOB_LENGTH];
	uint8 mac_salt_key[MAC_SALT_KEY_LENGTH];
	uint8 licensing_encryption_key[LICENSING_ENCRYPTION_KEY_LENGTH];
	PRODUCT_INFO* product_info;
	LICENSE_BLOB* error_info;
	LICENSE_BLOB* key_exchange_list;
	LICENSE_BLOB* server_certificate;
	LICENSE_BLOB* client_user_name;
	LICENSE_BLOB* client_machine_name;
	LICENSE_BLOB* platform_challenge;
	LICENSE_BLOB* encrypted_premaster_secret;
	LICENSE_BLOB* encrypted_platform_challenge;
	LICENSE_BLOB* encrypted_hwid;
	SCOPE_LIST* scope_list;
};

boolean license_recv(rdpLicense* license, STREAM* s);
boolean license_send(rdpLicense* license, STREAM* s, uint8 type);
STREAM* license_send_stream_init(rdpLicense* license);

void license_generate_randoms(rdpLicense* license);
void license_generate_keys(rdpLicense* license);
void license_generate_hwid(rdpLicense* license);
void license_encrypt_premaster_secret(rdpLicense* license);
void license_decrypt_platform_challenge(rdpLicense* license);

PRODUCT_INFO* license_new_product_info();
void license_free_product_info(PRODUCT_INFO* productInfo);
void license_read_product_info(STREAM* s, PRODUCT_INFO* productInfo);

LICENSE_BLOB* license_new_binary_blob(uint16 type);
void license_free_binary_blob(LICENSE_BLOB* blob);
void license_read_binary_blob(STREAM* s, LICENSE_BLOB* blob);
void license_write_binary_blob(STREAM* s, LICENSE_BLOB* blob);

SCOPE_LIST* license_new_scope_list();
void license_free_scope_list(SCOPE_LIST* scopeList);
void license_read_scope_list(STREAM* s, SCOPE_LIST* scopeList);

void license_read_license_request_packet(rdpLicense* license, STREAM* s);
void license_read_platform_challenge_packet(rdpLicense* license, STREAM* s);
void license_read_new_license_packet(rdpLicense* license, STREAM* s);
void license_read_upgrade_license_packet(rdpLicense* license, STREAM* s);
void license_read_error_alert_packet(rdpLicense* license, STREAM* s);

void license_write_new_license_request_packet(rdpLicense* license, STREAM* s);
void license_send_new_license_request_packet(rdpLicense* license);

void license_write_platform_challenge_response_packet(rdpLicense* license, STREAM* s, uint8* mac_data);
void license_send_platform_challenge_response_packet(rdpLicense* license);

boolean license_send_valid_client_error_packet(rdpLicense* license);

rdpLicense* license_new(rdpRdp* rdp);
void license_free(rdpLicense* license);

#ifdef WITH_DEBUG_LICENSE
#define DEBUG_LICENSE(fmt, ...) DEBUG_CLASS(LICENSE, fmt, ## __VA_ARGS__)
#else
#define DEBUG_LICENSE(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __LICENSE_H */
