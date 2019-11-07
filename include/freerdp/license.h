/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Licensing API
 *
 * Copyright 2018 David Fort <contact@hardening-consulting.com>
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

#ifndef FREERDP_LICENSE_H
#define FREERDP_LICENSE_H

#include <freerdp/api.h>

typedef struct rdp_license rdpLicense;

/** @brief Licensing Packet Types */
enum
{
	LICENSE_REQUEST = 0x01,
	PLATFORM_CHALLENGE = 0x02,
	NEW_LICENSE = 0x03,
	UPGRADE_LICENSE = 0x04,
	LICENSE_INFO = 0x12,
	NEW_LICENSE_REQUEST = 0x13,
	PLATFORM_CHALLENGE_RESPONSE = 0x15,
	ERROR_ALERT = 0xFF
};

#define LICENSE_PKT_CS_MASK \
	(LICENSE_INFO | NEW_LICENSE_REQUEST | PLATFORM_CHALLENGE_RESPONSE | ERROR_ALERT)
#define LICENSE_PKT_SC_MASK \
	(LICENSE_REQUEST | PLATFORM_CHALLENGE | NEW_LICENSE | UPGRADE_LICENSE | ERROR_ALERT)
#define LICENSE_PKT_MASK (LICENSE_PKT_CS_MASK | LICENSE_PKT_SC_MASK)

#define LICENSE_PREAMBLE_LENGTH 4

/* Cryptographic Lengths */

#define CLIENT_RANDOM_LENGTH 32
#define SERVER_RANDOM_LENGTH 32
#define MASTER_SECRET_LENGTH 48
#define PREMASTER_SECRET_LENGTH 48
#define SESSION_KEY_BLOB_LENGTH 48
#define MAC_SALT_KEY_LENGTH 16
#define LICENSING_ENCRYPTION_KEY_LENGTH 16
#define HWID_PLATFORM_ID_LENGTH 4
#define HWID_UNIQUE_DATA_LENGTH 16
#define HWID_LENGTH 20
#define LICENSING_PADDING_SIZE 8

/* Preamble Flags */

#define PREAMBLE_VERSION_2_0 0x02
#define PREAMBLE_VERSION_3_0 0x03
#define LicenseProtocolVersionMask 0x0F
#define EXTENDED_ERROR_MSG_SUPPORTED 0x80

/** @brief binary Blob Types */
enum
{
	BB_ANY_BLOB = 0x0000,
	BB_DATA_BLOB = 0x0001,
	BB_RANDOM_BLOB = 0x0002,
	BB_CERTIFICATE_BLOB = 0x0003,
	BB_ERROR_BLOB = 0x0004,
	BB_ENCRYPTED_DATA_BLOB = 0x0009,
	BB_KEY_EXCHG_ALG_BLOB = 0x000D,
	BB_SCOPE_BLOB = 0x000E,
	BB_CLIENT_USER_NAME_BLOB = 0x000F,
	BB_CLIENT_MACHINE_NAME_BLOB = 0x0010
};

/* License Key Exchange Algorithms */

#define KEY_EXCHANGE_ALG_RSA 0x00000001

/** @brief license Error Codes */
enum
{
	ERR_INVALID_SERVER_CERTIFICATE = 0x00000001,
	ERR_NO_LICENSE = 0x00000002,
	ERR_INVALID_MAC = 0x00000003,
	ERR_INVALID_SCOPE = 0x00000004,
	ERR_NO_LICENSE_SERVER = 0x00000006,
	STATUS_VALID_CLIENT = 0x00000007,
	ERR_INVALID_CLIENT = 0x00000008,
	ERR_INVALID_PRODUCT_ID = 0x0000000B,
	ERR_INVALID_MESSAGE_LENGTH = 0x0000000C
};

/** @brief state Transition Codes */
enum
{
	ST_TOTAL_ABORT = 0x00000001,
	ST_NO_TRANSITION = 0x00000002,
	ST_RESET_PHASE_TO_START = 0x00000003,
	ST_RESEND_LAST_MESSAGE = 0x00000004
};

/** @brief Platform Challenge Types */
enum
{
	WIN32_PLATFORM_CHALLENGE_TYPE = 0x0100,
	WIN16_PLATFORM_CHALLENGE_TYPE = 0x0200,
	WINCE_PLATFORM_CHALLENGE_TYPE = 0x0300,
	OTHER_PLATFORM_CHALLENGE_TYPE = 0xFF00
};

/** @brief License Detail Levels */
enum
{
	LICENSE_DETAIL_SIMPLE = 0x0001,
	LICENSE_DETAIL_MODERATE = 0x0002,
	LICENSE_DETAIL_DETAIL = 0x0003
};

/*
 * PlatformId:
 *
 * The most significant byte of the PlatformId field contains the operating system version of the
 * client. The second most significant byte of the PlatformId field identifies the ISV that provided
 * the client image. The remaining two bytes in the PlatformId field are used by the ISV to identify
 * the build number of the operating system.
 *
 * 0x04010000:
 *
 * CLIENT_OS_ID_WINNT_POST_52	(0x04000000)
 * CLIENT_IMAGE_ID_MICROSOFT	(0x00010000)
 */
enum
{
	CLIENT_OS_ID_WINNT_351 = 0x01000000,
	CLIENT_OS_ID_WINNT_40 = 0x02000000,
	CLIENT_OS_ID_WINNT_50 = 0x03000000,
	CLIENT_OS_ID_WINNT_POST_52 = 0x04000000,

	CLIENT_IMAGE_ID_MICROSOFT = 0x00010000,
	CLIENT_IMAGE_ID_CITRIX = 0x00020000,
};

#ifdef __cpluscplus
extern "C"
{
#endif

	FREERDP_API BOOL license_send_valid_client_error_packet(rdpRdp* rdp);

#ifdef __cpluscplus
}
#endif

#endif /* FREERDP_LICENSE_H */
