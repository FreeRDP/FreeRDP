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

#ifndef FREERDP_LIB_CORE_LICENSE_H
#define FREERDP_LIB_CORE_LICENSE_H

#include "rdp.h"

#include <freerdp/license.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/crypto/certificate.h>

#include <freerdp/freerdp.h>
#include <freerdp/log.h>
#include <freerdp/api.h>
#include <freerdp/license.h>

#include <winpr/stream.h>

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
	LICENSE_BLOB* EncryptedPlatformChallengeResponse;
	LICENSE_BLOB* EncryptedHardwareId;
	SCOPE_LIST* ScopeList;
	UINT32 PacketHeaderLength;
};

FREERDP_LOCAL int license_recv(rdpLicense* license, wStream* s);

FREERDP_LOCAL rdpLicense* license_new(rdpRdp* rdp);
FREERDP_LOCAL void license_free(rdpLicense* license);

#define LICENSE_TAG FREERDP_TAG("core.license")
#ifdef WITH_DEBUG_LICENSE
#define DEBUG_LICENSE(...) WLog_DBG(LICENSE_TAG, __VA_ARGS__)
#else
#define DEBUG_LICENSE(...) \
	do                     \
	{                      \
	} while (0)
#endif

#endif /* FREERDP_LIB_CORE_LICENSE_H */
