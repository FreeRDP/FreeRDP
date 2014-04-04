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

#ifndef __LICENSE_H
#define __LICENSE_H

typedef struct rdp_license rdpLicense;

#include "rdp.h"

#include <freerdp/crypto/crypto.h>
#include <freerdp/crypto/certificate.h>

#include <freerdp/freerdp.h>
#include <freerdp/utils/debug.h>

#include <winpr/stream.h>

/* Licensing Packet Types */

#define LICENSE_REQUEST				0x01
#define PLATFORM_CHALLENGE			0x02
#define NEW_LICENSE				0x03
#define UPGRADE_LICENSE				0x04
#define LICENSE_INFO				0x12
#define NEW_LICENSE_REQUEST			0x13
#define PLATFORM_CHALLENGE_RESPONSE		0x15
#define ERROR_ALERT				0xFF

#define LICENSE_PKT_CS_MASK			(LICENSE_INFO | NEW_LICENSE_REQUEST | PLATFORM_CHALLENGE_RESPONSE | ERROR_ALERT)
#define LICENSE_PKT_SC_MASK			(LICENSE_REQUEST | PLATFORM_CHALLENGE | NEW_LICENSE | UPGRADE_LICENSE | ERROR_ALERT)
#define LICENSE_PKT_MASK			(LICENSE_PKT_CS_MASK | LICENSE_PKT_SC_MASK)

#define LICENSE_PREAMBLE_LENGTH			4

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

/* Preamble Flags */

#define PREAMBLE_VERSION_2_0			0x02
#define PREAMBLE_VERSION_3_0			0x03
#define LicenseProtocolVersionMask		0x0F
#define EXTENDED_ERROR_MSG_SUPPORTED		0x80

/* Binary Blob Types */

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

/* License Key Exchange Algorithms */

#define KEY_EXCHANGE_ALG_RSA			0x00000001

/* License Error Codes */

#define ERR_INVALID_SERVER_CERTIFICATE		0x00000001
#define ERR_NO_LICENSE				0x00000002
#define ERR_INVALID_MAC				0x00000003
#define ERR_INVALID_SCOPE			0x00000004
#define ERR_NO_LICENSE_SERVER			0x00000006
#define STATUS_VALID_CLIENT			0x00000007
#define ERR_INVALID_CLIENT			0x00000008
#define ERR_INVALID_PRODUCT_ID			0x0000000B
#define ERR_INVALID_MESSAGE_LENGTH		0x0000000C

/* State Transition Codes */

#define ST_TOTAL_ABORT				0x00000001
#define ST_NO_TRANSITION			0x00000002
#define ST_RESET_PHASE_TO_START			0x00000003
#define ST_RESEND_LAST_MESSAGE			0x00000004

/* Platform Challenge Types */

#define WIN32_PLATFORM_CHALLENGE_TYPE		0x0100
#define WIN16_PLATFORM_CHALLENGE_TYPE		0x0200
#define WINCE_PLATFORM_CHALLENGE_TYPE		0x0300
#define OTHER_PLATFORM_CHALLENGE_TYPE		0xFF00

/* License Detail Levels */

#define LICENSE_DETAIL_SIMPLE			0x0001
#define LICENSE_DETAIL_MODERATE			0x0002
#define LICENSE_DETAIL_DETAIL			0x0003

/*
 * PlatformId:
 *
 * The most significant byte of the PlatformId field contains the operating system version of the client.
 * The second most significant byte of the PlatformId field identifies the ISV that provided the client image.
 * The remaining two bytes in the PlatformId field are used by the ISV to identify the build number of the operating system.
 *
 * 0x04010000:
 *
 * CLIENT_OS_ID_WINNT_POST_52	(0x04000000)
 * CLIENT_IMAGE_ID_MICROSOFT	(0x00010000)
 */

#define CLIENT_OS_ID_WINNT_351			0x01000000
#define CLIENT_OS_ID_WINNT_40			0x02000000
#define CLIENT_OS_ID_WINNT_50			0x03000000
#define CLIENT_OS_ID_WINNT_POST_52		0x04000000

#define CLIENT_IMAGE_ID_MICROSOFT		0x00010000
#define CLIENT_IMAGE_ID_CITRIX			0x00020000

typedef struct
{
	UINT32 dwVersion;
	UINT32 cbCompanyName;
	BYTE* pbCompanyName;
	UINT32 cbProductId;
	BYTE* pbProductId;
} LICENSE_PRODUCT_INFO;

typedef struct
{
	UINT16 type;
	UINT16 length;
	BYTE* data;
} LICENSE_BLOB;

typedef struct
{
	UINT32 count;
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
	rdpRdp* rdp;
	rdpCertificate* certificate;
	BYTE* Modulus;
	UINT32 ModulusLength;
	BYTE Exponent[4];
	BYTE HardwareId[HWID_LENGTH];
	BYTE ClientRandom[CLIENT_RANDOM_LENGTH];
	BYTE ServerRandom[SERVER_RANDOM_LENGTH];
	BYTE MasterSecret[MASTER_SECRET_LENGTH];
	BYTE PremasterSecret[PREMASTER_SECRET_LENGTH];
	BYTE SessionKeyBlob[SESSION_KEY_BLOB_LENGTH];
	BYTE MacSaltKey[MAC_SALT_KEY_LENGTH];
	BYTE LicensingEncryptionKey[LICENSING_ENCRYPTION_KEY_LENGTH];
	LICENSE_PRODUCT_INFO* ProductInfo;
	LICENSE_BLOB* ErrorInfo;
	LICENSE_BLOB* KeyExchangeList;
	LICENSE_BLOB* ServerCertificate;
	LICENSE_BLOB* ClientUserName;
	LICENSE_BLOB* ClientMachineName;
	LICENSE_BLOB* PlatformChallenge;
	LICENSE_BLOB* EncryptedPremasterSecret;
	LICENSE_BLOB* EncryptedPlatformChallenge;
	LICENSE_BLOB* EncryptedHardwareId;
	SCOPE_LIST* ScopeList;
	UINT32 PacketHeaderLength;
};

int license_recv(rdpLicense* license, wStream* s);
BOOL license_send(rdpLicense* license, wStream* s, BYTE type);
wStream* license_send_stream_init(rdpLicense* license);

void license_generate_randoms(rdpLicense* license);
void license_generate_keys(rdpLicense* license);
void license_generate_hwid(rdpLicense* license);
void license_encrypt_premaster_secret(rdpLicense* license);
void license_decrypt_platform_challenge(rdpLicense* license);

LICENSE_PRODUCT_INFO* license_new_product_info(void);
void license_free_product_info(LICENSE_PRODUCT_INFO* productInfo);
BOOL license_read_product_info(wStream* s, LICENSE_PRODUCT_INFO* productInfo);

LICENSE_BLOB* license_new_binary_blob(UINT16 type);
void license_free_binary_blob(LICENSE_BLOB* blob);
BOOL license_read_binary_blob(wStream* s, LICENSE_BLOB* blob);
void license_write_binary_blob(wStream* s, LICENSE_BLOB* blob);

SCOPE_LIST* license_new_scope_list(void);
void license_free_scope_list(SCOPE_LIST* scopeList);
BOOL license_read_scope_list(wStream* s, SCOPE_LIST* scopeList);

BOOL license_read_license_request_packet(rdpLicense* license, wStream* s);
BOOL license_read_platform_challenge_packet(rdpLicense* license, wStream* s);
void license_read_new_license_packet(rdpLicense* license, wStream* s);
void license_read_upgrade_license_packet(rdpLicense* license, wStream* s);
BOOL license_read_error_alert_packet(rdpLicense* license, wStream* s);

void license_write_new_license_request_packet(rdpLicense* license, wStream* s);
void license_send_new_license_request_packet(rdpLicense* license);

void license_write_platform_challenge_response_packet(rdpLicense* license, wStream* s, BYTE* mac_data);
void license_send_platform_challenge_response_packet(rdpLicense* license);

BOOL license_send_valid_client_error_packet(rdpLicense* license);

rdpLicense* license_new(rdpRdp* rdp);
void license_free(rdpLicense* license);

#ifdef WITH_DEBUG_LICENSE
#define DEBUG_LICENSE(fmt, ...) DEBUG_CLASS(LICENSE, fmt, ## __VA_ARGS__)
#else
#define DEBUG_LICENSE(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __LICENSE_H */
