/*
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Licensing
 *
 * Copyright 2011-2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2018 David Fort <contact@hardening-consulting.com>
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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
#include <freerdp/license.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/crypto.h>
#include <winpr/shell.h>
#include <winpr/path.h>
#include <winpr/file.h>

#include <freerdp/log.h>

#include "redirection.h"
#include "certificate.h"

#include "license.h"

#define TAG FREERDP_TAG("core.license")

#if 0
#define LICENSE_NULL_CLIENT_RANDOM 1
#define LICENSE_NULL_PREMASTER_SECRET 1
#endif

#define PLATFORM_CHALLENGE_RESPONSE_VERSION 0x0100

/** @brief Licensing Packet Types */
enum LicenseRequestType
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

/** @brief license Error Codes
 */
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

/** @brief state Transition Codes
 */
enum
{
	ST_TOTAL_ABORT = 0x00000001,
	ST_NO_TRANSITION = 0x00000002,
	ST_RESET_PHASE_TO_START = 0x00000003,
	ST_RESEND_LAST_MESSAGE = 0x00000004
};

/** @brief Platform Challenge Types
 */
enum
{
	WIN32_PLATFORM_CHALLENGE_TYPE = 0x0100,
	WIN16_PLATFORM_CHALLENGE_TYPE = 0x0200,
	WINCE_PLATFORM_CHALLENGE_TYPE = 0x0300,
	OTHER_PLATFORM_CHALLENGE_TYPE = 0xFF00
};

/** @brief License Detail Levels
 */
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

struct rdp_license
{
	LICENSE_STATE state;
	LICENSE_TYPE type;
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
	LICENSE_BLOB* LicenseInfo; /* Client -> Server */
	LICENSE_BLOB* KeyExchangeList;
	LICENSE_BLOB* ServerCertificate;
	LICENSE_BLOB* ClientUserName;
	LICENSE_BLOB* ClientMachineName;
	LICENSE_BLOB* PlatformChallenge;
	LICENSE_BLOB* EncryptedPremasterSecret;
	LICENSE_BLOB* EncryptedPlatformChallenge;
	LICENSE_BLOB* EncryptedPlatformChallengeResponse;
	LICENSE_BLOB* EncryptedHardwareId;
	LICENSE_BLOB* EncryptedLicenseInfo;
	BYTE MACData[LICENSING_ENCRYPTION_KEY_LENGTH];
	SCOPE_LIST* ScopeList;
	UINT32 PacketHeaderLength;
	UINT32 PreferredKeyExchangeAlg;
	UINT32 PlatformId;
	UINT16 ClientType;
	UINT16 LicenseDetailLevel;
	BOOL update;
};

static BOOL license_send_error_alert(rdpLicense* license, UINT32 dwErrorCode,
                                     UINT32 dwStateTransition, const LICENSE_BLOB* info);
static BOOL license_set_state(rdpLicense* license, LICENSE_STATE state);
static const char* license_get_state_string(LICENSE_STATE state);

static const char* license_preferred_key_exchange_alg_string(UINT32 alg)
{
	static char buffer[64] = { 0 };
	const char* name = NULL;

	switch (alg)
	{
		case KEY_EXCHANGE_ALG_RSA:
			name = "KEY_EXCHANGE_ALG_RSA";
			break;
		default:
			name = "KEY_EXCHANGE_ALG_UNKNOWN";
			break;
	}

	_snprintf(buffer, sizeof(buffer), "%s [0x%08" PRIx32 "]", name, alg);
	return buffer;
}

static const char* license_request_type_string(UINT32 type)
{
	switch (type)
	{
		case LICENSE_REQUEST:
			return "LICENSE_REQUEST";
		case PLATFORM_CHALLENGE:
			return "PLATFORM_CHALLENGE";
		case NEW_LICENSE:
			return "NEW_LICENSE";
		case UPGRADE_LICENSE:
			return "UPGRADE_LICENSE";
		case LICENSE_INFO:
			return "LICENSE_INFO";
		case NEW_LICENSE_REQUEST:
			return "NEW_LICENSE_REQUEST";
		case PLATFORM_CHALLENGE_RESPONSE:
			return "PLATFORM_CHALLENGE_RESPONSE";
		case ERROR_ALERT:
			return "ERROR_ALERT";
		default:
			return "LICENSE_REQUEST_TYPE_UNKNOWN";
	}
}

static const char* licencse_blob_type_string(UINT16 type)
{
	switch (type)
	{
		case BB_ANY_BLOB:
			return "BB_ANY_BLOB";
		case BB_DATA_BLOB:
			return "BB_DATA_BLOB";
		case BB_RANDOM_BLOB:
			return "BB_RANDOM_BLOB";
		case BB_CERTIFICATE_BLOB:
			return "BB_CERTIFICATE_BLOB";
		case BB_ERROR_BLOB:
			return "BB_ERROR_BLOB";
		case BB_ENCRYPTED_DATA_BLOB:
			return "BB_ENCRYPTED_DATA_BLOB";
		case BB_KEY_EXCHG_ALG_BLOB:
			return "BB_KEY_EXCHG_ALG_BLOB";
		case BB_SCOPE_BLOB:
			return "BB_SCOPE_BLOB";
		case BB_CLIENT_USER_NAME_BLOB:
			return "BB_CLIENT_USER_NAME_BLOB";
		case BB_CLIENT_MACHINE_NAME_BLOB:
			return "BB_CLIENT_MACHINE_NAME_BLOB";
		default:
			return "BB_UNKNOWN";
	}
}
static wStream* license_send_stream_init(rdpLicense* license);

static void license_generate_randoms(rdpLicense* license);
static BOOL license_generate_keys(rdpLicense* license);
static BOOL license_generate_hwid(rdpLicense* license);
static BOOL license_encrypt_premaster_secret(rdpLicense* license);

static LICENSE_PRODUCT_INFO* license_new_product_info(void);
static void license_free_product_info(LICENSE_PRODUCT_INFO* productInfo);
static BOOL license_read_product_info(wStream* s, LICENSE_PRODUCT_INFO* productInfo);

static LICENSE_BLOB* license_new_binary_blob(UINT16 type);
static void license_free_binary_blob(LICENSE_BLOB* blob);
static BOOL license_read_binary_blob_data(LICENSE_BLOB* blob, UINT32 type, const void* data,
                                          size_t length);
static BOOL license_read_binary_blob(wStream* s, LICENSE_BLOB* blob);
static BOOL license_write_binary_blob(wStream* s, const LICENSE_BLOB* blob);

static SCOPE_LIST* license_new_scope_list(void);
static BOOL license_scope_list_resize(SCOPE_LIST* scopeList, UINT32 count);
static void license_free_scope_list(SCOPE_LIST* scopeList);
static BOOL license_read_scope_list(wStream* s, SCOPE_LIST* scopeList);
static BOOL license_write_scope_list(wStream* s, const SCOPE_LIST* scopeList);

static BOOL license_read_license_request_packet(rdpLicense* license, wStream* s);
static BOOL license_write_license_request_packet(const rdpLicense* license, wStream* s);

static BOOL license_read_platform_challenge_packet(rdpLicense* license, wStream* s);
static BOOL license_send_platform_challenge_packet(rdpLicense* license);
static BOOL license_read_new_or_upgrade_license_packet(rdpLicense* license, wStream* s);
static BOOL license_read_error_alert_packet(rdpLicense* license, wStream* s);

static BOOL license_write_new_license_request_packet(const rdpLicense* license, wStream* s);
static BOOL license_read_new_license_request_packet(rdpLicense* license, wStream* s);
static BOOL license_answer_license_request(rdpLicense* license);

static BOOL license_send_platform_challenge_response(rdpLicense* license);
static BOOL license_read_platform_challenge_response(rdpLicense* license, wStream* s);

static BOOL license_read_client_platform_challenge_response(rdpLicense* license, wStream* s);
static BOOL license_write_client_platform_challenge_response(rdpLicense* license, wStream* s);

static BOOL license_read_server_upgrade_license(rdpLicense* license, wStream* s);
static BOOL license_write_server_upgrade_license(const rdpLicense* license, wStream* s);

static BOOL license_send_license_info(rdpLicense* license, const LICENSE_BLOB* calBlob,
                                      BYTE* signature);
static BOOL license_read_license_info(rdpLicense* license, wStream* s);
static state_run_t license_client_recv(rdpLicense* license, wStream* s);
static state_run_t license_server_recv(rdpLicense* license, wStream* s);

#define PLATFORMID (CLIENT_OS_ID_WINNT_POST_52 | CLIENT_IMAGE_ID_MICROSOFT)

#ifdef WITH_DEBUG_LICENSE

static const char* const error_codes[] = { "ERR_UNKNOWN",
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
	                                       "ERR_INVALID_MESSAGE_LENGTH" };

static const char* const state_transitions[] = { "ST_UNKNOWN", "ST_TOTAL_ABORT", "ST_NO_TRANSITION",
	                                             "ST_RESET_PHASE_TO_START",
	                                             "ST_RESEND_LAST_MESSAGE" };

static void license_print_product_info(const LICENSE_PRODUCT_INFO* productInfo)
{
	char* CompanyName = NULL;
	char* ProductId = NULL;

	WINPR_ASSERT(productInfo);
	WINPR_ASSERT(productInfo->pbCompanyName);
	WINPR_ASSERT(productInfo->pbProductId);

	CompanyName = ConvertWCharToUtf8Alloc(productInfo->pbCompanyName,
	                                      productInfo->cbCompanyName / sizeof(WCHAR));
	ProductId =
	    ConvertWCharToUtf8Alloc(productInfo->pbProductId, productInfo->cbProductId / sizeof(WCHAR));
	WLog_INFO(TAG, "ProductInfo:");
	WLog_INFO(TAG, "\tdwVersion: 0x%08" PRIX32 "", productInfo->dwVersion);
	WLog_INFO(TAG, "\tCompanyName: %s", CompanyName);
	WLog_INFO(TAG, "\tProductId: %s", ProductId);
	free(CompanyName);
	free(ProductId);
}

static void license_print_scope_list(const SCOPE_LIST* scopeList)
{
	UINT32 index;

	WINPR_ASSERT(scopeList);

	WLog_INFO(TAG, "ScopeList (%" PRIu32 "):", scopeList->count);

	for (index = 0; index < scopeList->count; index++)
	{
		const LICENSE_BLOB* scope;

		WINPR_ASSERT(scopeList->array);
		scope = scopeList->array[index];
		WINPR_ASSERT(scope);

		WLog_INFO(TAG, "\t%s", (const char*)scope->data);
	}
}
#endif

static const char licenseStore[] = "licenses";

static BOOL license_ensure_state(rdpLicense* license, LICENSE_STATE state, UINT32 msg)
{
	const LICENSE_STATE cstate = license_get_state(license);

	WINPR_ASSERT(license);

	if (cstate != state)
	{
		const char* scstate = license_get_state_string(cstate);
		const char* sstate = license_get_state_string(state);
		const char* where = license_request_type_string(msg);

		WLog_WARN(TAG, "Received [%s], but found invalid licensing state %s, expected %s", where,
		          scstate, sstate);
		return FALSE;
	}
	return TRUE;
}

state_run_t license_recv(rdpLicense* license, wStream* s)
{
	WINPR_ASSERT(license);
	WINPR_ASSERT(license->rdp);
	WINPR_ASSERT(license->rdp->settings);

	if (freerdp_settings_get_bool(license->rdp->settings, FreeRDP_ServerMode))
		return license_server_recv(license, s);
	else
		return license_client_recv(license, s);
}

static BOOL license_check_stream_length(wStream* s, SSIZE_T expect, const char* where)
{
	const size_t remain = Stream_GetRemainingLength(s);

	WINPR_ASSERT(where);

	if (expect < 0)
	{
		WLog_WARN(TAG, "invalid %s, expected value %" PRIdz " invalid", where, expect);
		return FALSE;
	}
	if (remain < (size_t)expect)
	{
		WLog_WARN(TAG, "short %s, expected %" PRIdz " bytes, got %" PRIuz, where, expect, remain);
		return FALSE;
	}
	return TRUE;
}

static BOOL license_check_stream_capacity(wStream* s, size_t expect, const char* where)
{
	WINPR_ASSERT(where);

	if (Stream_GetRemainingCapacity(s) < expect)
	{
		WLog_WARN(TAG, "short capacity %s, expected %" PRIuz " bytes, got %" PRIuz, where, expect,
		          Stream_GetRemainingCapacity(s));
		return FALSE;
	}
	return TRUE;
}

static BOOL computeCalHash(const char* hostname, char* hashStr)
{
	WINPR_DIGEST_CTX* sha1 = NULL;
	BOOL ret = FALSE;
	BYTE hash[20] = { 0 };
	size_t i;

	WINPR_ASSERT(hostname);
	WINPR_ASSERT(hashStr);

	if (!(sha1 = winpr_Digest_New()))
		goto out;
	if (!winpr_Digest_Init(sha1, WINPR_MD_SHA1))
		goto out;
	if (!winpr_Digest_Update(sha1, (const BYTE*)hostname, strlen(hostname)))
		goto out;
	if (!winpr_Digest_Final(sha1, hash, sizeof(hash)))
		goto out;

	for (i = 0; i < sizeof(hash); i++, hashStr += 2)
		sprintf_s(hashStr, 3, "%.2x", hash[i]);

	ret = TRUE;
out:
	winpr_Digest_Free(sha1);
	return ret;
}

static BOOL saveCal(const rdpSettings* settings, const BYTE* data, size_t length,
                    const char* hostname)
{
	char hash[41] = { 0 };
	FILE* fp;
	char* licenseStorePath = NULL;
	char filename[MAX_PATH] = { 0 }, filenameNew[MAX_PATH] = { 0 };
	char *filepath = NULL, *filepathNew = NULL;

	size_t written;
	BOOL ret = FALSE;
	const char* path = freerdp_settings_get_string(settings, FreeRDP_ConfigPath);

	WINPR_ASSERT(path);
	WINPR_ASSERT(data || (length == 0));
	WINPR_ASSERT(hostname);

	if (!winpr_PathFileExists(path))
	{
		if (!winpr_PathMakePath(path, 0))
		{
			WLog_ERR(TAG, "error creating directory '%s'", path);
			goto out;
		}
		WLog_INFO(TAG, "creating directory %s", path);
	}

	if (!(licenseStorePath = GetCombinedPath(path, licenseStore)))
		goto out;

	if (!winpr_PathFileExists(licenseStorePath))
	{
		if (!winpr_PathMakePath(licenseStorePath, 0))
		{
			WLog_ERR(TAG, "error creating directory '%s'", licenseStorePath);
			goto out;
		}
		WLog_INFO(TAG, "creating directory %s", licenseStorePath);
	}

	if (!computeCalHash(hostname, hash))
		goto out;
	sprintf_s(filename, sizeof(filename) - 1, "%s.cal", hash);
	sprintf_s(filenameNew, sizeof(filenameNew) - 1, "%s.cal.new", hash);

	if (!(filepath = GetCombinedPath(licenseStorePath, filename)))
		goto out;

	if (!(filepathNew = GetCombinedPath(licenseStorePath, filenameNew)))
		goto out;

	fp = winpr_fopen(filepathNew, "wb");
	if (!fp)
		goto out;

	written = fwrite(data, length, 1, fp);
	fclose(fp);

	if (written != 1)
	{
		winpr_DeleteFile(filepathNew);
		goto out;
	}

	ret = winpr_MoveFileEx(filepathNew, filepath, MOVEFILE_REPLACE_EXISTING);

out:
	free(filepathNew);
	free(filepath);
	free(licenseStorePath);
	return ret;
}

static BYTE* loadCalFile(const rdpSettings* settings, const char* hostname, size_t* dataLen)
{
	char *licenseStorePath = NULL, *calPath = NULL;
	char calFilename[MAX_PATH] = { 0 };
	char hash[41] = { 0 };
	INT64 length;
	int status;
	FILE* fp;
	BYTE* ret = NULL;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(hostname);
	WINPR_ASSERT(dataLen);

	if (!computeCalHash(hostname, hash))
	{
		WLog_ERR(TAG, "loadCalFile: unable to compute hostname hash");
		return NULL;
	}

	sprintf_s(calFilename, sizeof(calFilename) - 1, "%s.cal", hash);

	if (!(licenseStorePath = GetCombinedPath(
	          freerdp_settings_get_string(settings, FreeRDP_ConfigPath), licenseStore)))
		return NULL;

	if (!(calPath = GetCombinedPath(licenseStorePath, calFilename)))
		goto error_path;

	fp = winpr_fopen(calPath, "rb");
	if (!fp)
		goto error_open;

	_fseeki64(fp, 0, SEEK_END);
	length = _ftelli64(fp);
	_fseeki64(fp, 0, SEEK_SET);
	if (length < 0)
		goto error_malloc;

	ret = (BYTE*)malloc((size_t)length);
	if (!ret)
		goto error_malloc;

	status = fread(ret, (size_t)length, 1, fp);
	if (status <= 0)
		goto error_read;

	*dataLen = (size_t)length;

	fclose(fp);
	free(calPath);
	free(licenseStorePath);
	return ret;

error_read:
	free(ret);
error_malloc:
	fclose(fp);
error_open:
	free(calPath);
error_path:
	free(licenseStorePath);
	return NULL;
}

/**
 * Read a licensing preamble.
 * msdn{cc240480}
 * @param s stream
 * @param bMsgType license message type
 * @param flags message flags
 * @param wMsgSize message size
 * @return if the operation completed successfully
 */

static BOOL license_read_preamble(wStream* s, BYTE* bMsgType, BYTE* flags, UINT16* wMsgSize)
{
	WINPR_ASSERT(bMsgType);
	WINPR_ASSERT(flags);
	WINPR_ASSERT(wMsgSize);

	/* preamble (4 bytes) */
	if (!license_check_stream_length(s, 4, "license preamble"))
		return FALSE;

	Stream_Read_UINT8(s, *bMsgType);  /* bMsgType (1 byte) */
	Stream_Read_UINT8(s, *flags);     /* flags (1 byte) */
	Stream_Read_UINT16(s, *wMsgSize); /* wMsgSize (2 bytes) */
	return license_check_stream_length(s, *wMsgSize - 4ll, "license preamble::wMsgSize");
}

/**
 * Write a licensing preamble.
 * msdn{cc240480}
 * @param s stream
 * @param bMsgType license message type
 * @param flags message flags
 * @param wMsgSize message size
 * @return if the operation completed successfully
 */

static BOOL license_write_preamble(wStream* s, BYTE bMsgType, BYTE flags, UINT16 wMsgSize)
{
	if (!Stream_EnsureRemainingCapacity(s, 4))
		return FALSE;

	/* preamble (4 bytes) */
	Stream_Write_UINT8(s, bMsgType);  /* bMsgType (1 byte) */
	Stream_Write_UINT8(s, flags);     /* flags (1 byte) */
	Stream_Write_UINT16(s, wMsgSize); /* wMsgSize (2 bytes) */
	return TRUE;
}

/**
 * @brief Initialize a license packet stream.
 *
 * @param license license module
 *
 * @return stream or NULL
 */

wStream* license_send_stream_init(rdpLicense* license)
{
	wStream* s;
	BOOL do_crypt;

	WINPR_ASSERT(license);
	WINPR_ASSERT(license->rdp);

	do_crypt = license->rdp->do_crypt;

	license->rdp->sec_flags = SEC_LICENSE_PKT;

	/*
	 * Encryption of licensing packets is optional even if the rdp security
	 * layer is used. If the peer has not indicated that it is capable of
	 * processing encrypted licensing packets (rdp->do_crypt_license) we turn
	 * off encryption (via rdp->do_crypt) before initializing the rdp stream
	 * and reenable it afterwards.
	 */

	if (do_crypt)
	{
		license->rdp->sec_flags |= SEC_LICENSE_ENCRYPT_CS;
		license->rdp->do_crypt = license->rdp->do_crypt_license;
	}

	s = rdp_send_stream_init(license->rdp);
	if (!s)
		return NULL;

	license->rdp->do_crypt = do_crypt;
	license->PacketHeaderLength = Stream_GetPosition(s);
	if (!Stream_SafeSeek(s, LICENSE_PREAMBLE_LENGTH))
		goto fail;
	return s;

fail:
	Stream_Release(s);
	return NULL;
}

/**
 * Send an RDP licensing packet.
 * msdn{cc240479}
 * @param license license module
 * @param s stream
 */

static BOOL license_send(rdpLicense* license, wStream* s, BYTE type)
{
	size_t length;
	BYTE flags;
	UINT16 wMsgSize;
	rdpRdp* rdp;
	BOOL ret;

	WINPR_ASSERT(license);
	WINPR_ASSERT(license->rdp);

	rdp = license->rdp;
	WINPR_ASSERT(rdp->settings);

	DEBUG_LICENSE("Sending %s Packet", license_request_type_string(type));
	length = Stream_GetPosition(s);
	wMsgSize = length - license->PacketHeaderLength;
	Stream_SetPosition(s, license->PacketHeaderLength);
	flags = PREAMBLE_VERSION_3_0;

	/**
	 * Using EXTENDED_ERROR_MSG_SUPPORTED here would cause mstsc to crash when
	 * running in server mode! This flag seems to be incorrectly documented.
	 */

	if (!rdp->settings->ServerMode)
		flags |= EXTENDED_ERROR_MSG_SUPPORTED;

	if (!license_write_preamble(s, type, flags, wMsgSize))
		return FALSE;

#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "Sending %s Packet, length %" PRIu16 "", license_request_type_string(type),
	         wMsgSize);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Pointer(s) - LICENSE_PREAMBLE_LENGTH, wMsgSize);
#endif
	Stream_SetPosition(s, length);
	ret = rdp_send(rdp, s, MCS_GLOBAL_CHANNEL_ID);
	rdp->sec_flags = 0;
	return ret;
}

BOOL license_read_server_upgrade_license(rdpLicense* license, wStream* s)
{
	WINPR_ASSERT(license);

	if (!license_read_binary_blob(s, license->EncryptedLicenseInfo))
		return FALSE;
	if (!license_check_stream_length(s, sizeof(license->MACData),
	                                 "SERVER_UPGRADE_LICENSE::MACData"))
		return FALSE;
	Stream_Read(s, license->MACData, sizeof(license->MACData));
	return TRUE;
}

BOOL license_write_server_upgrade_license(const rdpLicense* license, wStream* s)
{
	WINPR_ASSERT(license);

	if (!license_write_binary_blob(s, license->EncryptedLicenseInfo))
		return FALSE;
	if (!license_check_stream_capacity(s, sizeof(license->MACData),
	                                   "SERVER_UPGRADE_LICENSE::MACData"))
		return FALSE;
	Stream_Write(s, license->MACData, sizeof(license->MACData));
	return TRUE;
}

static BOOL license_server_send_new_or_upgrade_license(rdpLicense* license, BOOL upgrade)
{
	wStream* s = license_send_stream_init(license);
	const BYTE type = upgrade ? UPGRADE_LICENSE : NEW_LICENSE;

	if (!s)
		return FALSE;

	if (!license_write_server_upgrade_license(license, s))
		goto fail;

	return license_send(license, s, type);

fail:
	Stream_Release(s);
	return FALSE;
}

/**
 * Receive an RDP licensing packet.
 * msdn{cc240479}
 * @param license license module
 * @param s stream
 * @return if the operation completed successfully
 */

int license_client_recv(rdpLicense* license, wStream* s)
{
	BYTE flags;
	BYTE bMsgType;
	UINT16 wMsgSize;
	const size_t length = Stream_GetRemainingLength(s);

	WINPR_ASSERT(license);

	if (!license_read_preamble(s, &bMsgType, &flags, &wMsgSize)) /* preamble (4 bytes) */
		return STATE_RUN_FAILED;

	DEBUG_LICENSE("Receiving %s Packet", license_request_type_string(bMsgType));

	switch (bMsgType)
	{
		case LICENSE_REQUEST:
			/* Client does not require configuration, so skip this state */
			if (license_get_state(license) == LICENSE_STATE_INITIAL)
				license_set_state(license, LICENSE_STATE_CONFIGURED);

			if (!license_ensure_state(license, LICENSE_STATE_CONFIGURED, bMsgType))
				return STATE_RUN_FAILED;

			if (!license_read_license_request_packet(license, s))
				return STATE_RUN_FAILED;

			if (!license_answer_license_request(license))
				return STATE_RUN_FAILED;

			license_set_state(license, LICENSE_STATE_NEW_REQUEST);
			break;

		case PLATFORM_CHALLENGE:
			if (!license_ensure_state(license, LICENSE_STATE_NEW_REQUEST, bMsgType))
				return STATE_RUN_FAILED;

			if (!license_read_platform_challenge_packet(license, s))
				return STATE_RUN_FAILED;

			if (!license_send_platform_challenge_response(license))
				return STATE_RUN_FAILED;
			license_set_state(license, LICENSE_STATE_PLATFORM_CHALLENGE_RESPONSE);
			break;

		case NEW_LICENSE:
		case UPGRADE_LICENSE:
			if (!license_ensure_state(license, LICENSE_STATE_PLATFORM_CHALLENGE_RESPONSE, bMsgType))
				return STATE_RUN_FAILED;
			if (!license_read_new_or_upgrade_license_packet(license, s))
				return STATE_RUN_FAILED;
			break;

		case ERROR_ALERT:
			if (!license_read_error_alert_packet(license, s))
				return STATE_RUN_FAILED;
			break;

		default:
			WLog_ERR(TAG, "invalid bMsgType:%" PRIu8 "", bMsgType);
			return STATE_RUN_FAILED;
	}

	if (!tpkt_ensure_stream_consumed(s, length))
		return STATE_RUN_FAILED;
	return STATE_RUN_SUCCESS;
}

state_run_t license_server_recv(rdpLicense* license, wStream* s)
{
	state_run_t rc = STATE_RUN_FAILED;
	BYTE flags;
	BYTE bMsgType;
	UINT16 wMsgSize;
	const size_t length = Stream_GetRemainingLength(s);

	WINPR_ASSERT(license);

	if (!license_read_preamble(s, &bMsgType, &flags, &wMsgSize)) /* preamble (4 bytes) */
		goto fail;

	DEBUG_LICENSE("Receiving %s Packet", license_request_type_string(bMsgType));

	switch (bMsgType)
	{
		case NEW_LICENSE_REQUEST:
			if (!license_ensure_state(license, LICENSE_STATE_REQUEST, bMsgType))
				goto fail;
			if (!license_read_new_license_request_packet(license, s))
				goto fail;
			// TODO: Validate if client is allowed
			if (!license_send_error_alert(license, ERR_INVALID_MAC, ST_TOTAL_ABORT,
			                              license->ErrorInfo))
				goto fail;
			if (!license_send_platform_challenge_packet(license))
				goto fail;
			license->update = FALSE;
			if (!license_set_state(license, LICENSE_STATE_PLATFORM_CHALLENGE))
				goto fail;
			break;
		case LICENSE_INFO:
			if (!license_ensure_state(license, LICENSE_STATE_REQUEST, bMsgType))
				goto fail;
			if (!license_read_license_info(license, s))
				goto fail;
			// TODO: Validate license info
			if (!license_send_platform_challenge_packet(license))
				goto fail;
			if (!license_set_state(license, LICENSE_STATE_PLATFORM_CHALLENGE))
				goto fail;
			license->update = TRUE;
			break;

		case PLATFORM_CHALLENGE_RESPONSE:
			if (!license_ensure_state(license, LICENSE_STATE_PLATFORM_CHALLENGE, bMsgType))
				goto fail;
			if (!license_read_client_platform_challenge_response(license, s))
				goto fail;

			// TODO: validate challenge response
			if (FALSE)
			{
				if (license_send_error_alert(license, ERR_INVALID_CLIENT, ST_TOTAL_ABORT,
				                             license->ErrorInfo))
					goto fail;
			}
			else
			{
				if (!license_server_send_new_or_upgrade_license(license, license->update))
					goto fail;

				license->type = LICENSE_TYPE_ISSUED;
				license_set_state(license, LICENSE_STATE_COMPLETED);

				rc = STATE_RUN_CONTINUE; /* License issued, switch state */
			}
			break;

		case ERROR_ALERT:
			if (!license_read_error_alert_packet(license, s))
				goto fail;
			break;

		default:
			WLog_ERR(TAG, "invalid bMsgType:%" PRIu8 "", bMsgType);
			goto fail;
	}

	if (!tpkt_ensure_stream_consumed(s, length))
		goto fail;

	if (!state_run_success(rc))
		rc = STATE_RUN_SUCCESS;

fail:
	if (state_run_failed(rc))
	{
		if (flags & EXTENDED_ERROR_MSG_SUPPORTED)
			license_send_error_alert(license, ERR_INVALID_CLIENT, ST_TOTAL_ABORT, NULL);
		license_set_state(license, LICENSE_STATE_ABORTED);
	}

	return rc;
}

void license_generate_randoms(rdpLicense* license)
{
	WINPR_ASSERT(license);

#ifdef LICENSE_NULL_CLIENT_RANDOM
	ZeroMemory(license->ClientRandom, CLIENT_RANDOM_LENGTH); /* ClientRandom */
#else
	winpr_RAND(license->ClientRandom, CLIENT_RANDOM_LENGTH);       /* ClientRandom */
#endif

	winpr_RAND(license->ServerRandom, SERVER_RANDOM_LENGTH); /* ServerRandom */

#ifdef LICENSE_NULL_PREMASTER_SECRET
	ZeroMemory(license->PremasterSecret, PREMASTER_SECRET_LENGTH); /* PremasterSecret */
#else
	winpr_RAND(license->PremasterSecret, PREMASTER_SECRET_LENGTH); /* PremasterSecret */
#endif
}

/**
 * Generate License Cryptographic Keys.
 * @param license license module
 */

static BOOL license_generate_keys(rdpLicense* license)
{
	BOOL ret;

	WINPR_ASSERT(license);

	if (
	    /* MasterSecret */
	    !security_master_secret(license->PremasterSecret, license->ClientRandom,
	                            license->ServerRandom, license->MasterSecret) ||
	    /* SessionKeyBlob */
	    !security_session_key_blob(license->MasterSecret, license->ClientRandom,
	                               license->ServerRandom, license->SessionKeyBlob))
	{
		return FALSE;
	}
	security_mac_salt_key(license->SessionKeyBlob, license->ClientRandom, license->ServerRandom,
	                      license->MacSaltKey); /* MacSaltKey */
	ret = security_licensing_encryption_key(
	    license->SessionKeyBlob, license->ClientRandom, license->ServerRandom,
	    license->LicensingEncryptionKey); /* LicensingEncryptionKey */
#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "ClientRandom:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->ClientRandom, CLIENT_RANDOM_LENGTH);
	WLog_DBG(TAG, "ServerRandom:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->ServerRandom, SERVER_RANDOM_LENGTH);
	WLog_DBG(TAG, "PremasterSecret:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->PremasterSecret, PREMASTER_SECRET_LENGTH);
	WLog_DBG(TAG, "MasterSecret:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->MasterSecret, MASTER_SECRET_LENGTH);
	WLog_DBG(TAG, "SessionKeyBlob:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->SessionKeyBlob, SESSION_KEY_BLOB_LENGTH);
	WLog_DBG(TAG, "MacSaltKey:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->MacSaltKey, MAC_SALT_KEY_LENGTH);
	WLog_DBG(TAG, "LicensingEncryptionKey:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->LicensingEncryptionKey,
	              LICENSING_ENCRYPTION_KEY_LENGTH);
#endif
	return ret;
}

/**
 * Generate Unique Hardware Identifier (CLIENT_HARDWARE_ID).
 * @param license license module
 */

BOOL license_generate_hwid(rdpLicense* license)
{
	const BYTE* hashTarget;
	size_t targetLen;
	BYTE macAddress[6] = { 0 };

	WINPR_ASSERT(license);
	WINPR_ASSERT(license->rdp);
	WINPR_ASSERT(license->rdp->settings);

	ZeroMemory(license->HardwareId, HWID_LENGTH);

	if (license->rdp->settings->OldLicenseBehaviour)
	{
		hashTarget = macAddress;
		targetLen = sizeof(macAddress);
	}
	else
	{
		wStream buffer;
		wStream* s;
		const char* hostname = license->rdp->settings->ClientHostname;
		s = Stream_StaticInit(&buffer, license->HardwareId, 4);
		Stream_Write_UINT32(s, license->PlatformId);
		Stream_Free(s, TRUE);

		hashTarget = (const BYTE*)hostname;
		targetLen = strlen(hostname);
	}

	/* Allow FIPS override for use of MD5 here, really this does not have to be MD5 as we are just
	 * taking a MD5 hash of the 6 bytes of 0's(macAddress) */
	/* and filling in the Data1-Data4 fields of the CLIENT_HARDWARE_ID structure(from MS-RDPELE
	 * section 2.2.2.3.1). This is for RDP licensing packets */
	/* which will already be encrypted under FIPS, so the use of MD5 here is not for sensitive data
	 * protection. */
	return winpr_Digest_Allow_FIPS(WINPR_MD_MD5, hashTarget, targetLen,
	                               &license->HardwareId[HWID_PLATFORM_ID_LENGTH],
	                               WINPR_MD5_DIGEST_LENGTH);
}

static BOOL license_get_server_rsa_public_key(rdpLicense* license)
{
	BYTE* Exponent;
	BYTE* Modulus;
	int ModulusLength;
	rdpSettings* settings;

	WINPR_ASSERT(license);
	WINPR_ASSERT(license->certificate);
	WINPR_ASSERT(license->rdp);

	settings = license->rdp->settings;
	WINPR_ASSERT(settings);

	if (license->ServerCertificate->length < 1)
	{
		if (!certificate_read_server_certificate(license->certificate, settings->ServerCertificate,
		                                         settings->ServerCertificateLength))
			return FALSE;
	}

	Exponent = license->certificate->cert_info.exponent;
	Modulus = license->certificate->cert_info.Modulus;
	ModulusLength = license->certificate->cert_info.ModulusLength;
	CopyMemory(license->Exponent, Exponent, 4);
	license->ModulusLength = ModulusLength;
	license->Modulus = (BYTE*)malloc(ModulusLength);
	if (!license->Modulus)
		return FALSE;
	CopyMemory(license->Modulus, Modulus, ModulusLength);
	return TRUE;
}

BOOL license_encrypt_premaster_secret(rdpLicense* license)
{
	BYTE* EncryptedPremasterSecret;

	WINPR_ASSERT(license);

	if (!license_get_server_rsa_public_key(license))
		return FALSE;

	WINPR_ASSERT(license->EncryptedPremasterSecret);

#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "Modulus (%" PRIu32 " bits):", license->ModulusLength * 8);
	winpr_HexDump(TAG, WLOG_DEBUG, license->Modulus, license->ModulusLength);
	WLog_DBG(TAG, "Exponent:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->Exponent, 4);
#endif

	EncryptedPremasterSecret = (BYTE*)calloc(1, license->ModulusLength);
	if (!EncryptedPremasterSecret)
		return FALSE;

	license->EncryptedPremasterSecret->type = BB_RANDOM_BLOB;
	license->EncryptedPremasterSecret->length = PREMASTER_SECRET_LENGTH;
#ifndef LICENSE_NULL_PREMASTER_SECRET
	{
		SSIZE_T length = crypto_rsa_public_encrypt(
		    license->PremasterSecret, PREMASTER_SECRET_LENGTH, license->ModulusLength,
		    license->Modulus, license->Exponent, EncryptedPremasterSecret);
		if ((length < 0) || (length > UINT16_MAX))
			return FALSE;
		license->EncryptedPremasterSecret->length = (UINT16)length;
	}
#endif
	license->EncryptedPremasterSecret->data = EncryptedPremasterSecret;
	return TRUE;
}

static BOOL license_rc4_with_licenseKey(const rdpLicense* license, const BYTE* input, size_t len,
                                        LICENSE_BLOB* target)
{
	WINPR_RC4_CTX* rc4;
	BYTE* buffer = NULL;

	WINPR_ASSERT(license);
	WINPR_ASSERT(input || (len == 0));
	WINPR_ASSERT(target);

	rc4 =
	    winpr_RC4_New_Allow_FIPS(license->LicensingEncryptionKey, LICENSING_ENCRYPTION_KEY_LENGTH);
	if (!rc4)
		return FALSE;

	buffer = (BYTE*)realloc(target->data, len);
	if (!buffer)
		goto error_buffer;

	target->data = buffer;
	target->length = len;

	if (!winpr_RC4_Update(rc4, len, input, buffer))
		goto error_buffer;

	winpr_RC4_Free(rc4);
	return TRUE;

error_buffer:
	winpr_RC4_Free(rc4);
	return FALSE;
}

/**
 * Encrypt the input using the license key and MAC the input for a signature
 *
 * @param license rdpLicense to get keys and salt from
 * @param input the input data to encrypt and MAC
 * @param len size of input
 * @param target a target LICENSE_BLOB where the encrypted input will be stored
 * @param mac the signature buffer (16 bytes)
 * @return if the operation completed successfully
 */
static BOOL license_encrypt_and_MAC(rdpLicense* license, const BYTE* input, size_t len,
                                    LICENSE_BLOB* target, BYTE* mac)
{
	WINPR_ASSERT(license);
	return license_rc4_with_licenseKey(license, input, len, target) &&
	       security_mac_data(license->MacSaltKey, input, len, mac);
}

/**
 * Decrypt the input using the license key and check the MAC
 *
 * @param license rdpLicense to get keys and salt from
 * @param input the input data to decrypt and MAC
 * @param len size of input
 * @param target a target LICENSE_BLOB where the decrypted input will be stored
 * @param packetMac the signature buffer (16 bytes)
 *
 * @return if the operation completed successfully
 */
static BOOL license_decrypt_and_check_MAC(rdpLicense* license, const BYTE* input, size_t len,
                                          LICENSE_BLOB* target, const BYTE* packetMac)
{
	BYTE macData[16] = { 0 };

	WINPR_ASSERT(license);
	WINPR_ASSERT(target);

	if (freerdp_settings_get_bool(license->rdp->settings, FreeRDP_TransportDumpReplay))
		return TRUE;

	return license_rc4_with_licenseKey(license, input, len, target) &&
	       security_mac_data(license->MacSaltKey, target->data, len, macData) &&
	       (memcmp(packetMac, macData, sizeof(macData)) == 0);
}

/**
 * Read Product Information (PRODUCT_INFO).
 * msdn{cc241915}
 * @param s stream
 * @param productInfo product information
 */

BOOL license_read_product_info(wStream* s, LICENSE_PRODUCT_INFO* productInfo)
{
	WINPR_ASSERT(productInfo);

	if (!license_check_stream_length(s, 8, "license product info::cbCompanyName"))
		return FALSE;

	Stream_Read_UINT32(s, productInfo->dwVersion);     /* dwVersion (4 bytes) */
	Stream_Read_UINT32(s, productInfo->cbCompanyName); /* cbCompanyName (4 bytes) */

	/* Name must be >0, but there is no upper limit defined, use UINT32_MAX */
	if ((productInfo->cbCompanyName < 2) || (productInfo->cbCompanyName % 2 != 0))
	{
		WLog_WARN(TAG, "license product info invalid cbCompanyName %" PRIu32,
		          productInfo->cbCompanyName);
		return FALSE;
	}

	if (!license_check_stream_length(s, productInfo->cbCompanyName,
	                                 "license product info::CompanyName"))
		return FALSE;

	productInfo->pbProductId = NULL;
	productInfo->pbCompanyName = (BYTE*)malloc(productInfo->cbCompanyName);
	if (!productInfo->pbCompanyName)
		goto out_fail;
	Stream_Read(s, productInfo->pbCompanyName, productInfo->cbCompanyName);

	if (!license_check_stream_length(s, 4, "license product info::cbProductId"))
		goto out_fail;

	Stream_Read_UINT32(s, productInfo->cbProductId); /* cbProductId (4 bytes) */

	if ((productInfo->cbProductId < 2) || (productInfo->cbProductId % 2 != 0))
	{
		WLog_WARN(TAG, "license product info invalid cbProductId %" PRIu32,
		          productInfo->cbProductId);
		goto out_fail;
	}

	if (!license_check_stream_length(s, productInfo->cbProductId,
	                                 "license product info::ProductId"))
		goto out_fail;

	productInfo->pbProductId = (BYTE*)malloc(productInfo->cbProductId);
	if (!productInfo->pbProductId)
		goto out_fail;
	Stream_Read(s, productInfo->pbProductId, productInfo->cbProductId);
	return TRUE;

out_fail:
	free(productInfo->pbCompanyName);
	free(productInfo->pbProductId);
	productInfo->pbCompanyName = NULL;
	productInfo->pbProductId = NULL;
	return FALSE;
}

static BOOL license_write_product_info(wStream* s, const LICENSE_PRODUCT_INFO* productInfo)
{
	WINPR_ASSERT(productInfo);

	if (!license_check_stream_capacity(s, 8, "license product info::cbCompanyName"))
		return FALSE;

	Stream_Write_UINT32(s, productInfo->dwVersion);     /* dwVersion (4 bytes) */
	Stream_Write_UINT32(s, productInfo->cbCompanyName); /* cbCompanyName (4 bytes) */

	/* Name must be >0, but there is no upper limit defined, use UINT32_MAX */
	if ((productInfo->cbCompanyName < 2) || (productInfo->cbCompanyName % 2 != 0) ||
	    !productInfo->pbCompanyName)
	{
		WLog_WARN(TAG, "license product info invalid cbCompanyName %" PRIu32,
		          productInfo->cbCompanyName);
		return FALSE;
	}

	if (!license_check_stream_capacity(s, productInfo->cbCompanyName,
	                                   "license product info::CompanyName"))
		return FALSE;

	Stream_Write(s, productInfo->pbCompanyName, productInfo->cbCompanyName);

	if (!license_check_stream_capacity(s, 4, "license product info::cbProductId"))
		return FALSE;

	Stream_Write_UINT32(s, productInfo->cbProductId); /* cbProductId (4 bytes) */

	if ((productInfo->cbProductId < 2) || (productInfo->cbProductId % 2 != 0) ||
	    !productInfo->pbProductId)
	{
		WLog_WARN(TAG, "license product info invalid cbProductId %" PRIu32,
		          productInfo->cbProductId);
		return FALSE;
	}

	if (!license_check_stream_capacity(s, productInfo->cbProductId,
	                                   "license product info::ProductId"))
		return FALSE;

	Stream_Write(s, productInfo->pbProductId, productInfo->cbProductId);
	return TRUE;
}

/**
 * Allocate New Product Information (LICENSE_PRODUCT_INFO).
 * msdn{cc241915}
 * @return new product information
 */

LICENSE_PRODUCT_INFO* license_new_product_info(void)
{
	LICENSE_PRODUCT_INFO* productInfo =
	    (LICENSE_PRODUCT_INFO*)calloc(1, sizeof(LICENSE_PRODUCT_INFO));
	if (!productInfo)
		return NULL;
	return productInfo;
}

/**
 * Free Product Information (LICENSE_PRODUCT_INFO).
 * msdn{cc241915}
 * @param productInfo product information
 */

void license_free_product_info(LICENSE_PRODUCT_INFO* productInfo)
{
	if (productInfo)
	{
		free(productInfo->pbCompanyName);
		free(productInfo->pbProductId);
		free(productInfo);
	}
}

BOOL license_read_binary_blob_data(LICENSE_BLOB* blob, UINT32 wBlobType, const void* data,
                                   size_t length)
{
	WINPR_ASSERT(blob);
	WINPR_ASSERT(length <= UINT16_MAX);
	WINPR_ASSERT(data || (length == 0));

	blob->length = (UINT16)length;
	free(blob->data);
	blob->data = NULL;

	if ((blob->type != wBlobType) && (blob->type != BB_ANY_BLOB))
	{
		WLog_ERR(TAG, "license binary blob::type expected %s, got %s",
		         licencse_blob_type_string(wBlobType), licencse_blob_type_string(blob->type));
	}

	/*
	 * Server can choose to not send data by setting length to 0.
	 * If so, it may not bother to set the type, so shortcut the warning
	 */
	if ((blob->type != BB_ANY_BLOB) && (blob->length == 0))
	{
		WLog_WARN(TAG, "license binary blob::type %s, length=0, skipping.",
		          licencse_blob_type_string(blob->type));
		return TRUE;
	}

	blob->type = wBlobType;
	blob->data = (BYTE*)malloc(blob->length);
	if (!blob->data)
		return FALSE;
	memcpy(blob->data, data, blob->length); /* blobData */
	return TRUE;
}

/**
 * Read License Binary Blob (LICENSE_BINARY_BLOB).
 * msdn{cc240481}
 * @param s stream
 * @param blob license binary blob
 */

BOOL license_read_binary_blob(wStream* s, LICENSE_BLOB* blob)
{
	UINT16 wBlobType;
	UINT16 length;

	WINPR_ASSERT(blob);

	if (!license_check_stream_length(s, 4, "license binary blob::type"))
		return FALSE;

	Stream_Read_UINT16(s, wBlobType); /* wBlobType (2 bytes) */
	Stream_Read_UINT16(s, length);    /* wBlobLen (2 bytes) */

	if (!license_check_stream_length(s, length, "license binary blob::length"))
		return FALSE;

	if (!license_read_binary_blob_data(blob, wBlobType, Stream_Pointer(s), length))
		return FALSE;

	return Stream_SafeSeek(s, length);
}

/**
 * Write License Binary Blob (LICENSE_BINARY_BLOB).
 * msdn{cc240481}
 * @param s stream
 * @param blob license binary blob
 */

BOOL license_write_binary_blob(wStream* s, const LICENSE_BLOB* blob)
{
	WINPR_ASSERT(blob);

	if (!Stream_EnsureRemainingCapacity(s, blob->length + 4))
		return FALSE;

	Stream_Write_UINT16(s, blob->type);   /* wBlobType (2 bytes) */
	Stream_Write_UINT16(s, blob->length); /* wBlobLen (2 bytes) */

	if (blob->length > 0)
		Stream_Write(s, blob->data, blob->length); /* blobData */
	return TRUE;
}

static BOOL license_write_encrypted_premaster_secret_blob(wStream* s, const LICENSE_BLOB* blob,
                                                          UINT32 ModulusLength)
{
	const UINT32 length = ModulusLength + 8;

	WINPR_ASSERT(blob);

	if (blob->length > ModulusLength)
	{
		WLog_ERR(TAG, "license_write_encrypted_premaster_secret_blob: invalid blob");
		return FALSE;
	}

	if (!Stream_EnsureRemainingCapacity(s, length + 4))
		return FALSE;
	Stream_Write_UINT16(s, blob->type); /* wBlobType (2 bytes) */
	Stream_Write_UINT16(s, length);     /* wBlobLen (2 bytes) */

	if (blob->length > 0)
		Stream_Write(s, blob->data, blob->length); /* blobData */

	Stream_Zero(s, length - blob->length);
	return TRUE;
}

static BOOL license_read_encrypted_premaster_secret_blob(wStream* s, LICENSE_BLOB* blob,
                                                         UINT32* ModulusLength)
{
	if (!license_read_binary_blob(s, blob))
		return FALSE;
	// TODO
	return TRUE;
}

/**
 * Allocate New License Binary Blob (LICENSE_BINARY_BLOB).
 * msdn{cc240481}
 * @return new license binary blob
 */

LICENSE_BLOB* license_new_binary_blob(UINT16 type)
{
	LICENSE_BLOB* blob = (LICENSE_BLOB*)calloc(1, sizeof(LICENSE_BLOB));
	if (blob)
		blob->type = type;
	return blob;
}

/**
 * Free License Binary Blob (LICENSE_BINARY_BLOB).
 * msdn{cc240481}
 * @param blob license binary blob
 */

void license_free_binary_blob(LICENSE_BLOB* blob)
{
	if (blob)
	{
		free(blob->data);
		free(blob);
	}
}

/**
 * Read License Scope List (SCOPE_LIST).
 * msdn{cc241916}
 * @param s stream
 * @param scopeList scope list
 */

BOOL license_read_scope_list(wStream* s, SCOPE_LIST* scopeList)
{
	UINT32 i;
	UINT32 scopeCount;

	WINPR_ASSERT(scopeList);

	if (!license_check_stream_length(s, 4, "license scope list"))
		return FALSE;

	Stream_Read_UINT32(s, scopeCount); /* ScopeCount (4 bytes) */

	if (!license_check_stream_length(s, scopeCount * 4ull, "license scope list::count"))
		return FALSE;

	if (!license_scope_list_resize(scopeList, scopeCount))
		return FALSE;
	/* ScopeArray */
	for (i = 0; i < scopeCount; i++)
	{
		if (!license_read_binary_blob(s, scopeList->array[i]))
			return FALSE;
	}

	return TRUE;
}

BOOL license_write_scope_list(wStream* s, const SCOPE_LIST* scopeList)
{
	UINT32 i;

	WINPR_ASSERT(scopeList);

	if (!license_check_stream_capacity(s, 4, "license scope list"))
		return FALSE;

	Stream_Write_UINT32(s, scopeList->count); /* ScopeCount (4 bytes) */

	if (!license_check_stream_capacity(s, scopeList->count * 4ull, "license scope list::count"))
		return FALSE;

	/* ScopeArray */
	WINPR_ASSERT(scopeList->array || (scopeList->count == 0));
	for (i = 0; i < scopeList->count; i++)
	{
		const LICENSE_BLOB* element = scopeList->array[i];

		if (!license_write_binary_blob(s, element))
			return FALSE;
	}

	return TRUE;
}

/**
 * Allocate New License Scope List (SCOPE_LIST).
 * msdn{cc241916}
 * @return new scope list
 */

SCOPE_LIST* license_new_scope_list(void)
{
	SCOPE_LIST* list = calloc(1, sizeof(SCOPE_LIST));
	return list;
}

BOOL license_scope_list_resize(SCOPE_LIST* scopeList, UINT32 count)
{
	UINT32 x;

	WINPR_ASSERT(scopeList);
	WINPR_ASSERT(scopeList->array || (scopeList->count == 0));

	for (x = count; x < scopeList->count; x++)
	{
		license_free_binary_blob(scopeList->array[x]);
		scopeList->array[x] = NULL;
	}

	if (count > 0)
	{
		LICENSE_BLOB** tmp = realloc(scopeList->array, count * sizeof(LICENSE_BLOB*));
		if (!tmp)
			return FALSE;
		scopeList->array = tmp;
	}
	else
	{
		free(scopeList->array);
		scopeList->array = NULL;
	}

	for (x = scopeList->count; x < count; x++)
	{
		LICENSE_BLOB* blob = license_new_binary_blob(BB_SCOPE_BLOB);
		if (!blob)
		{
			scopeList->count = x;
			return FALSE;
		}
		scopeList->array[x] = blob;
	}

	scopeList->count = count;
	return TRUE;
}

/**
 * Free License Scope List (SCOPE_LIST).
 * msdn{cc241916}
 * @param scopeList scope list
 */

void license_free_scope_list(SCOPE_LIST* scopeList)
{
	if (!scopeList)
		return;

	license_scope_list_resize(scopeList, 0);
	free(scopeList);
}

BOOL license_send_license_info(rdpLicense* license, const LICENSE_BLOB* calBlob, BYTE* signature)
{
	wStream* s = license_send_stream_init(license);

	WINPR_ASSERT(calBlob);
	WINPR_ASSERT(signature);

	if (!s)
		return FALSE;

	if (!license_check_stream_capacity(s, 8 + CLIENT_RANDOM_LENGTH, "license info::ClientRandom"))
		return FALSE;

	Stream_Write_UINT32(s,
	                    license->PreferredKeyExchangeAlg); /* PreferredKeyExchangeAlg (4 bytes) */
	Stream_Write_UINT32(s, license->PlatformId);           /* PlatformId (4 bytes) */

	/* ClientRandom (32 bytes) */
	Stream_Write(s, license->ClientRandom, CLIENT_RANDOM_LENGTH);

	/* Licensing Binary Blob with EncryptedPreMasterSecret: */
	if (!license_write_encrypted_premaster_secret_blob(s, license->EncryptedPremasterSecret,
	                                                   license->ModulusLength))
		goto error;

	/* Licensing Binary Blob with LicenseInfo: */
	if (!license_write_binary_blob(s, calBlob))
		goto error;

	/* Licensing Binary Blob with EncryptedHWID */
	if (!license_write_binary_blob(s, license->EncryptedHardwareId))
		goto error;

	/* MACData */
	if (!license_check_stream_capacity(s, LICENSING_ENCRYPTION_KEY_LENGTH, "license info::MACData"))
		goto error;
	Stream_Write(s, signature, LICENSING_ENCRYPTION_KEY_LENGTH);

	return license_send(license, s, LICENSE_INFO);

error:
	Stream_Release(s);
	return FALSE;
}

static BOOL license_check_preferred_alg(rdpLicense* license, UINT32 PreferredKeyExchangeAlg,
                                        const char* where)
{
	WINPR_ASSERT(license);
	WINPR_ASSERT(where);

	if (license->PreferredKeyExchangeAlg != PreferredKeyExchangeAlg)
	{
		WLog_WARN(TAG, "%s::PreferredKeyExchangeAlg, expected %s, got %s", where,
		          license_preferred_key_exchange_alg_string(license->PreferredKeyExchangeAlg),
		          license_preferred_key_exchange_alg_string(PreferredKeyExchangeAlg));
		return FALSE;
	}
	return TRUE;
}

BOOL license_read_license_info(rdpLicense* license, wStream* s)
{
	BOOL rc = FALSE;
	UINT32 PreferredKeyExchangeAlg;

	WINPR_ASSERT(license);

	/* ClientRandom (32 bytes) */
	if (!license_check_stream_length(s, 8 + CLIENT_RANDOM_LENGTH, "license info"))
		goto error;

	Stream_Read_UINT32(s, PreferredKeyExchangeAlg); /* PreferredKeyExchangeAlg (4 bytes) */
	if (!license_check_preferred_alg(license, PreferredKeyExchangeAlg, "license info"))
		goto error;
	Stream_Read_UINT32(s, license->PlatformId); /* PlatformId (4 bytes) */

	/* ClientRandom (32 bytes) */
	Stream_Read(s, license->ClientRandom, CLIENT_RANDOM_LENGTH);

	/* Licensing Binary Blob with EncryptedPreMasterSecret: */
	if (!license_read_encrypted_premaster_secret_blob(s, license->EncryptedPremasterSecret,
	                                                  &license->ModulusLength))
		goto error;

	/* Licensing Binary Blob with LicenseInfo: */
	if (!license_read_binary_blob(s, license->LicenseInfo))
		goto error;

	/* Licensing Binary Blob with EncryptedHWID */
	if (!license_read_binary_blob(s, license->EncryptedHardwareId))
		goto error;

	/* MACData */
	if (!license_check_stream_length(s, LICENSING_ENCRYPTION_KEY_LENGTH, "license info::MACData"))
		goto error;
	Stream_Read(s, license->MACData, LICENSING_ENCRYPTION_KEY_LENGTH);

	rc = TRUE;

error:
	return rc;
}

/**
 * Read a LICENSE_REQUEST packet.
 * msdn{cc241914}
 * @param license license module
 * @param s stream
 */

BOOL license_read_license_request_packet(rdpLicense* license, wStream* s)
{
	WINPR_ASSERT(license);

	/* ServerRandom (32 bytes) */
	if (!license_check_stream_length(s, SERVER_RANDOM_LENGTH, "license request"))
		return FALSE;

	Stream_Read(s, license->ServerRandom, SERVER_RANDOM_LENGTH);

	/* ProductInfo */
	if (!license_read_product_info(s, license->ProductInfo))
		return FALSE;

	/* KeyExchangeList */
	if (!license_read_binary_blob(s, license->KeyExchangeList))
		return FALSE;

	/* ServerCertificate */
	if (!license_read_binary_blob(s, license->ServerCertificate))
		return FALSE;

	/* ScopeList */
	if (!license_read_scope_list(s, license->ScopeList))
		return FALSE;

	/* Parse Server Certificate */
	if (!certificate_read_server_certificate(license->certificate, license->ServerCertificate->data,
	                                         license->ServerCertificate->length))
		return FALSE;

	if (!license_generate_keys(license) || !license_generate_hwid(license) ||
	    !license_encrypt_premaster_secret(license))
		return FALSE;

#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "ServerRandom:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->ServerRandom, SERVER_RANDOM_LENGTH);
	license_print_product_info(license->ProductInfo);
	license_print_scope_list(license->ScopeList);
#endif
	return TRUE;
}

BOOL license_write_license_request_packet(const rdpLicense* license, wStream* s)
{
	WINPR_ASSERT(license);

	/* ServerRandom (32 bytes) */
	if (!license_check_stream_capacity(s, SERVER_RANDOM_LENGTH, "license request"))
		return FALSE;
	Stream_Write(s, license->ServerRandom, SERVER_RANDOM_LENGTH);

	/* ProductInfo */
	if (!license_write_product_info(s, license->ProductInfo))
		return FALSE;

	/* KeyExchangeList */
	if (!license_write_binary_blob(s, license->KeyExchangeList))
		return FALSE;

	/* ServerCertificate */
	if (!license_write_binary_blob(s, license->ServerCertificate))
		return FALSE;

	/* ScopeList */
	if (!license_write_scope_list(s, license->ScopeList))
		return FALSE;

	return TRUE;
}

static BOOL license_send_license_request_packet(rdpLicense* license)
{
	wStream* s = license_send_stream_init(license);
	if (!s)
		return FALSE;

	if (!license_write_license_request_packet(license, s))
		goto fail;

	return license_send(license, s, LICENSE_REQUEST);

fail:
	Stream_Release(s);
	return FALSE;
}

/*
 * Read a PLATFORM_CHALLENGE packet.
 * msdn{cc241921}
 * @param license license module
 * @param s stream
 */

BOOL license_read_platform_challenge_packet(rdpLicense* license, wStream* s)
{
	BYTE macData[16] = { 0 };
	UINT32 ConnectFlags = 0;

	WINPR_ASSERT(license);

	DEBUG_LICENSE("Receiving Platform Challenge Packet");

	if (!license_check_stream_length(s, 4, "license platform challenge"))
		return FALSE;

	Stream_Read_UINT32(s, ConnectFlags); /* ConnectFlags, Reserved (4 bytes) */

	/* EncryptedPlatformChallenge */
	license->EncryptedPlatformChallenge->type = BB_ANY_BLOB;
	if (!license_read_binary_blob(s, license->EncryptedPlatformChallenge))
		return FALSE;
	license->EncryptedPlatformChallenge->type = BB_ENCRYPTED_DATA_BLOB;

	/* MACData (16 bytes) */
	if (!license_check_stream_length(s, LICENSING_ENCRYPTION_KEY_LENGTH,
	                                 "license platform challenge::MAC"))
		return FALSE;

	Stream_Read(s, macData, 16);
	if (!license_decrypt_and_check_MAC(license, license->EncryptedPlatformChallenge->data,
	                                   license->EncryptedPlatformChallenge->length,
	                                   license->PlatformChallenge, macData))
		return FALSE;

#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "ConnectFlags: 0x%08" PRIX32 "", ConnectFlags);
	WLog_DBG(TAG, "EncryptedPlatformChallenge:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->EncryptedPlatformChallenge->data,
	              license->EncryptedPlatformChallenge->length);
	WLog_DBG(TAG, "PlatformChallenge:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->PlatformChallenge->data,
	              license->PlatformChallenge->length);
	WLog_DBG(TAG, "MacData:");
	winpr_HexDump(TAG, WLOG_DEBUG, macData, LICENSING_ENCRYPTION_KEY_LENGTH);
#endif
	return TRUE;
}

BOOL license_send_error_alert(rdpLicense* license, UINT32 dwErrorCode, UINT32 dwStateTransition,
                              const LICENSE_BLOB* info)
{
	wStream* s = license_send_stream_init(license);

	if (!s)
		goto fail;

	if (!license_check_stream_capacity(s, 8, "license error alert"))
		goto fail;
	Stream_Write_UINT32(s, dwErrorCode);
	Stream_Write_UINT32(s, dwStateTransition);

	if (!license_write_binary_blob(s, info))
		goto fail;

	return license_send(license, s, ERROR_ALERT);
fail:
	Stream_Release(s);
	return FALSE;
}

BOOL license_send_platform_challenge_packet(rdpLicense* license)
{
	wStream* s = license_send_stream_init(license);

	if (!s)
		goto fail;

	DEBUG_LICENSE("Receiving Platform Challenge Packet");

	if (!license_check_stream_capacity(s, 4, "license platform challenge"))
		goto fail;

	Stream_Zero(s, 4); /* ConnectFlags, Reserved (4 bytes) */

	/* EncryptedPlatformChallenge */
	if (!license_write_binary_blob(s, license->EncryptedPlatformChallenge))
		goto fail;

	/* MACData (16 bytes) */
	if (!license_check_stream_length(s, LICENSING_ENCRYPTION_KEY_LENGTH,
	                                 "license platform challenge::MAC"))
		goto fail;

	Stream_Write(s, license->MACData, LICENSING_ENCRYPTION_KEY_LENGTH);

	return license_send(license, s, PLATFORM_CHALLENGE);
fail:
	Stream_Release(s);
	return FALSE;
}

static BOOL license_read_encrypted_blob(const rdpLicense* license, wStream* s, LICENSE_BLOB* target)
{
	UINT16 wBlobType, wBlobLen;
	BYTE* encryptedData;

	WINPR_ASSERT(license);
	WINPR_ASSERT(target);

	if (!license_check_stream_length(s, 4, "license encrypted blob"))
		return FALSE;

	Stream_Read_UINT16(s, wBlobType);
	if (wBlobType != BB_ENCRYPTED_DATA_BLOB)
	{
		WLog_WARN(
		    TAG,
		    "expecting BB_ENCRYPTED_DATA_BLOB blob, probably a windows 2003 server, continuing...");
	}

	Stream_Read_UINT16(s, wBlobLen);

	encryptedData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, wBlobLen))
	{
		WLog_WARN(TAG,
		          "short license encrypted blob::length, expected %" PRIu16 " bytes, got %" PRIuz,
		          wBlobLen, Stream_GetRemainingLength(s));
		return FALSE;
	}

	return license_rc4_with_licenseKey(license, encryptedData, wBlobLen, target);
}

/**
 * Read a NEW_LICENSE packet.
 * msdn{cc241926}
 * @param license license module
 * @param s stream
 */

BOOL license_read_new_or_upgrade_license_packet(rdpLicense* license, wStream* s)
{
	UINT32 os_major;
	UINT32 os_minor;
	UINT32 cbScope, cbCompanyName, cbProductId, cbLicenseInfo;
	wStream sbuffer;
	wStream* licenseStream = NULL;
	BOOL ret = FALSE;
	BYTE computedMac[16] = { 0 };
	LICENSE_BLOB* calBlob;
	const BYTE* readMac;

	WINPR_ASSERT(license);

	DEBUG_LICENSE("Receiving Server New/Upgrade License Packet");

	calBlob = license_new_binary_blob(BB_DATA_BLOB);
	if (!calBlob)
		return FALSE;

	/* EncryptedLicenseInfo */
	if (!license_read_encrypted_blob(license, s, calBlob))
		goto fail;

	/* compute MAC and check it */
	readMac = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, 16))
	{
		WLog_WARN(TAG, "short license new/upgrade, expected 16 bytes, got %" PRIuz,
		          Stream_GetRemainingLength(s));
		goto fail;
	}

	if (!security_mac_data(license->MacSaltKey, calBlob->data, calBlob->length, computedMac))
		goto fail;

	if (memcmp(computedMac, readMac, sizeof(computedMac)) != 0)
	{
		WLog_ERR(TAG, "new or upgrade license MAC mismatch");
		goto fail;
	}

	licenseStream = Stream_StaticConstInit(&sbuffer, calBlob->data, calBlob->length);
	if (!licenseStream)
		goto fail;

	if (!license_check_stream_length(s, 8, "license new/upgrade::blob::version"))
		goto fail;

	Stream_Read_UINT16(licenseStream, os_minor);
	Stream_Read_UINT16(licenseStream, os_major);

	/* Scope */
	Stream_Read_UINT32(licenseStream, cbScope);
	if (!license_check_stream_length(s, cbScope, "license new/upgrade::blob::scope"))
		goto fail;

#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "Scope:");
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Pointer(licenseStream), cbScope);
#endif
	Stream_Seek(licenseStream, cbScope);

	/* CompanyName */
	if (!license_check_stream_length(s, 4, "license new/upgrade::blob::cbCompanyName"))
		goto fail;

	Stream_Read_UINT32(licenseStream, cbCompanyName);
	if (!license_check_stream_length(s, cbCompanyName, "license new/upgrade::blob::CompanyName"))
		goto fail;

#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "Company name:");
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Pointer(licenseStream), cbCompanyName);
#endif
	Stream_Seek(licenseStream, cbCompanyName);

	/* productId */
	if (!license_check_stream_length(s, 4, "license new/upgrade::blob::cbProductId"))
		goto fail;

	Stream_Read_UINT32(licenseStream, cbProductId);

	if (!license_check_stream_length(s, cbProductId, "license new/upgrade::blob::ProductId"))
		goto fail;

#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "Product id:");
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Pointer(licenseStream), cbProductId);
#endif
	Stream_Seek(licenseStream, cbProductId);

	/* licenseInfo */
	if (!license_check_stream_length(s, 4, "license new/upgrade::blob::cbLicenseInfo"))
		goto fail;

	Stream_Read_UINT32(licenseStream, cbLicenseInfo);
	if (!license_check_stream_length(s, cbLicenseInfo, "license new/upgrade::blob::LicenseInfo"))
		goto fail;

	license->type = LICENSE_TYPE_ISSUED;
	ret = license_set_state(license, LICENSE_STATE_COMPLETED);

	ret = TRUE;
	if (!license->rdp->settings->OldLicenseBehaviour)
		ret = saveCal(license->rdp->settings, Stream_Pointer(licenseStream), cbLicenseInfo,
		              license->rdp->settings->ClientHostname);

fail:
	license_free_binary_blob(calBlob);
	return ret;
}

/**
 * Read an ERROR_ALERT packet.
 * msdn{cc240482}
 * @param license license module
 * @param s stream
 */

BOOL license_read_error_alert_packet(rdpLicense* license, wStream* s)
{
	UINT32 dwErrorCode;
	UINT32 dwStateTransition;

	WINPR_ASSERT(license);
	WINPR_ASSERT(license->rdp);

	if (!license_check_stream_length(s, 8ul, "error alert"))
		return FALSE;

	Stream_Read_UINT32(s, dwErrorCode);       /* dwErrorCode (4 bytes) */
	Stream_Read_UINT32(s, dwStateTransition); /* dwStateTransition (4 bytes) */

	if (!license_read_binary_blob(s, license->ErrorInfo)) /* bbErrorInfo */
		return FALSE;

#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "dwErrorCode: %s, dwStateTransition: %s", error_codes[dwErrorCode],
	         state_transitions[dwStateTransition]);
#endif

	if (dwErrorCode == STATUS_VALID_CLIENT)
	{
		license->type = LICENSE_TYPE_NONE;
		return license_set_state(license, LICENSE_STATE_COMPLETED);
	}

	switch (dwStateTransition)
	{
		case ST_TOTAL_ABORT:
			license_set_state(license, LICENSE_STATE_ABORTED);
			break;
		case ST_NO_TRANSITION:
			license_set_state(license, LICENSE_STATE_COMPLETED);
			break;
		case ST_RESET_PHASE_TO_START:
			license_set_state(license, LICENSE_STATE_CONFIGURED);
			break;
		case ST_RESEND_LAST_MESSAGE:
			break;
		default:
			break;
	}

	return TRUE;
}

/**
 * Write a NEW_LICENSE_REQUEST packet.
 * msdn{cc241918}
 * @param license license module
 * @param s stream
 */

BOOL license_write_new_license_request_packet(const rdpLicense* license, wStream* s)
{
	WINPR_ASSERT(license);

	if (!license_check_stream_capacity(s, 8 + CLIENT_RANDOM_LENGTH, "License Request"))
		return FALSE;

	Stream_Write_UINT32(s,
	                    license->PreferredKeyExchangeAlg); /* PreferredKeyExchangeAlg (4 bytes) */
	Stream_Write_UINT32(s, license->PlatformId);           /* PlatformId (4 bytes) */
	Stream_Write(s, license->ClientRandom, CLIENT_RANDOM_LENGTH); /* ClientRandom (32 bytes) */

	if (/* EncryptedPremasterSecret */
	    !license_write_encrypted_premaster_secret_blob(s, license->EncryptedPremasterSecret,
	                                                   license->ModulusLength) ||
	    /* ClientUserName */
	    !license_write_binary_blob(s, license->ClientUserName) ||
	    /* ClientMachineName */
	    !license_write_binary_blob(s, license->ClientMachineName))
	{
		return FALSE;
	}

#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "PreferredKeyExchangeAlg: 0x%08" PRIX32 "", license->PreferredKeyExchangeAlg);
	WLog_DBG(TAG, "ClientRandom:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->ClientRandom, CLIENT_RANDOM_LENGTH);
	WLog_DBG(TAG, "EncryptedPremasterSecret");
	winpr_HexDump(TAG, WLOG_DEBUG, license->EncryptedPremasterSecret->data,
	              license->EncryptedPremasterSecret->length);
	WLog_DBG(TAG, "ClientUserName (%" PRIu16 "): %s", license->ClientUserName->length,
	         (char*)license->ClientUserName->data);
	WLog_DBG(TAG, "ClientMachineName (%" PRIu16 "): %s", license->ClientMachineName->length,
	         (char*)license->ClientMachineName->data);
#endif
	return TRUE;
}

BOOL license_read_new_license_request_packet(rdpLicense* license, wStream* s)
{
	UINT32 PreferredKeyExchangeAlg;

	WINPR_ASSERT(license);

	if (!license_check_stream_length(s, 8ull + CLIENT_RANDOM_LENGTH, "new license request"))
		return FALSE;

	Stream_Read_UINT32(s, PreferredKeyExchangeAlg); /* PreferredKeyExchangeAlg (4 bytes) */
	if (!license_check_preferred_alg(license, PreferredKeyExchangeAlg, "new license request"))
		return FALSE;

	Stream_Read_UINT32(s, license->PlatformId);                  /* PlatformId (4 bytes) */
	Stream_Read(s, license->ClientRandom, CLIENT_RANDOM_LENGTH); /* ClientRandom (32 bytes) */

	/* EncryptedPremasterSecret */
	if (!license_read_encrypted_premaster_secret_blob(s, license->EncryptedPremasterSecret,
	                                                  &license->ModulusLength))
		return FALSE;

	/* ClientUserName */
	if (!license_read_binary_blob(s, license->ClientUserName))
		return FALSE;
	/* ClientMachineName */
	if (!license_read_binary_blob(s, license->ClientMachineName))
		return FALSE;

	return TRUE;
}

/**
 * Send a NEW_LICENSE_REQUEST packet.
 * msdn{cc241918}
 * @param license license module
 */

BOOL license_answer_license_request(rdpLicense* license)
{
	wStream* s;
	BYTE* license_data = NULL;
	size_t license_size = 0;
	BOOL status;
	char* username;

	WINPR_ASSERT(license);
	WINPR_ASSERT(license->rdp);
	WINPR_ASSERT(license->rdp->settings);

	if (!license->rdp->settings->OldLicenseBehaviour)
		license_data = loadCalFile(license->rdp->settings, license->rdp->settings->ClientHostname,
		                           &license_size);

	if (license_data)
	{
		LICENSE_BLOB* calBlob = NULL;
		BYTE signature[LICENSING_ENCRYPTION_KEY_LENGTH] = { 0 };

		DEBUG_LICENSE("Sending Saved License Packet");

		WINPR_ASSERT(license->EncryptedHardwareId);
		license->EncryptedHardwareId->type = BB_ENCRYPTED_DATA_BLOB;
		if (!license_encrypt_and_MAC(license, license->HardwareId, HWID_LENGTH,
		                             license->EncryptedHardwareId, signature))
		{
			free(license_data);
			return FALSE;
		}

		calBlob = license_new_binary_blob(BB_DATA_BLOB);
		if (!calBlob)
		{
			free(license_data);
			return FALSE;
		}
		calBlob->data = license_data;
		calBlob->length = license_size;

		status = license_send_license_info(license, calBlob, signature);
		license_free_binary_blob(calBlob);

		return status;
	}

	DEBUG_LICENSE("Sending New License Packet");

	s = license_send_stream_init(license);
	if (!s)
		return FALSE;
	if (license->rdp->settings->Username != NULL)
		username = license->rdp->settings->Username;
	else
		username = "username";

	WINPR_ASSERT(license->ClientUserName);
	license->ClientUserName->data = (BYTE*)username;
	license->ClientUserName->length = strlen(username) + 1;

	WINPR_ASSERT(license->ClientMachineName);
	license->ClientMachineName->data = (BYTE*)license->rdp->settings->ClientHostname;
	license->ClientMachineName->length = strlen(license->rdp->settings->ClientHostname) + 1;

	status = license_write_new_license_request_packet(license, s);

	WINPR_ASSERT(license->ClientUserName);
	license->ClientUserName->data = NULL;
	license->ClientUserName->length = 0;

	WINPR_ASSERT(license->ClientMachineName);
	license->ClientMachineName->data = NULL;
	license->ClientMachineName->length = 0;

	if (!status)
	{
		Stream_Release(s);
		return FALSE;
	}

	return license_send(license, s, NEW_LICENSE_REQUEST);
}

/**
 * Send Client Challenge Response Packet.
 * msdn{cc241922}
 * @param license license module
 */

BOOL license_send_platform_challenge_response(rdpLicense* license)
{
	wStream* s = license_send_stream_init(license);
	wStream* challengeRespData;
	int length;
	BYTE* buffer;
	BOOL status;

	WINPR_ASSERT(license);
	WINPR_ASSERT(license->PlatformChallenge);
	WINPR_ASSERT(license->MacSaltKey);
	WINPR_ASSERT(license->EncryptedPlatformChallenge);
	WINPR_ASSERT(license->EncryptedHardwareId);

	DEBUG_LICENSE("Sending Platform Challenge Response Packet");

	license->EncryptedPlatformChallenge->type = BB_DATA_BLOB;

	/* prepare the PLATFORM_CHALLENGE_RESPONSE_DATA */
	challengeRespData = Stream_New(NULL, 8 + license->PlatformChallenge->length);
	if (!challengeRespData)
		return FALSE;
	Stream_Write_UINT16(challengeRespData, PLATFORM_CHALLENGE_RESPONSE_VERSION); /* wVersion */
	Stream_Write_UINT16(challengeRespData, license->ClientType);                 /* wClientType */
	Stream_Write_UINT16(challengeRespData, license->LicenseDetailLevel); /* wLicenseDetailLevel */
	Stream_Write_UINT16(challengeRespData, license->PlatformChallenge->length); /* cbChallenge */
	Stream_Write(challengeRespData, license->PlatformChallenge->data,
	             license->PlatformChallenge->length); /* pbChallenge */
	Stream_SealLength(challengeRespData);

	/* compute MAC of PLATFORM_CHALLENGE_RESPONSE_DATA + HWID */
	length = Stream_Length(challengeRespData) + HWID_LENGTH;
	buffer = (BYTE*)malloc(length);
	if (!buffer)
	{
		Stream_Free(challengeRespData, TRUE);
		return FALSE;
	}

	CopyMemory(buffer, Stream_Buffer(challengeRespData), Stream_Length(challengeRespData));
	CopyMemory(&buffer[Stream_Length(challengeRespData)], license->HardwareId, HWID_LENGTH);
	status = security_mac_data(license->MacSaltKey, buffer, length, license->MACData);
	free(buffer);

	if (!status)
	{
		Stream_Free(challengeRespData, TRUE);
		return FALSE;
	}

	license->EncryptedHardwareId->type = BB_ENCRYPTED_DATA_BLOB;
	if (!license_rc4_with_licenseKey(license, license->HardwareId, HWID_LENGTH,
	                                 license->EncryptedHardwareId))
	{
		Stream_Free(challengeRespData, TRUE);
		return FALSE;
	}

	status = license_rc4_with_licenseKey(license, Stream_Buffer(challengeRespData),
	                                     Stream_Length(challengeRespData),
	                                     license->EncryptedPlatformChallengeResponse);
	Stream_Free(challengeRespData, TRUE);
	if (!status)
		return FALSE;

#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "LicensingEncryptionKey:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->LicensingEncryptionKey, 16);
	WLog_DBG(TAG, "HardwareId:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->HardwareId, HWID_LENGTH);
	WLog_DBG(TAG, "EncryptedHardwareId:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->EncryptedHardwareId->data, HWID_LENGTH);
#endif
	if (license_write_client_platform_challenge_response(license, s))
		return license_send(license, s, PLATFORM_CHALLENGE_RESPONSE);

	Stream_Release(s);
	return FALSE;
}

BOOL license_read_platform_challenge_response(rdpLicense* license, wStream* s)
{
	UINT16 wVersion;
	UINT16 cbChallenge;
	const BYTE* pbChallenge;

	WINPR_ASSERT(license);
	WINPR_ASSERT(license->PlatformChallenge);
	WINPR_ASSERT(license->MacSaltKey);
	WINPR_ASSERT(license->EncryptedPlatformChallenge);
	WINPR_ASSERT(license->EncryptedHardwareId);

	DEBUG_LICENSE("Receiving Platform Challenge Response Packet");

	if (!license_check_stream_length(s, 8, "PLATFORM_CHALLENGE_RESPONSE_DATA"))
		return FALSE;

	Stream_Read_UINT16(s, wVersion);
	if (wVersion != PLATFORM_CHALLENGE_RESPONSE_VERSION)
	{
		WLog_WARN(TAG,
		          "Invalid PLATFORM_CHALLENGE_RESPONSE_DATA::wVersion 0x%04" PRIx16
		          ", expected 0x04" PRIx16,
		          wVersion, PLATFORM_CHALLENGE_RESPONSE_VERSION);
		return FALSE;
	}
	Stream_Read_UINT16(s, license->ClientType);
	Stream_Read_UINT16(s, license->LicenseDetailLevel);
	Stream_Read_UINT16(s, cbChallenge);

	if (!license_check_stream_length(s, cbChallenge,
	                                 "PLATFORM_CHALLENGE_RESPONSE_DATA::pbChallenge"))
		return FALSE;

	pbChallenge = Stream_Pointer(s);
	if (!license_read_binary_blob_data(license->EncryptedPlatformChallengeResponse, BB_DATA_BLOB,
	                                   pbChallenge, cbChallenge))
		return FALSE;
	return Stream_SafeSeek(s, cbChallenge);
}

BOOL license_write_client_platform_challenge_response(rdpLicense* license, wStream* s)
{
	WINPR_ASSERT(license);

	if (!license_write_binary_blob(s, license->EncryptedPlatformChallengeResponse))
		return FALSE;
	if (!license_write_binary_blob(s, license->EncryptedHardwareId))
		return FALSE;
	if (!license_check_stream_capacity(s, sizeof(license->MACData),
	                                   "CLIENT_PLATFORM_CHALLENGE_RESPONSE::MACData"))
		return FALSE;
	Stream_Write(s, license->MACData, sizeof(license->MACData));
	return TRUE;
}

BOOL license_read_client_platform_challenge_response(rdpLicense* license, wStream* s)
{
	WINPR_ASSERT(license);

	if (!license_read_binary_blob(s, license->EncryptedPlatformChallengeResponse))
		return FALSE;
	if (!license_read_binary_blob(s, license->EncryptedHardwareId))
		return FALSE;
	if (!license_check_stream_length(s, sizeof(license->MACData),
	                                 "CLIENT_PLATFORM_CHALLENGE_RESPONSE::MACData"))
		return FALSE;
	Stream_Read(s, license->MACData, sizeof(license->MACData));
	return TRUE;
}

/**
 * Send Server License Error - Valid Client Packet.
 * msdn{cc241922}
 *
 * @param rdp A pointer to the context to use
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL license_send_valid_client_error_packet(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);
	rdpLicense* license = rdp->license;
	WINPR_ASSERT(license);

	license->state = LICENSE_STATE_COMPLETED;
	license->type = LICENSE_TYPE_NONE;
	return license_send_error_alert(license, STATUS_VALID_CLIENT, ST_NO_TRANSITION,
	                                license->ErrorInfo);
}

/**
 * Instantiate new license module.
 * @param rdp RDP module
 * @return new license module
 */

rdpLicense* license_new(rdpRdp* rdp)
{
	rdpLicense* license;
	WINPR_ASSERT(rdp);

	license = (rdpLicense*)calloc(1, sizeof(rdpLicense));
	if (!license)
		return NULL;

	license->PlatformId = PLATFORMID;
	license->ClientType = OTHER_PLATFORM_CHALLENGE_TYPE;
	license->LicenseDetailLevel = LICENSE_DETAIL_DETAIL;
	license->PreferredKeyExchangeAlg = KEY_EXCHANGE_ALG_RSA;
	license->rdp = rdp;

	license_set_state(license, LICENSE_STATE_INITIAL);
	if (!(license->certificate = certificate_new()))
		goto out_error;
	if (!(license->ProductInfo = license_new_product_info()))
		goto out_error;
	if (!(license->ErrorInfo = license_new_binary_blob(BB_ERROR_BLOB)))
		goto out_error;
	if (!(license->LicenseInfo = license_new_binary_blob(BB_DATA_BLOB)))
		goto out_error;
	if (!(license->KeyExchangeList = license_new_binary_blob(BB_KEY_EXCHG_ALG_BLOB)))
		goto out_error;
	if (!(license->ServerCertificate = license_new_binary_blob(BB_CERTIFICATE_BLOB)))
		goto out_error;
	if (!(license->ClientUserName = license_new_binary_blob(BB_CLIENT_USER_NAME_BLOB)))
		goto out_error;
	if (!(license->ClientMachineName = license_new_binary_blob(BB_CLIENT_MACHINE_NAME_BLOB)))
		goto out_error;
	if (!(license->PlatformChallenge = license_new_binary_blob(BB_ANY_BLOB)))
		goto out_error;
	if (!(license->EncryptedPlatformChallenge = license_new_binary_blob(BB_ANY_BLOB)))
		goto out_error;
	if (!(license->EncryptedPlatformChallengeResponse =
	          license_new_binary_blob(BB_ENCRYPTED_DATA_BLOB)))
		goto out_error;
	if (!(license->EncryptedPremasterSecret = license_new_binary_blob(BB_ANY_BLOB)))
		goto out_error;
	if (!(license->EncryptedHardwareId = license_new_binary_blob(BB_ENCRYPTED_DATA_BLOB)))
		goto out_error;
	if (!(license->EncryptedLicenseInfo = license_new_binary_blob(BB_ENCRYPTED_DATA_BLOB)))
		goto out_error;
	if (!(license->ScopeList = license_new_scope_list()))
		goto out_error;

	license_generate_randoms(license);

	return license;

out_error:
	license_free(license);
	return NULL;
}

/**
 * Free license module.
 * @param license license module to be freed
 */

void license_free(rdpLicense* license)
{
	if (license)
	{
		free(license->Modulus);
		certificate_free(license->certificate);
		license_free_product_info(license->ProductInfo);
		license_free_binary_blob(license->ErrorInfo);
		license_free_binary_blob(license->LicenseInfo);
		license_free_binary_blob(license->KeyExchangeList);
		license_free_binary_blob(license->ServerCertificate);
		license_free_binary_blob(license->ClientUserName);
		license_free_binary_blob(license->ClientMachineName);
		license_free_binary_blob(license->PlatformChallenge);
		license_free_binary_blob(license->EncryptedPlatformChallenge);
		license_free_binary_blob(license->EncryptedPlatformChallengeResponse);
		license_free_binary_blob(license->EncryptedPremasterSecret);
		license_free_binary_blob(license->EncryptedHardwareId);
		license_free_binary_blob(license->EncryptedLicenseInfo);
		license_free_scope_list(license->ScopeList);
		free(license);
	}
}

LICENSE_STATE license_get_state(const rdpLicense* license)
{
	WINPR_ASSERT(license);
	return license->state;
}

LICENSE_TYPE license_get_type(const rdpLicense* license)
{
	WINPR_ASSERT(license);
	return license->type;
}

BOOL license_set_state(rdpLicense* license, LICENSE_STATE state)
{
	WINPR_ASSERT(license);
	license->state = state;
	switch (state)
	{
		case LICENSE_STATE_COMPLETED:
			break;
		case LICENSE_STATE_ABORTED:
		default:
			license->type = LICENSE_TYPE_INVALID;
			break;
	}

	return TRUE;
}

const char* license_get_state_string(LICENSE_STATE state)
{
	switch (state)
	{
		case LICENSE_STATE_INITIAL:
			return "LICENSE_STATE_INITIAL";
		case LICENSE_STATE_CONFIGURED:
			return "LICENSE_STATE_CONFIGURED";
		case LICENSE_STATE_REQUEST:
			return "LICENSE_STATE_REQUEST";
		case LICENSE_STATE_NEW_REQUEST:
			return "LICENSE_STATE_NEW_REQUEST";
		case LICENSE_STATE_PLATFORM_CHALLENGE:
			return "LICENSE_STATE_PLATFORM_CHALLENGE";
		case LICENSE_STATE_PLATFORM_CHALLENGE_RESPONSE:
			return "LICENSE_STATE_PLATFORM_CHALLENGE_RESPONSE";
		case LICENSE_STATE_COMPLETED:
			return "LICENSE_STATE_COMPLETED";
		case LICENSE_STATE_ABORTED:
			return "LICENSE_STATE_ABORTED";
		default:
			return "LICENSE_STATE_UNKNOWN";
	}
}

BOOL license_server_send_request(rdpLicense* license)
{
	if (!license_ensure_state(license, LICENSE_STATE_CONFIGURED, LICENSE_REQUEST))
		return FALSE;
	if (!license_send_license_request_packet(license))
		return FALSE;
	return license_set_state(license, LICENSE_STATE_REQUEST);
}

BOOL license_server_configure(rdpLicense* license)
{

	size_t len;
	wStream* s;
	UINT32 algs[] = { KEY_EXCHANGE_ALG_RSA };
	UINT32 x;
	const rdpSettings* settings;
	UINT32 ProductVersion, issuerCount;
	const char* CompanyName;
	const char* ProductName;
	const char* const* issuers;

	WINPR_ASSERT(license);
	WINPR_ASSERT(license->rdp);

	settings = license->rdp->settings;

	CompanyName = freerdp_settings_get_string(settings, FreeRDP_ServerLicenseCompanyName);

	ProductName = freerdp_settings_get_string(settings, FreeRDP_ServerLicenseProductName);
	ProductVersion = freerdp_settings_get_uint32(settings, FreeRDP_ServerLicenseProductVersion);
	issuerCount = freerdp_settings_get_uint32(settings, FreeRDP_ServerLicenseProductIssuersCount);
	issuers = (const char* const*)freerdp_settings_get_pointer(settings,
	                                                           FreeRDP_ServerLicenseProductIssuers);

	WINPR_ASSERT(CompanyName);
	WINPR_ASSERT(ProductName);
	WINPR_ASSERT(ProductVersion > 0);
	WINPR_ASSERT(issuers || (issuerCount == 0));

	if (!license_ensure_state(license, LICENSE_STATE_INITIAL, LICENSE_REQUEST))
		return FALSE;

	license->ProductInfo->dwVersion = ProductVersion;
	license->ProductInfo->pbCompanyName = (BYTE*)ConvertUtf8ToWCharAlloc(CompanyName, &len);
	if (!license->ProductInfo->pbCompanyName || (len > UINT32_MAX / sizeof(WCHAR)))
		return FALSE;
	license->ProductInfo->cbCompanyName = (UINT32)len * sizeof(WCHAR);

	license->ProductInfo->pbProductId = (BYTE*)ConvertUtf8ToWCharAlloc(ProductName, &len);
	if (!license->ProductInfo->pbProductId || (len > UINT32_MAX / sizeof(WCHAR)))
		return FALSE;
	license->ProductInfo->cbProductId = (UINT32)len * sizeof(WCHAR);

	if (!license_read_binary_blob_data(license->KeyExchangeList, BB_KEY_EXCHG_ALG_BLOB, algs,
	                                   sizeof(algs)))
		return FALSE;

	if (!certificate_read_server_certificate(license->certificate, settings->ServerCertificate,
	                                         settings->ServerCertificateLength))
		return FALSE;

	s = Stream_New(NULL, 1024);
	if (!s)
		return FALSE;
	else
	{
		BOOL r =
		    certificate_write_server_certificate(license->certificate, CERT_CHAIN_VERSION_2, s);
		if (r)
			r = license_read_binary_blob_data(license->ServerCertificate, BB_CERTIFICATE_BLOB,
			                                  Stream_Buffer(s), Stream_GetPosition(s));

		Stream_Free(s, TRUE);
		if (!r)
			return FALSE;
	}

	if (!license_scope_list_resize(license->ScopeList, issuerCount))
		return FALSE;
	for (x = 0; x < issuerCount; x++)
	{
		LICENSE_BLOB* blob = license->ScopeList->array[x];
		const char* name = issuers[x];
		const size_t length = strnlen(name, UINT16_MAX) + 1;
		if ((length == 0) || (length > UINT16_MAX))
		{
			WLog_WARN(TAG,
			          "%s: Invalid issuer at position %" PRIuz ": length 0 < %" PRIuz " <= %" PRIu16
			          " ['%s']",
			          x, length, UINT16_MAX, name);
			return FALSE;
		}
		if (!license_read_binary_blob_data(blob, BB_SCOPE_BLOB, name, length))
			return FALSE;
	}

	return license_set_state(license, LICENSE_STATE_CONFIGURED);
}

rdpLicense* license_get(rdpContext* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->rdp);
	return context->rdp->license;
}
