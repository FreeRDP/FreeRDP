/**
 * WinPR: Windows Portable Runtime
 * Virtual GIDS implementation
 *
 * Copyright 2021 Martin Fleisz <martin.fleisz@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#include <winpr/wlog.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include <openssl/bn.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>

#include <zlib.h>

#include "smartcard_virtual_gids.h"

#define TAG CHANNELS_TAG("smartcard.vgids")

#define VGIDS_EFID_MASTER 0xA000
#define VGIDS_EFID_COMMON 0xA010
#define VGIDS_EFID_CARDCF VGIDS_EFID_COMMON
#define VGIDS_EFID_CARDAPPS VGIDS_EFID_COMMON
#define VGIDS_EFID_CMAPFILE VGIDS_EFID_COMMON
#define VGIDS_EFID_CARDID 0xA012
#define VGIDS_EFID_KXC00 VGIDS_EFID_COMMON
#define VGIDS_EFID_CURRENTDF 0x3FFF

#define VGIDS_DO_FILESYSTEMTABLE 0xDF1F
#define VGIDS_DO_KEYMAP 0xDF20
#define VGIDS_DO_CARDID 0xDF20
#define VGIDS_DO_CARDAPPS 0xDF21
#define VGIDS_DO_CARDCF 0xDF22
#define VGIDS_DO_CMAPFILE 0xDF23
#define VGIDS_DO_KXC00 0xDF24

#define VGIDS_CARDID_SIZE 16
#define VGIDS_MAX_PIN_SIZE 127

#define VGIDS_DEFAULT_RETRY_COUNTER 3

#define VGIDS_KEY_TYPE_KEYEXCHANGE 0x9A
#define VGIDS_KEY_TYPE_SIGNATURE 0x9C

#define VGIDS_ALGID_RSA_1024 0x06
#define VGIDS_ALGID_RSA_2048 0x07
#define VGIDS_ALGID_RSA_3072 0x08
#define VGIDS_ALGID_RSA_4096 0x09

#define VGIDS_SE_CRT_AUTH 0xA4
#define VGIDS_SE_CRT_SIGN 0xB6
#define VGIDS_SE_CRT_CONF 0xB8

#define VGIDS_SE_ALGOID_CT_PAD_PKCS1 0x40
#define VGIDS_SE_ALGOID_CT_PAD_OAEP 0x80
#define VGIDS_SE_ALGOID_CT_RSA_1024 0x06
#define VGIDS_SE_ALGOID_CT_RSA_2048 0x07
#define VGIDS_SE_ALGOID_CT_RSA_3072 0x08
#define VGIDS_SE_ALGOID_CT_RSA_4096 0x09

#define VGIDS_SE_ALGOID_DST_PAD_PKCS1 0x40
#define VGIDS_SE_ALGOID_DST_RSA_1024 0x06
#define VGIDS_SE_ALGOID_DST_RSA_2048 0x07
#define VGIDS_SE_ALGOID_DST_RSA_3072 0x08
#define VGIDS_SE_ALGOID_DST_RSA_4096 0x09
#define VGIDS_SE_ALGOID_DST_ECDSA_P192 0x0A
#define VGIDS_SE_ALGOID_DST_ECDSA_P224 0x0B
#define VGIDS_SE_ALGOID_DST_ECDSA_P256 0x0C
#define VGIDS_SE_ALGOID_DST_ECDSA_P384 0x0D
#define VGIDS_SE_ALGOID_DST_ECDSA_P512 0x0E

#define VGIDS_DEFAULT_KEY_REF 0x81

#define ISO_INS_SELECT 0xA4
#define ISO_INS_GETDATA 0xCB
#define ISO_INS_GETRESPONSE 0xC0
#define ISO_INS_MSE 0x22
#define ISO_INS_PSO 0x2A
#define ISO_INS_VERIFY 0x20

#define ISO_STATUS_MORE_DATA 0x6100
#define ISO_STATUS_VERIFYFAILED 0x6300
#define ISO_STATUS_WRONGLC 0x6700
#define ISO_STATUS_COMMANDNOTALLOWED 0x6900
#define ISO_STATUS_SECURITYSTATUSNOTSATISFIED 0x6982
#define ISO_STATUS_AUTHMETHODBLOCKED 0x6983
#define ISO_STATUS_INVALIDCOMMANDDATA 0x6A80
#define ISO_STATUS_FILENOTFOUND 0x6A82
#define ISO_STATUS_INVALIDP1P2 0x6A86
#define ISO_STATUS_INVALIDLC 0x6A87
#define ISO_STATUS_REFERENCEDATANOTFOUND 0x6A88
#define ISO_STATUS_SUCCESS 0x9000

#define ISO_AID_MAX_SIZE 16

#define ISO_FID_MF 0x3F00

struct vgids_ef
{
	UINT16 id;
	UINT16 dirID;
	wStream* data;
};
typedef struct vgids_ef vgidsEF;

struct vgids_se
{
	BYTE crt;    /* control reference template tag */
	BYTE algoId; /* Algorithm ID */
	BYTE keyRef; /* Key reference */
};
typedef struct vgids_se vgidsSE;

struct vgids_context
{
	UINT16 currentDF;
	char* pin;
	UINT16 curRetryCounter;
	UINT16 retryCounter;
	wStream* commandData;
	wStream* responseData;
	BOOL pinVerified;
	vgidsSE currentSE;

	X509* certificate;

	RSA* publicKey;
	RSA* privateKey;

	wArrayList* files;
};

/* PKCS 1.5 DER encoded digest information */
#define VGIDS_MAX_DIGEST_INFO 7

static const BYTE g_PKCS1_SHA1[] = { 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
	                                 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14 };
static const BYTE g_PKCS1_SHA224[] = { 0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
	                                   0x65, 0x03, 0x04, 0x02, 0x04, 0x05, 0x00, 0x04, 0x1c };
static const BYTE g_PKCS1_SHA256[] = { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
	                                   0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20 };
static const BYTE g_PKCS1_SHA384[] = { 0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
	                                   0x65, 0x03, 0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30 };
static const BYTE g_PKCS1_SHA512[] = { 0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
	                                   0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40 };
static const BYTE g_PKCS1_SHA512_224[] = { 0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60,
	                                       0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02,
	                                       0x05, 0x05, 0x00, 0x04, 0x1c };
static const BYTE g_PKCS1_SHA512_256[] = { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60,
	                                       0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02,
	                                       0x06, 0x05, 0x00, 0x04, 0x20 };

/* Helper struct to map PKCS1.5 digest info to OpenSSL EVP_MD */
struct vgids_digest_info_map
{
	const BYTE* info;
	size_t infoSize;
	const EVP_MD* digest;
};
typedef struct vgids_digest_info_map vgidsDigestInfoMap;

/* MS GIDS AID */
/* xx: Used by the Windows smart card framework for the GIDS version number. This byte must be set
 * to the GIDS specification revision number which is either 0x01 or 0x02.
 * yy: Reserved for use by the card application (set to 01)
 */
static const BYTE g_MsGidsAID[] = {
	0xA0, 0x00, 0x00, 0x03, 0x97, 0x42, 0x54, 0x46, 0x59, 0x02, 0x01
};

/* GIDS APP File Control Parameter:
   FD-Byte (82): 38 (not shareable-DF)
   Sec Attr (8C): 03 30 30  Create/Delete File(03) Ext/User-Auth (30)
*/
static const BYTE g_GidsAppFCP[] = { 0x62, 0x08, 0x82, 0x01, 0x38, 0x8C, 0x03, 0x03, 0x30, 0x30 };
/* GIDS APP File Control Information:
   AppID (4F, Len 0B): A0 00 00 03 97 42 54 46 59 02 01
   Discretionary DOs (73, Len 03): 40 01 C0
      Supported Auth Protocols (40, Len 01): C0 Mutual/External-Auth
 */
static const BYTE g_GidsAppFCI[] = { 0x61, 0x12, 0x4F, 0x0B, 0xA0, 0x00, 0x00, 0x03, 0x97, 0x42,
	                                 0x54, 0x46, 0x59, 0x02, 0x01, 0x73, 0x03, 0x40, 0x01, 0xC0 };

/*
typedef struct
{
    BYTE bVersion; // Cache version
    BYTE bPinsFreshness; // Card PIN
    WORD wContainersFreshness;
    WORD wFilesFreshness;
} CARD_CACHE_FILE_FORMAT, *PCARD_CACHE_FILE_FORMAT; */
static const BYTE g_CardCFContents[] = { 0x00, 0x00, 0x01, 0x00, 0x04, 0x00 };

/* {mscp,0,0,0,0} */
static const BYTE g_CardAppsContents[] = { 0x6d, 0x73, 0x63, 0x70, 0x00, 0x00, 0x00, 0x00 };

#pragma pack(push, 1)

/* Type: CONTAINER_MAP_RECORD (taken from Windows Smart Card Minidriver Specification)

   This structure describes the format of the Base CSP's
   container map file, stored on the card. This is wellknown
   logical file wszCONTAINER_MAP_FILE. The file consists of
   zero or more of these records. */
#define MAX_CONTAINER_NAME_LEN 39

/* This flag is set in the CONTAINER_MAP_RECORD bFlags
   member if the corresponding container is valid and currently
   exists on the card. // If the container is deleted, its
   bFlags field must be cleared. */
#define CONTAINER_MAP_VALID_CONTAINER 1

/* This flag is set in the CONTAINER_MAP_RECORD bFlags
   member if the corresponding container is the default
   container on the card. */
#define CONTAINER_MAP_DEFAULT_CONTAINER 2

struct vgids_container_map_entry
{
	WCHAR wszGuid[MAX_CONTAINER_NAME_LEN + 1];
	BYTE bFlags;
	BYTE bReserved;
	WORD wSigKeySizeBits;
	WORD wKeyExchangeKeySizeBits;
};
typedef struct vgids_container_map_entry vgidsContainerMapEntry;

struct vgids_filesys_table_entry
{
	char directory[9];
	char filename[9];
	UINT16 pad0;
	UINT16 dataObjectIdentifier;
	UINT16 pad1;
	UINT16 fileIdentifier;
	UINT16 unknown;
};
typedef struct vgids_filesys_table_entry vgidsFilesysTableEntry;

struct vgids_keymap_record
{
	UINT32 state;
	BYTE algid;
	BYTE keytype;
	UINT16 keyref;
	UINT16 unknownWithFFFF;
	UINT16 unknownWith0000;
};
typedef struct vgids_keymap_record vgidsKeymapRecord;

#pragma pack(pop)

static vgidsEF* vgids_ef_new(vgidsContext* ctx, USHORT id)
{
	vgidsEF* ef = calloc(1, sizeof(vgidsEF));

	ef->id = id;
	ef->data = Stream_New(NULL, 1024);
	if (!ef->data)
	{
		WLog_ERR(TAG, "Failed to create file data stream");
		goto create_failed;
	}
	Stream_SetLength(ef->data, 0);

	if (!ArrayList_Append(ctx->files, ef))
	{
		WLog_ERR(TAG, "Failed to add new ef to file list");
		goto create_failed;
	}

	return ef;

create_failed:
	free(ef);
	return NULL;
}

static BOOL vgids_write_tlv(wStream* s, UINT16 tag, const BYTE* data, DWORD dataSize)
{
	/* A maximum of 5 additional bytes is needed */
	if (!Stream_EnsureRemainingCapacity(s, dataSize + 5))
	{
		WLog_ERR(TAG, "Failed to ensure capacity of DO stream");
		return FALSE;
	}

	/* BER encoding: If the most-significant bit is set (0x80) the length is encoded in the
	 * remaining bits. So lengths < 128 bytes can be set directly, all others are encoded */
	if (tag > 0xFF)
		Stream_Write_UINT16_BE(s, tag);
	else
		Stream_Write_UINT8(s, (BYTE)tag);
	if (dataSize < 128)
	{
		Stream_Write_UINT8(s, (BYTE)dataSize);
	}
	else if (dataSize < 256)
	{
		Stream_Write_UINT8(s, 0x81);
		Stream_Write_UINT8(s, (BYTE)dataSize);
	}
	else
	{
		Stream_Write_UINT8(s, 0x82);
		Stream_Write_UINT16_BE(s, (UINT16)dataSize);
	}
	Stream_Write(s, data, dataSize);
	Stream_SealLength(s);
	return TRUE;
}

static BOOL vgids_ef_write_do(vgidsEF* ef, UINT16 doID, const BYTE* data, DWORD dataSize)
{
	/* Write DO to end of file: 2-Byte ID, 1-Byte Len, Data */
	return vgids_write_tlv(ef->data, doID, data, dataSize);
}

static BOOL vgids_ef_read_do(vgidsEF* ef, UINT16 doID, BYTE** data, DWORD* dataSize)
{
	/* Read the given DO from the file: 2-Byte ID, 1-Byte Len, Data */
	if (!Stream_SetPosition(ef->data, 0))
	{
		WLog_ERR(TAG, "Failed to seek to front of file");
		return FALSE;
	}

	/* Look for the requested DO */
	while (Stream_GetRemainingLength(ef->data) > 3)
	{
		BYTE len;
		size_t curPos;
		UINT16 doSize;
		UINT16 nextDOID;

		curPos = Stream_GetPosition(ef->data);
		Stream_Read_UINT16_BE(ef->data, nextDOID);
		Stream_Read_UINT8(ef->data, len);
		if ((len & 0x80))
		{
			BYTE lenSize = len & 0x7F;
			if (!Stream_CheckAndLogRequiredLength(TAG, ef->data, lenSize))
				return FALSE;

			switch (lenSize)
			{
				case 1:
					Stream_Read_UINT8(ef->data, doSize);
					break;
				case 2:
					Stream_Read_UINT16_BE(ef->data, doSize);
					break;
				default:
					WLog_ERR(TAG, "Unexpected tag length %" PRIu8, lenSize);
					return FALSE;
			}
		}
		else
			doSize = len;

		if (!Stream_CheckAndLogRequiredLength(TAG, ef->data, doSize))
			return FALSE;

		if (nextDOID == doID)
		{
			BYTE* outData;

			/* Include Tag and length in result */
			doSize += (UINT16)(Stream_GetPosition(ef->data) - curPos);
			outData = malloc(doSize);
			if (!outData)
			{
				WLog_ERR(TAG, "Failed to allocate output buffer");
				return FALSE;
			}

			Stream_SetPosition(ef->data, curPos);
			Stream_Read(ef->data, outData, doSize);
			*data = outData;
			*dataSize = doSize;
			return TRUE;
		}
		else
		{
			/* Skip DO */
			Stream_SafeSeek(ef->data, doSize);
		}
	}

	return FALSE;
}

static void vgids_ef_free(void* ptr)
{
	vgidsEF* ef = ptr;
	if (ef)
	{
		Stream_Free(ef->data, TRUE);
		free(ef);
	}
}

static BOOL vgids_prepare_fstable(const vgidsFilesysTableEntry* fstable, DWORD numEntries,
                                  BYTE** outData, DWORD* outDataSize)
{
	/* Filesystem table:
	    BYTE unkonwn: 0x01
	    Array of vgidsFilesysTableEntry
	*/
	DWORD i;
	BYTE* data = malloc(sizeof(vgidsFilesysTableEntry) * numEntries + 1);
	if (!data)
	{
		WLog_ERR(TAG, "Failed to allocate filesystem table data blob");
		return FALSE;
	}

	*data = 0x01;
	for (i = 0; i < numEntries; ++i)
		memcpy(data + 1 + (sizeof(vgidsFilesysTableEntry) * i), &fstable[i],
		       sizeof(vgidsFilesysTableEntry));

	*outData = data;
	*outDataSize = sizeof(vgidsFilesysTableEntry) * numEntries + 1;

	return TRUE;
}

static BOOL vgids_prepare_certificate(X509* cert, BYTE** kxc, DWORD* kxcSize)
{
	/* Key exchange container:
	    UINT16 compression version: 0001
	    UINT16 source size
	    ZLIB compressed cert
	*/
	BYTE* i2dParam;
	uLongf destSize;
	wStream* s = NULL;
	BYTE* certData = NULL;
	BYTE* comprData = NULL;
	int certSize = i2d_X509(cert, NULL);
	if (certSize < 0)
	{
		WLog_ERR(TAG, "Failed to get certificate size");
		goto handle_error;
	}

	certData = malloc(certSize);
	comprData = malloc(certSize);
	if (!certData || !comprData)
	{
		WLog_ERR(TAG, "Failed to allocate certificate buffer");
		goto handle_error;
	}

	/* serialize certificate to buffer (out buffer pointer is modified so pass a copy!) */
	i2dParam = certData;
	if (i2d_X509(cert, &i2dParam) < 0)
	{
		WLog_ERR(TAG, "Failed to encode X509 certificate to DER");
		goto handle_error;
	}

	/* compress certificate data */
	destSize = certSize;
	if (compress(comprData, &destSize, certData, certSize) != Z_OK)
	{
		WLog_ERR(TAG, "Failed to compress certificate data");
		goto handle_error;
	}

	/* Write container data */
	s = Stream_New(NULL, destSize + 4);
	Stream_Write_UINT16(s, 0x0001);
	Stream_Write_UINT16(s, (UINT16)certSize);
	Stream_Write(s, comprData, destSize);
	Stream_SealLength(s);

	*kxc = Stream_Buffer(s);
	*kxcSize = (DWORD)Stream_Length(s);

	Stream_Free(s, FALSE);
	free(certData);
	free(comprData);
	return TRUE;

handle_error:
	Stream_Free(s, TRUE);
	free(certData);
	free(comprData);
	return FALSE;
}

static BYTE vgids_get_algid(vgidsContext* p_Ctx)
{
	int modulusSize = RSA_size(p_Ctx->privateKey);
	switch (modulusSize)
	{
		case (1024 / 8):
			return VGIDS_ALGID_RSA_1024;
		case (2048 / 8):
			return VGIDS_ALGID_RSA_2048;
		case (3072 / 8):
			return VGIDS_ALGID_RSA_3072;
		case (4096 / 8):
			return VGIDS_ALGID_RSA_4096;
		default:
			WLog_ERR(TAG, "Failed to determine algid for private key");
			break;
	}

	return 0;
}

static BOOL vgids_prepare_keymap(vgidsContext* context, BYTE** outData, DWORD* outDataSize)
{
	/* Key map record table:
	    BYTE unkonwn (count?): 0x01
	    Array of vgidsKeymapRecord
	*/
	BYTE* data;
	vgidsKeymapRecord record = {
		1,                                /* state */
		0,                                /* algo */
		VGIDS_KEY_TYPE_KEYEXCHANGE,       /* keytpe */
		(0xB000 | VGIDS_DEFAULT_KEY_REF), /* keyref */
		0xFFFF,                           /* unknown FFFF */
		0x0000                            /* unknown 0000 */
	};

	/* Determine algo */
	BYTE algid = vgids_get_algid(context);
	if (algid == 0)
		return FALSE;

	data = malloc(sizeof(record) + 1);
	if (!data)
	{
		WLog_ERR(TAG, "Failed to allocate filesystem table data blob");
		return FALSE;
	}

	*data = 0x01;
	record.algid = algid;
	memcpy(data + 1, &record, sizeof(record));

	*outData = data;
	*outDataSize = sizeof(record) + 1;

	return TRUE;
}

static BOOL vgids_parse_apdu_header(wStream* s, BYTE* cla, BYTE* ins, BYTE* p1, BYTE* p2, BYTE* lc,
                                    BYTE* le)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	/* Read and verify APDU data */
	if (cla)
		Stream_Read_UINT8(s, *cla);
	else
		Stream_Seek(s, 1);
	if (ins)
		Stream_Read_UINT8(s, *ins);
	else
		Stream_Seek(s, 1);
	if (p1)
		Stream_Read_UINT8(s, *p1);
	else
		Stream_Seek(s, 1);
	if (p2)
		Stream_Read_UINT8(s, *p2);
	else
		Stream_Seek(s, 1);

	/* If LC is requested - check remaining length and read as well */
	if (lc)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			return FALSE;

		Stream_Read_UINT8(s, *lc);
		if (!Stream_CheckAndLogRequiredLength(TAG, s, *lc))
			return FALSE;
	}

	/* read LE */
	if (le)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			return FALSE;
		Stream_Read_UINT8(s, *le);
	}

	return TRUE;
}

static BOOL vgids_create_response(UINT16 status, const BYTE* answer, DWORD answerSize,
                                  BYTE** outData, DWORD* outDataSize)
{
	BYTE* out = malloc(answerSize + 2);
	if (!out)
	{
		WLog_ERR(TAG, "Failed to allocate memory for response data");
		return FALSE;
	}

	*outData = out;
	if (answer)
	{
		memcpy(out, answer, answerSize);
		out += answerSize;
	}

	*out = (BYTE)((status >> 8) & 0xFF);
	*(out + 1) = (BYTE)(status & 0xFF);
	*outDataSize = answerSize + 2;
	return TRUE;
}

static BOOL vgids_read_do_fkt(void* data, size_t index, va_list ap)
{
	BYTE* response;
	DWORD responseSize;
	vgidsEF* file = (vgidsEF*)data;
	vgidsContext* context = va_arg(ap, vgidsContext*);
	UINT16 efID = (UINT16)va_arg(ap, unsigned);
	UINT16 doID = (UINT16)va_arg(ap, unsigned);
	WINPR_UNUSED(index);

	if (efID == 0x3FFF || efID == file->id)
	{
		/* If the DO was successfully read - abort file enum */
		if (vgids_ef_read_do(file, doID, &response, &responseSize))
		{
			context->responseData = Stream_New(response, (size_t)responseSize);
			return FALSE;
		}
	}

	return TRUE;
}

static void vgids_read_do(vgidsContext* context, UINT16 efID, UINT16 doID)
{
	ArrayList_ForEach(context->files, vgids_read_do_fkt, context, efID, doID);
}

static void vgids_reset_context_response(vgidsContext* context)
{
	Stream_Free(context->responseData, TRUE);
	context->responseData = NULL;
}

static void vgids_reset_context_command_data(vgidsContext* context)
{
	Stream_Free(context->commandData, TRUE);
	context->commandData = NULL;
}

static BOOL vgids_ins_select(vgidsContext* context, wStream* s, BYTE** response,
                             DWORD* responseSize)
{
	BYTE p1, p2, lc;
	DWORD resultDataSize = 0;
	const BYTE* resultData = NULL;
	UINT16 status = ISO_STATUS_SUCCESS;

	/* The only select operations performed are either select by AID or select 3FFF (return
	 * information about the currently selected DF) */
	if (!vgids_parse_apdu_header(s, NULL, NULL, &p1, &p2, &lc, NULL))
		return FALSE;

	/* Check P1 for selection mode */
	switch (p1)
	{
		/* Select by AID */
		case 0x04:
		{
			/* read AID from APDU */
			BYTE aid[ISO_AID_MAX_SIZE] = { 0 };
			if (lc > ISO_AID_MAX_SIZE)
			{
				WLog_ERR(TAG, "The LC byte is greater than the maximum AID length");
				status = ISO_STATUS_INVALIDLC;
				break;
			}

			/* Check if we select MS GIDS App (only one we know) */
			Stream_Read(s, aid, lc);
			if (memcmp(aid, g_MsGidsAID, lc) != 0)
			{
				status = ISO_STATUS_FILENOTFOUND;
				break;
			}

			/* Return FCI or FCP for MsGids App */
			switch (p2)
			{
				/* Return FCI information */
				case 0x00:
				{
					resultData = g_GidsAppFCI;
					resultDataSize = sizeof(g_GidsAppFCI);
					break;
				}
				/* Return FCP information */
				case 0x04:
				{
					resultData = g_GidsAppFCP;
					resultDataSize = sizeof(g_GidsAppFCP);
					break;
				}
				default:
					status = ISO_STATUS_INVALIDP1P2;
					break;
			}

			if (resultData)
				context->currentDF = ISO_FID_MF;
			break;
		}
		/* Select by FID */
		case 0x00:
		{
			/* read FID from APDU */
			UINT16 fid;
			if (lc > 2)
			{
				WLog_ERR(TAG, "The LC byte for the file ID is greater than 2");
				status = ISO_STATUS_INVALIDLC;
				break;
			}

			Stream_Read_UINT16_BE(s, fid);
			if (fid != VGIDS_EFID_CURRENTDF || context->currentDF == 0)
			{
				status = ISO_STATUS_FILENOTFOUND;
				break;
			}
			break;
		}
		default:
		{
			/* P1 P2 combination not supported */
			status = ISO_STATUS_INVALIDP1P2;
			break;
		}
	}

	return vgids_create_response(status, resultData, resultDataSize, response, responseSize);
}

static UINT16 vgids_handle_chained_response(vgidsContext* context, const BYTE** response,
                                            DWORD* responseSize)
{
	/* Cap to a maximum of 256 bytes and set status to more data */
	UINT16 status = ISO_STATUS_SUCCESS;
	DWORD remainingBytes = (DWORD)Stream_Length(context->responseData);
	if (remainingBytes > 256)
	{
		status = ISO_STATUS_MORE_DATA;
		remainingBytes = 256;
	}

	*response = Stream_Buffer(context->responseData);
	*responseSize = remainingBytes;
	Stream_Seek(context->responseData, remainingBytes);

	/* Check if there are more than 256 bytes left or if we can already provide the remaining length
	 * in the status word */
	remainingBytes = (DWORD)(Stream_Length(context->responseData) - remainingBytes);
	if (remainingBytes < 256 && remainingBytes != 0)
		status |= (remainingBytes & 0xFF);
	return status;
}

static BOOL vgids_get_public_key(vgidsContext* context, UINT16 doTag)
{
	BYTE* buf = NULL;
	wStream* pubKey = NULL;
	wStream* response = NULL;
	const BIGNUM *n, *e;
	int nSize, eSize;

	/* Get key components */
	RSA_get0_key(context->publicKey, &n, &e, NULL);
	nSize = BN_num_bytes(n);
	eSize = BN_num_bytes(e);

	buf = malloc(nSize > eSize ? nSize : eSize);
	if (!buf)
	{
		WLog_ERR(TAG, "Failed to allocate buffer for public key");
		goto handle_error;
	}

	pubKey = Stream_New(NULL, nSize + eSize + 0x10);
	if (!pubKey)
	{
		WLog_ERR(TAG, "Failed to allocate public key stream");
		goto handle_error;
	}

	response = Stream_New(NULL, Stream_Capacity(pubKey) + 0x10);
	if (!response)
	{
		WLog_ERR(TAG, "Failed to allocate response stream");
		goto handle_error;
	}

	/* write modulus and exponent DOs */
	BN_bn2bin(n, buf);
	if (!vgids_write_tlv(pubKey, 0x81, buf, nSize))
		goto handle_error;

	BN_bn2bin(e, buf);
	if (!vgids_write_tlv(pubKey, 0x82, buf, eSize))
		goto handle_error;

	/* write ISO public key template */
	if (!vgids_write_tlv(response, doTag, Stream_Buffer(pubKey), (DWORD)Stream_Length(pubKey)))
		goto handle_error;

	/* set response data */
	Stream_SetPosition(response, 0);
	context->responseData = response;

	free(buf);
	Stream_Free(pubKey, TRUE);
	return TRUE;

handle_error:
	free(buf);
	Stream_Free(pubKey, TRUE);
	Stream_Free(response, TRUE);
	return FALSE;
}

static BOOL vgids_ins_getdata(vgidsContext* context, wStream* s, BYTE** response,
                              DWORD* responseSize)
{
	UINT16 doId;
	UINT16 fileId;
	BYTE p1, p2, lc;
	DWORD resultDataSize = 0;
	const BYTE* resultData = NULL;
	UINT16 status = ISO_STATUS_SUCCESS;

	/* GetData is called a lot!
	     - To retrieve DOs from files
	     - To retrieve public key information
	*/
	if (!vgids_parse_apdu_header(s, NULL, NULL, &p1, &p2, &lc, NULL))
		return FALSE;

	/* free any previous queried data */
	vgids_reset_context_response(context);

	/* build up file identifier */
	fileId = ((UINT16)p1 << 8) | p2;

	/* Do we have a DO reference? */
	switch (lc)
	{
		case 4:
		{
			BYTE tag, length;
			Stream_Read_UINT8(s, tag);
			Stream_Read_UINT8(s, length);
			if (tag != 0x5C && length != 0x02)
			{
				status = ISO_STATUS_INVALIDCOMMANDDATA;
				break;
			}

			Stream_Read_UINT16_BE(s, doId);
			vgids_read_do(context, fileId, doId);
			break;
		}
		case 0xA:
		{
			UINT16 pubKeyDO;
			BYTE tag, length, keyRef;

			/* We want to retrieve the public key? */
			if (p1 != 0x3F && p2 != 0xFF)
			{
				status = ISO_STATUS_INVALIDP1P2;
				break;
			}

			/* read parent tag/length */
			Stream_Read_UINT8(s, tag);
			Stream_Read_UINT8(s, length);
			if (tag != 0x70 || length != 0x08)
			{
				status = ISO_STATUS_INVALIDCOMMANDDATA;
				break;
			}

			/* read key reference TLV */
			Stream_Read_UINT8(s, tag);
			Stream_Read_UINT8(s, length);
			Stream_Read_UINT8(s, keyRef);
			if (tag != 0x84 || length != 0x01 || keyRef != VGIDS_DEFAULT_KEY_REF)
			{
				status = ISO_STATUS_INVALIDCOMMANDDATA;
				break;
			}

			/* read key value template TLV */
			Stream_Read_UINT8(s, tag);
			Stream_Read_UINT8(s, length);
			if (tag != 0xA5 || length != 0x03)
			{
				status = ISO_STATUS_INVALIDCOMMANDDATA;
				break;
			}

			Stream_Read_UINT16_BE(s, pubKeyDO);
			Stream_Read_UINT8(s, length);
			if (pubKeyDO != 0x7F49 || length != 0x80)
			{
				status = ISO_STATUS_INVALIDCOMMANDDATA;
				break;
			}

			if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			{
				status = ISO_STATUS_INVALIDLC;
				break;
			}

			/* Return public key value */
			vgids_get_public_key(context, pubKeyDO);
			break;
		}
		default:
			status = ISO_STATUS_INVALIDCOMMANDDATA;
			break;
	}

	/* If we have response data, make it ready for return */
	if (context->responseData)
		status = vgids_handle_chained_response(context, &resultData, &resultDataSize);
	else if (status == ISO_STATUS_SUCCESS)
		status = ISO_STATUS_REFERENCEDATANOTFOUND;

	return vgids_create_response(status, resultData, resultDataSize, response, responseSize);
}

static BOOL vgids_ins_manage_security_environment(vgidsContext* context, wStream* s,
                                                  BYTE** response, DWORD* responseSize)
{
	BYTE tag, length;
	BYTE p1, p2, lc;
	DWORD resultDataSize = 0;
	const BYTE* resultData = NULL;
	UINT16 status = ISO_STATUS_SUCCESS;

	vgids_reset_context_command_data(context);
	vgids_reset_context_response(context);

	/* Manage security environment prepares the card for performing crypto operations. */
	if (!vgids_parse_apdu_header(s, NULL, NULL, &p1, &p2, &lc, NULL))
		return FALSE;

	/* Check APDU params */
	/* P1: Set Computation, decipherment, Internal Auth */
	/* P2: Digital Signature (B6), Confidentiality (B8) */
	if (p1 != 0x41 && p2 != 0xB6 && p2 != 0xB8)
	{
		status = ISO_STATUS_INVALIDP1P2;
		goto create_response;
	}

	if (lc != 6)
	{
		status = ISO_STATUS_WRONGLC;
		goto create_response;
	}

	context->currentSE.crt = p2;

	/* parse command buffer */
	/* Read algo ID */
	Stream_Read_UINT8(s, tag);
	Stream_Read_UINT8(s, length);
	if (tag != 0x80 || length != 0x01)
	{
		status = ISO_STATUS_INVALIDCOMMANDDATA;
		goto create_response;
	}
	Stream_Read_UINT8(s, context->currentSE.algoId);

	/* Read private key reference */
	Stream_Read_UINT8(s, tag);
	Stream_Read_UINT8(s, length);
	if (tag != 0x84 || length != 0x01)
	{
		status = ISO_STATUS_INVALIDCOMMANDDATA;
		goto create_response;
	}
	Stream_Read_UINT8(s, context->currentSE.keyRef);

create_response:
	/* If an error occured reset SE */
	if (status != ISO_STATUS_SUCCESS)
		memset(&context->currentSE, 0, sizeof(context->currentSE));
	return vgids_create_response(status, resultData, resultDataSize, response, responseSize);
}

static BOOL vgids_perform_digital_signature(vgidsContext* context)
{
	size_t sigSize, msgSize;
	EVP_PKEY_CTX* ctx = NULL;
	EVP_PKEY* pk = EVP_PKEY_new();
	vgidsDigestInfoMap gidsDigestInfo[VGIDS_MAX_DIGEST_INFO] = {
		{ g_PKCS1_SHA1, sizeof(g_PKCS1_SHA1), EVP_sha1() },
		{ g_PKCS1_SHA224, sizeof(g_PKCS1_SHA224), EVP_sha224() },
		{ g_PKCS1_SHA256, sizeof(g_PKCS1_SHA256), EVP_sha256() },
		{ g_PKCS1_SHA384, sizeof(g_PKCS1_SHA384), EVP_sha384() },
		{ g_PKCS1_SHA512, sizeof(g_PKCS1_SHA512), EVP_sha512() },
		{ g_PKCS1_SHA512_224, sizeof(g_PKCS1_SHA512_224), EVP_sha512_224() },
		{ g_PKCS1_SHA512_256, sizeof(g_PKCS1_SHA512_256), EVP_sha512_256() }
	};

	if (!pk)
	{
		WLog_ERR(TAG, "Failed to create PKEY");
		return FALSE;
	}

	EVP_PKEY_set1_RSA(pk, context->privateKey);
	vgids_reset_context_response(context);

	/* for each digest info */
	Stream_SetPosition(context->commandData, 0);
	for (int i = 0; i < VGIDS_MAX_DIGEST_INFO; ++i)
	{
		/* have we found our digest? */
		const vgidsDigestInfoMap* digest = &gidsDigestInfo[i];
		if (Stream_Length(context->commandData) >= digest->infoSize &&
		    memcmp(Stream_Buffer(context->commandData), digest->info, digest->infoSize) == 0)
		{
			/* skip digest info and calculate message size */
			Stream_Seek(context->commandData, digest->infoSize);
			if (!Stream_CheckAndLogRequiredLength(TAG, context->commandData, 2))
				goto sign_failed;
			msgSize = Stream_GetRemainingLength(context->commandData);

			/* setup signing context */
			ctx = EVP_PKEY_CTX_new(pk, NULL);
			if (!ctx)
			{
				WLog_ERR(TAG, "Failed to create signing context");
				goto sign_failed;
			}

			if (EVP_PKEY_sign_init(ctx) <= 0)
			{
				WLog_ERR(TAG, "Failed to init signing context");
				goto sign_failed;
			}

			/* set padding and signature algo */
			if (context->currentSE.algoId & VGIDS_SE_ALGOID_DST_PAD_PKCS1)
			{
				if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0)
				{
					WLog_ERR(TAG, "Failed to set padding mode");
					goto sign_failed;
				}
			}

			if (EVP_PKEY_CTX_set_signature_md(ctx, digest->digest) <= 0)
			{
				WLog_ERR(TAG, "Failed to set signing mode");
				goto sign_failed;
			}

			/* Determine buffer length */
			if (EVP_PKEY_sign(ctx, NULL, &sigSize, Stream_Pointer(context->commandData), msgSize) <=
			    0)
			{
				WLog_ERR(TAG, "Failed to determine signature size");
				goto sign_failed;
			}

			context->responseData = Stream_New(NULL, sigSize);
			if (!context->responseData)
			{
				WLog_ERR(TAG, "Failed to allocate signing buffer");
				goto sign_failed;
			}

			/* sign */
			if (EVP_PKEY_sign(ctx, Stream_Buffer(context->responseData), &sigSize,
			                  Stream_Pointer(context->commandData), msgSize) <= 0)
			{
				WLog_ERR(TAG, "Failed to create signature");
				goto sign_failed;
			}

			Stream_SetLength(context->responseData, sigSize);
			EVP_PKEY_CTX_free(ctx);
			break;
		}
	}

	EVP_PKEY_free(pk);
	vgids_reset_context_command_data(context);
	return TRUE;

sign_failed:
	vgids_reset_context_command_data(context);
	vgids_reset_context_response(context);
	EVP_PKEY_CTX_free(ctx);
	EVP_PKEY_free(pk);
	return FALSE;
}

static BOOL vgids_perform_decrypt(vgidsContext* context)
{
	int res;
	int padding = RSA_NO_PADDING;

	vgids_reset_context_response(context);

	/* determine padding */
	if (context->currentSE.algoId & VGIDS_SE_ALGOID_CT_PAD_PKCS1)
		padding = RSA_PKCS1_PADDING;
	else if (context->currentSE.algoId & VGIDS_SE_ALGOID_CT_PAD_OAEP)
		padding = RSA_PKCS1_OAEP_PADDING;

	/* init response buffer */
	context->responseData = Stream_New(NULL, RSA_size(context->privateKey));
	if (!context->responseData)
	{
		WLog_ERR(TAG, "Failed to create decryption buffer");
		goto decrypt_failed;
	}

	/* Determine buffer length */
	res = RSA_private_decrypt((int)Stream_Length(context->commandData),
	                          Stream_Buffer(context->commandData),
	                          Stream_Buffer(context->responseData), context->privateKey, padding);
	if (res < 0)
	{
		WLog_ERR(TAG, "Failed to decrypt data");
		goto decrypt_failed;
	}

	Stream_SetLength(context->responseData, res);
	vgids_reset_context_command_data(context);
	return TRUE;

decrypt_failed:
	vgids_reset_context_command_data(context);
	vgids_reset_context_response(context);
	return FALSE;
}

static BOOL vgids_ins_perform_security_operation(vgidsContext* context, wStream* s, BYTE** response,
                                                 DWORD* responseSize)
{
	BYTE cla, p1, p2, lc;
	DWORD resultDataSize = 0;
	const BYTE* resultData = NULL;
	UINT16 status = ISO_STATUS_SUCCESS;

	/* Perform security operation */
	if (!vgids_parse_apdu_header(s, &cla, NULL, &p1, &p2, &lc, NULL))
		return FALSE;

	if (lc == 0)
	{
		status = ISO_STATUS_WRONGLC;
		goto create_response;
	}

	/* Is our default key referenced? */
	if (context->currentSE.keyRef != VGIDS_DEFAULT_KEY_REF)
	{
		status = ISO_STATUS_SECURITYSTATUSNOTSATISFIED;
		goto create_response;
	}

	/* is the pin protecting the key verified? */
	if (!context->pinVerified)
	{
		status = ISO_STATUS_SECURITYSTATUSNOTSATISFIED;
		goto create_response;
	}

	/* Append the data to the context command buffer (PSO might chain command data) */
	if (!context->commandData)
	{
		context->commandData = Stream_New(NULL, lc);
		if (!context->commandData)
			return FALSE;
	}
	else
		Stream_EnsureRemainingCapacity(context->commandData, lc);

	Stream_Write(context->commandData, Stream_Pointer(s), lc);
	Stream_SealLength(context->commandData);

	/* Check if the correct operation is requested for our current SE */
	switch (context->currentSE.crt)
	{
		case VGIDS_SE_CRT_SIGN:
		{
			if (p1 != 0x9E || p2 != 0x9A)
			{
				status = ISO_STATUS_INVALIDP1P2;
				break;
			}

			/* If chaining is over perform op */
			if (!(cla & 0x10))
				vgids_perform_digital_signature(context);
			break;
		}
		case VGIDS_SE_CRT_CONF:
		{
			if ((p1 != 0x86 || p2 != 0x80) && (p1 != 0x80 || p2 != 0x86))
			{
				status = ISO_STATUS_INVALIDP1P2;
				break;
			}

			/* If chaining is over perform op */
			if (!(cla & 0x10))
				vgids_perform_decrypt(context);
			break;
		}
		default:
			status = ISO_STATUS_INVALIDP1P2;
			break;
	}

	/* Do chaining of response data if necessary */
	if (status == ISO_STATUS_SUCCESS && context->responseData)
		status = vgids_handle_chained_response(context, &resultData, &resultDataSize);

	/* Check APDU params */
create_response:
	return vgids_create_response(status, resultData, resultDataSize, response, responseSize);
}

static BOOL vgids_ins_getresponse(vgidsContext* context, wStream* s, BYTE** response,
                                  DWORD* responseSize)
{
	BYTE p1, p2, le;
	DWORD resultDataSize = 0;
	const BYTE* resultData = NULL;
	DWORD expectedLen, remainingSize;
	UINT16 status = ISO_STATUS_SUCCESS;

	/* Get response continues data transfer after a previous get data command */
	/* Check if there is any data to transfer left */
	if (!context->responseData || !Stream_CheckAndLogRequiredLength(TAG, context->responseData, 1))
	{
		status = ISO_STATUS_COMMANDNOTALLOWED;
		goto create_response;
	}

	if (!vgids_parse_apdu_header(s, NULL, NULL, &p1, &p2, NULL, &le))
		return FALSE;

	/* Check APDU params */
	if (p1 != 00 || p2 != 0x00)
	{
		status = ISO_STATUS_INVALIDP1P2;
		goto create_response;
	}

	/* LE = 0 means 256 bytes expected */
	expectedLen = le;
	if (expectedLen == 0)
		expectedLen = 256;

	/* prepare response size and update offset */
	remainingSize = (DWORD)Stream_GetRemainingLength(context->responseData);
	if (remainingSize < expectedLen)
		expectedLen = remainingSize;

	resultData = Stream_Pointer(context->responseData);
	resultDataSize = expectedLen;
	Stream_Seek(context->responseData, expectedLen);

	/* If more data is left return 61XX - otherwise 9000 */
	remainingSize = (DWORD)Stream_GetRemainingLength(context->responseData);
	if (remainingSize > 0)
	{
		status = ISO_STATUS_MORE_DATA;
		if (remainingSize < 256)
			status |= (remainingSize & 0xFF);
	}

create_response:
	return vgids_create_response(status, resultData, resultDataSize, response, responseSize);
}

static BOOL vgids_ins_verify(vgidsContext* context, wStream* s, BYTE** response,
                             DWORD* responseSize)
{
	BYTE ins, p1, p2, lc;
	UINT16 status = ISO_STATUS_SUCCESS;
	char pin[VGIDS_MAX_PIN_SIZE + 1] = { 0 };

	/* Verify is always called for the application password (PIN) P2=0x80 */
	if (!vgids_parse_apdu_header(s, NULL, &ins, &p1, &p2, NULL, NULL))
		return FALSE;

	/* Check APDU params */
	if (p1 != 00 && p2 != 0x80 && p2 != 0x82)
	{
		status = ISO_STATUS_INVALIDP1P2;
		goto create_response;
	}

	/* shall we reset the security state? */
	if (p2 == 0x82)
	{
		context->pinVerified = FALSE;
		goto create_response;
	}

	/* Check if pin is not already blocked */
	if (context->curRetryCounter == 0)
	{
		status = ISO_STATUS_AUTHMETHODBLOCKED;
		goto create_response;
	}

	/* Read and verify LC */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
	{
		status = ISO_STATUS_INVALIDLC;
		goto create_response;
	}

	Stream_Read_UINT8(s, lc);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, lc) || (lc > VGIDS_MAX_PIN_SIZE))
	{
		status = ISO_STATUS_INVALIDLC;
		goto create_response;
	}

	/* read and verify pin */
	Stream_Read(s, pin, lc);
	if (strcmp(context->pin, pin) != 0)
	{
		/* retries are encoded in the lowest 4-bit of the status code */
		--context->curRetryCounter;
		context->pinVerified = FALSE;
		status = (ISO_STATUS_VERIFYFAILED | (context->curRetryCounter & 0xFF));
	}
	else
	{
		/* reset retry counter and mark pin as verified */
		context->curRetryCounter = context->retryCounter;
		context->pinVerified = TRUE;
	}

create_response:
	return vgids_create_response(status, NULL, 0, response, responseSize);
}

vgidsContext* vgids_new()
{
	wObject* obj;
	vgidsContext* ctx = calloc(1, sizeof(vgidsContext));

	ctx->files = ArrayList_New(FALSE);
	if (!ctx->files)
	{
		WLog_ERR(TAG, "Failed to create files array list");
		goto create_failed;
	}

	obj = ArrayList_Object(ctx->files);
	obj->fnObjectFree = vgids_ef_free;

	return ctx;

create_failed:
	vgids_free(ctx);
	return NULL;
}

BOOL vgids_init(vgidsContext* ctx, const char* cert, const char* privateKey, const char* pin)
{
	DWORD kxcSize;
	DWORD keymapSize;
	DWORD fsTableSize;
	BOOL rc = FALSE;
	BIO* certBio = NULL;
	BIO* privateKeyBio = NULL;
	BYTE* kxc = NULL;
	BYTE* keymap = NULL;
	BYTE* fsTable = NULL;
	EVP_PKEY* pubKey = NULL;
	vgidsEF* masterEF = NULL;
	vgidsEF* cardidEF = NULL;
	vgidsEF* commonEF = NULL;
	BYTE cardid[VGIDS_CARDID_SIZE] = { 0 };
	vgidsContainerMapEntry cmrec = { { 'P', 'r', 'i', 'v', 'a', 't', 'e', ' ', 'K', 'e', 'y', ' ',
		                               '0', '0' },
		                             CONTAINER_MAP_VALID_CONTAINER |
		                                 CONTAINER_MAP_DEFAULT_CONTAINER,
		                             0,
		                             0,
		                             0x00 /* key-size in bits - filled out later */ };
	vgidsFilesysTableEntry filesys[] = {
		{ "mscp", "", 0, 0, 0, 0xA000, 0 },
		{ "", "cardid", 0, 0xDF20, 0, 0xA012, 0 },
		{ "", "cardapps", 0, 0xDF21, 0, 0xA010, 0 },
		{ "", "cardcf", 0, 0xDF22, 0, 0xA010, 0 },
		{ "mscp", "cmapfile", 0, 0xDF23, 0, 0xA010, 0 },
		{ "mscp", "kxc00", 0, 0xDF24, 0, 0xA010, 0 },
	};

	/* Check params */
	if (!cert || !privateKey || !pin)
	{
		WLog_ERR(TAG, "Passed invalid NULL pointer argument");
		goto init_failed;
	}

	/* Convert PEM input to DER certificate/public key/private key */
	certBio = BIO_new_mem_buf((const void*)cert, (int)strlen(cert));
	if (!certBio)
		goto init_failed;
	ctx->certificate = PEM_read_bio_X509(certBio, NULL, NULL, NULL);
	if (!ctx->certificate)
		goto init_failed;
	pubKey = X509_get_pubkey(ctx->certificate);
	if (!pubKey)
		goto init_failed;
	ctx->publicKey = EVP_PKEY_get1_RSA(pubKey);
	if (!ctx->publicKey)
		goto init_failed;

	privateKeyBio = BIO_new_mem_buf((const void*)privateKey, (int)strlen(privateKey));
	if (!privateKeyBio)
		goto init_failed;
	ctx->privateKey = PEM_read_bio_RSAPrivateKey(privateKeyBio, NULL, NULL, NULL);
	if (!ctx->privateKey)
		goto init_failed;

	/* create masterfile */
	masterEF = vgids_ef_new(ctx, VGIDS_EFID_MASTER);
	if (!masterEF)
		goto init_failed;

	/* create cardid file with cardid DO */
	cardidEF = vgids_ef_new(ctx, VGIDS_EFID_CARDID);
	if (!cardidEF)
		goto init_failed;
	RAND_bytes(cardid, sizeof(cardid));
	if (!vgids_ef_write_do(cardidEF, VGIDS_DO_CARDID, cardid, sizeof(cardid)))
		goto init_failed;

	/* create user common file */
	commonEF = vgids_ef_new(ctx, VGIDS_EFID_COMMON);
	if (!commonEF)
		goto init_failed;

	/* write card cache DO */
	if (!vgids_ef_write_do(commonEF, VGIDS_DO_CARDCF, g_CardCFContents, sizeof(g_CardCFContents)))
		goto init_failed;

	/* write container map DO */
	cmrec.wKeyExchangeKeySizeBits = (WORD)(RSA_size(ctx->privateKey) * 8);
	if (!vgids_ef_write_do(commonEF, VGIDS_DO_CMAPFILE, (BYTE*)&cmrec, sizeof(cmrec)))
		goto init_failed;

	/* write cardapps DO */
	if (!vgids_ef_write_do(commonEF, VGIDS_DO_CARDAPPS, g_CardAppsContents,
	                       sizeof(g_CardAppsContents)))
		goto init_failed;

	/* convert and write certificate to key exchange container */
	if (!vgids_prepare_certificate(ctx->certificate, &kxc, &kxcSize))
		goto init_failed;
	if (!vgids_ef_write_do(commonEF, VGIDS_DO_KXC00, kxc, kxcSize))
		goto init_failed;

	/* prepare and write file system table */
	if (!vgids_prepare_fstable(filesys, ARRAYSIZE(filesys), &fsTable, &fsTableSize))
		goto init_failed;
	if (!vgids_ef_write_do(masterEF, VGIDS_DO_FILESYSTEMTABLE, fsTable, fsTableSize))
		goto init_failed;

	/* vgids_prepare_keymap and write to masterEF */
	if (!vgids_prepare_keymap(ctx, &keymap, &keymapSize))
		goto init_failed;
	if (!vgids_ef_write_do(masterEF, VGIDS_DO_KEYMAP, keymap, keymapSize))
		goto init_failed;

	/* store user pin */
	ctx->curRetryCounter = ctx->retryCounter = VGIDS_DEFAULT_RETRY_COUNTER;
	ctx->pin = _strdup(pin);
	if (!ctx->pin)
		goto init_failed;

	rc = TRUE;

init_failed:
	EVP_PKEY_free(pubKey);
	BIO_free_all(certBio);
	BIO_free_all(privateKeyBio);
	free(kxc);
	free(keymap);
	free(fsTable);
	return rc;
}

BOOL vgids_process_apdu(vgidsContext* context, const BYTE* data, DWORD dataSize, BYTE** response,
                        DWORD* responseSize)
{
	wStream s;
	static int x = 1;

	/* Check params */
	if (!context || !data || !response || !responseSize)
	{
		WLog_ERR(TAG, "Invalid NULL pointer passed");
		return FALSE;
	}

	if (dataSize < 4)
	{
		WLog_ERR(TAG, "APDU buffer is less than 4 bytes: %" PRIu32, dataSize);
		return FALSE;
	}

	/* Examine INS byte */
	Stream_StaticConstInit(&s, data, dataSize);
	if (x++ == 0xe)
		x = 0xe + 1;
	switch (data[1])
	{
		case ISO_INS_SELECT:
			return vgids_ins_select(context, &s, response, responseSize);
		case ISO_INS_GETDATA:
			return vgids_ins_getdata(context, &s, response, responseSize);
		case ISO_INS_GETRESPONSE:
			return vgids_ins_getresponse(context, &s, response, responseSize);
		case ISO_INS_MSE:
			return vgids_ins_manage_security_environment(context, &s, response, responseSize);
		case ISO_INS_PSO:
			return vgids_ins_perform_security_operation(context, &s, response, responseSize);
		case ISO_INS_VERIFY:
			return vgids_ins_verify(context, &s, response, responseSize);
		default:
			break;
	}

	/* return command not allowed */
	return vgids_create_response(ISO_STATUS_COMMANDNOTALLOWED, NULL, 0, response, responseSize);
}

void vgids_free(vgidsContext* context)
{
	if (context)
	{
		RSA_free(context->privateKey);
		RSA_free(context->publicKey);
		X509_free(context->certificate);
		Stream_Free(context->commandData, TRUE);
		Stream_Free(context->responseData, TRUE);
		free(context->pin);
		ArrayList_Free(context->files);
		free(context);
	}
}
