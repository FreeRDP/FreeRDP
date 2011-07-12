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

#include <freerdp/freerdp.h>
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

/* Licensing Preamble Flags */
#define PREAMBLE_VERSION_2_0			0x02
#define PREAMBLE_VERSION_3_0			0x03
#define LicenseProtocolVersionMask		0x0F
#define EXTENDED_ERROR_MSG_SUPPORTED		0x80

/* Licensing Binary Blob Types */
#define BB_DATA_BLOB				0x0001
#define BB_RANDOM_BLOB				0x0002
#define BB_CERTIFICATE_BLOB			0x0003
#define BB_ERROR_BLOB				0x0004
#define BB_ENCRYPTED_DATA_BLOB			0x0009
#define BB_KEY_EXCHG_ALG_BLOB			0x000D
#define BB_SCOPE_BLOB				0x000E
#define BB_CLIENT_USER_NAME_BLOB		0x000F
#define BB_CLIENT_MACHINE_NAME_BLOB		0x0010

#define KEY_EXCHANGE_ALG_RSA			0x00000001

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

struct rdp_license
{
	uint8 server_random[32];
	PRODUCT_INFO product_info;
	LICENSE_BLOB key_exchange_list;
	LICENSE_BLOB server_certificate;
	SCOPE_LIST scope_list;
};

void license_recv(rdpLicense* license, STREAM* s, uint16 sec_flags);

void license_read_product_info(STREAM* s, PRODUCT_INFO* productInfo);
void license_read_binary_blob(STREAM* s, LICENSE_BLOB* blob);

void license_read_license_request_packet(rdpLicense* license, STREAM* s);
void license_read_platform_challenge_packet(rdpLicense* license, STREAM* s);
void license_read_new_license_packet(rdpLicense* license, STREAM* s);
void license_read_upgrade_license_packet(rdpLicense* license, STREAM* s);
void license_read_error_alert_packet(rdpLicense* license, STREAM* s);

rdpLicense* license_new();
void license_free(rdpLicense* license);

#endif /* __LICENSE_H */
