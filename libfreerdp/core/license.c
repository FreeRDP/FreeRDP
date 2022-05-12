/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Licensing
 *
 * Copyright 2011-2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
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

#include <freerdp/config.h>

#include <winpr/crt.h>
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
static BOOL license_read_binary_blob(wStream* s, LICENSE_BLOB* blob);
static BOOL license_write_binary_blob(wStream* s, const LICENSE_BLOB* blob);

static SCOPE_LIST* license_new_scope_list(void);
static void license_free_scope_list(SCOPE_LIST* scopeList);
static BOOL license_read_scope_list(wStream* s, SCOPE_LIST* scopeList);

static BOOL license_read_license_request_packet(rdpLicense* license, wStream* s);
static BOOL license_read_platform_challenge_packet(rdpLicense* license, wStream* s);
static BOOL license_read_new_or_upgrade_license_packet(rdpLicense* license, wStream* s);
static BOOL license_read_error_alert_packet(rdpLicense* license, wStream* s);

static BOOL license_write_new_license_request_packet(rdpLicense* license, wStream* s);
static BOOL license_answer_license_request(rdpLicense* license);

static BOOL license_write_platform_challenge_response_packet(rdpLicense* license, wStream* s,
                                                             const BYTE* mac_data);
static BOOL license_send_platform_challenge_response_packet(rdpLicense* license);
static BOOL license_send_client_info(rdpLicense* license, const LICENSE_BLOB* calBlob,
                                     BYTE* signature);

#define PLATFORMID (CLIENT_OS_ID_WINNT_POST_52 | CLIENT_IMAGE_ID_MICROSOFT)

#ifdef WITH_DEBUG_LICENSE

static const char* const LICENSE_MESSAGE_STRINGS[] = { "",
	                                                   "License Request",
	                                                   "Platform Challenge",
	                                                   "New License",
	                                                   "Upgrade License",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "License Info",
	                                                   "New License Request",
	                                                   "",
	                                                   "Platform Challenge Response",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "",
	                                                   "Error Alert" };

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
	ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)productInfo->pbCompanyName,
	                   productInfo->cbCompanyName / 2, &CompanyName, 0, NULL, NULL);
	ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)productInfo->pbProductId, productInfo->cbProductId / 2,
	                   &ProductId, 0, NULL, NULL);
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
	const LICENSE_BLOB* scope;
	WLog_INFO(TAG, "ScopeList (%" PRIu32 "):", scopeList->count);

	for (index = 0; index < scopeList->count; index++)
	{
		scope = &scopeList->array[index];
		WLog_INFO(TAG, "\t%s", (const char*)scope->data);
	}
}
#endif

static const char licenseStore[] = "licenses";

static BOOL computeCalHash(const char* hostname, char* hashStr)
{
	WINPR_DIGEST_CTX* sha1 = NULL;
	BOOL ret = FALSE;
	BYTE hash[20];
	size_t i;

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

static BOOL saveCal(rdpSettings* settings, const BYTE* data, size_t length, const char* hostname)
{
	char hash[41];
	FILE* fp;
	char* licenseStorePath = NULL;
	char filename[MAX_PATH], filenameNew[MAX_PATH];
	char *filepath = NULL, *filepathNew = NULL;
	WCHAR* wFilepathNew = NULL;
	WCHAR* wFilepath = NULL;
	size_t written;
	BOOL ret = FALSE;

	if (!winpr_PathFileExists(settings->ConfigPath))
	{
		if (!winpr_PathMakePath(settings->ConfigPath, 0))
		{
			WLog_ERR(TAG, "error creating directory '%s'", settings->ConfigPath);
			goto out;
		}
		WLog_INFO(TAG, "creating directory %s", settings->ConfigPath);
	}

	if (!(licenseStorePath = GetCombinedPath(settings->ConfigPath, licenseStore)))
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
	if (ConvertToUnicode(CP_UTF8, 0, filepathNew, -1, &wFilepathNew, 0) <= 0)
		goto out;
	if (ConvertToUnicode(CP_UTF8, 0, filepath, -1, &wFilepath, 0) <= 0)
		goto out;

	fp = winpr_fopen(filepathNew, "wb");
	if (!fp)
		goto out;

	written = fwrite(data, length, 1, fp);
	fclose(fp);

	if (written != 1)
	{
		DeleteFileW(wFilepathNew);
		goto out;
	}

	ret = MoveFileExW(wFilepathNew, wFilepath, MOVEFILE_REPLACE_EXISTING);

out:
	free(wFilepathNew);
	free(filepathNew);
	free(wFilepath);
	free(filepath);
	free(licenseStorePath);
	return ret;
}

static BYTE* loadCalFile(rdpSettings* settings, const char* hostname, size_t* dataLen)
{
	char *licenseStorePath = NULL, *calPath = NULL;
	char calFilename[MAX_PATH];
	char hash[41];
	INT64 length;
	int status;
	FILE* fp;
	BYTE* ret = NULL;

	if (!computeCalHash(hostname, hash))
	{
		WLog_ERR(TAG, "loadCalFile: unable to compute hostname hash");
		return NULL;
	}

	sprintf_s(calFilename, sizeof(calFilename) - 1, "%s.cal", hash);

	if (!(licenseStorePath = GetCombinedPath(settings->ConfigPath, licenseStore)))
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
 * Read a licensing preamble.\n
 * @msdn{cc240480}
 * @param s stream
 * @param bMsgType license message type
 * @param flags message flags
 * @param wMsgSize message size
 * @return if the operation completed successfully
 */

static BOOL license_read_preamble(wStream* s, BYTE* bMsgType, BYTE* flags, UINT16* wMsgSize)
{
	/* preamble (4 bytes) */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT8(s, *bMsgType);  /* bMsgType (1 byte) */
	Stream_Read_UINT8(s, *flags);     /* flags (1 byte) */
	Stream_Read_UINT16(s, *wMsgSize); /* wMsgSize (2 bytes) */
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
 * Initialize a license packet stream.\n
 * @param license license module
 * @return stream
 */

wStream* license_send_stream_init(rdpLicense* license)
{
	wStream* s;
	BOOL do_crypt = license->rdp->do_crypt;

	license->rdp->sec_flags = SEC_LICENSE_PKT;

	/**
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
 * Send an RDP licensing packet.\n
 * @msdn{cc240479}
 * @param license license module
 * @param s stream
 */

static BOOL license_send(rdpLicense* license, wStream* s, BYTE type)
{
	size_t length;
	BYTE flags;
	UINT16 wMsgSize;
	rdpRdp* rdp = license->rdp;
	BOOL ret;

	DEBUG_LICENSE("Sending %s Packet", LICENSE_MESSAGE_STRINGS[type & 0x1F]);
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
	WLog_DBG(TAG, "Sending %s Packet, length %" PRIu16 "", LICENSE_MESSAGE_STRINGS[type & 0x1F],
	         wMsgSize);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Pointer(s) - LICENSE_PREAMBLE_LENGTH, wMsgSize);
#endif
	Stream_SetPosition(s, length);
	ret = rdp_send(rdp, s, MCS_GLOBAL_CHANNEL_ID);
	rdp->sec_flags = 0;
	return ret;
}

/**
 * Receive an RDP licensing packet.\n
 * @msdn{cc240479}
 * @param license license module
 * @param s stream
 * @return if the operation completed successfully
 */

int license_recv(rdpLicense* license, wStream* s)
{
	BYTE flags;
	BYTE bMsgType;
	UINT16 wMsgSize;
	UINT16 length;
	UINT16 channelId;
	UINT16 securityFlags = 0;

	if (!rdp_read_header(license->rdp, s, &length, &channelId))
		return -1;

	if (!rdp_read_security_header(s, &securityFlags, &length))
		return -1;

	if (securityFlags & SEC_ENCRYPT)
	{
		if (!rdp_decrypt(license->rdp, s, &length, securityFlags))
		{
			WLog_ERR(TAG, "rdp_decrypt failed");
			return -1;
		}
	}

	if (!(securityFlags & SEC_LICENSE_PKT))
	{
		int status;

		if (!(securityFlags & SEC_ENCRYPT))
			Stream_Rewind(s, RDP_SECURITY_HEADER_LENGTH);

		status = rdp_recv_out_of_sequence_pdu(license->rdp, s);
		if (status < 0)
		{
			WLog_ERR(TAG, "unexpected license packet.");
			return status;
		}

		return 0;
	}

	if (!license_read_preamble(s, &bMsgType, &flags, &wMsgSize)) /* preamble (4 bytes) */
		return -1;

	DEBUG_LICENSE("Receiving %s Packet", LICENSE_MESSAGE_STRINGS[bMsgType & 0x1F]);

	switch (bMsgType)
	{
		case LICENSE_REQUEST:
			if (!license_read_license_request_packet(license, s))
				return -1;

			if (!license_answer_license_request(license))
				return -1;
			break;

		case PLATFORM_CHALLENGE:
			if (!license_read_platform_challenge_packet(license, s))
				return -1;

			if (!license_send_platform_challenge_response_packet(license))
				return -1;
			break;

		case NEW_LICENSE:
		case UPGRADE_LICENSE:
			if (!license_read_new_or_upgrade_license_packet(license, s))
				return -1;
			break;

		case ERROR_ALERT:
			if (!license_read_error_alert_packet(license, s))
				return -1;
			break;

		default:
			WLog_ERR(TAG, "invalid bMsgType:%" PRIu8 "", bMsgType);
			return -1;
	}

	if (!tpkt_ensure_stream_consumed(s, length))
		return -1;
	return 0;
}

void license_generate_randoms(rdpLicense* license)
{
#ifdef LICENSE_NULL_CLIENT_RANDOM
	ZeroMemory(license->ClientRandom, CLIENT_RANDOM_LENGTH); /* ClientRandom */
#else
	winpr_RAND(license->ClientRandom, CLIENT_RANDOM_LENGTH);       /* ClientRandom */
#endif

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
 * Generate Unique Hardware Identifier (CLIENT_HARDWARE_ID).\n
 * @param license license module
 */

BOOL license_generate_hwid(rdpLicense* license)
{
	const BYTE* hashTarget;
	size_t targetLen;
	BYTE macAddress[6];

	ZeroMemory(license->HardwareId, HWID_LENGTH);

	if (license->rdp->settings->OldLicenseBehaviour)
	{
		ZeroMemory(macAddress, sizeof(macAddress));
		hashTarget = macAddress;
		targetLen = sizeof(macAddress);
	}
	else
	{
		wStream buffer;
		wStream* s;
		const char* hostname = license->rdp->settings->ClientHostname;
		s = Stream_StaticInit(&buffer, license->HardwareId, 4);
		Stream_Write_UINT32(s, PLATFORMID);
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
	rdpSettings* settings = license->rdp->settings;

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

	if (!license_get_server_rsa_public_key(license))
		return FALSE;

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
 * @param mac the signature buffer (16 bytes)
 * @return if the operation completed successfully
 */
static BOOL license_decrypt_and_check_MAC(rdpLicense* license, const BYTE* input, size_t len,
                                          LICENSE_BLOB* target, const BYTE* packetMac)
{
	BYTE macData[16];

	return license_rc4_with_licenseKey(license, input, len, target) &&
	       security_mac_data(license->MacSaltKey, target->data, len, macData) &&
	       (memcmp(packetMac, macData, sizeof(macData)) == 0);
}

/**
 * Read Product Information (PRODUCT_INFO).\n
 * @msdn{cc241915}
 * @param s stream
 * @param productInfo product information
 */

BOOL license_read_product_info(wStream* s, LICENSE_PRODUCT_INFO* productInfo)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, productInfo->dwVersion);     /* dwVersion (4 bytes) */
	Stream_Read_UINT32(s, productInfo->cbCompanyName); /* cbCompanyName (4 bytes) */

	/* Name must be >0, but there is no upper limit defined, use UINT32_MAX */
	if ((productInfo->cbCompanyName < 2) || (productInfo->cbCompanyName % 2 != 0))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, productInfo->cbCompanyName))
		return FALSE;

	productInfo->pbProductId = NULL;
	productInfo->pbCompanyName = (BYTE*)malloc(productInfo->cbCompanyName);
	if (!productInfo->pbCompanyName)
		return FALSE;
	Stream_Read(s, productInfo->pbCompanyName, productInfo->cbCompanyName);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		goto out_fail;

	Stream_Read_UINT32(s, productInfo->cbProductId); /* cbProductId (4 bytes) */

	if ((productInfo->cbProductId < 2) || (productInfo->cbProductId % 2 != 0))
		goto out_fail;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, productInfo->cbProductId))
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

/**
 * Allocate New Product Information (LICENSE_PRODUCT_INFO).\n
 * @msdn{cc241915}
 * @return new product information
 */

LICENSE_PRODUCT_INFO* license_new_product_info()
{
	LICENSE_PRODUCT_INFO* productInfo;
	productInfo = (LICENSE_PRODUCT_INFO*)malloc(sizeof(LICENSE_PRODUCT_INFO));
	if (!productInfo)
		return NULL;
	productInfo->dwVersion = 0;
	productInfo->cbCompanyName = 0;
	productInfo->pbCompanyName = NULL;
	productInfo->cbProductId = 0;
	productInfo->pbProductId = NULL;
	return productInfo;
}

/**
 * Free Product Information (LICENSE_PRODUCT_INFO).\n
 * @msdn{cc241915}
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

/**
 * Read License Binary Blob (LICENSE_BINARY_BLOB).\n
 * @msdn{cc240481}
 * @param s stream
 * @param blob license binary blob
 */

BOOL license_read_binary_blob(wStream* s, LICENSE_BLOB* blob)
{
	UINT16 wBlobType;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, wBlobType);    /* wBlobType (2 bytes) */
	Stream_Read_UINT16(s, blob->length); /* wBlobLen (2 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, blob->length))
		return FALSE;

	/*
	 * Server can choose to not send data by setting length to 0.
	 * If so, it may not bother to set the type, so shortcut the warning
	 */
	if ((blob->type != BB_ANY_BLOB) && (blob->length == 0))
		return TRUE;

	if ((blob->type != wBlobType) && (blob->type != BB_ANY_BLOB))
	{
		WLog_ERR(TAG,
		         "license binary blob type (0x%" PRIx16 ") does not match expected type (0x%" PRIx16
		         ").",
		         wBlobType, blob->type);
	}

	blob->type = wBlobType;
	blob->data = (BYTE*)malloc(blob->length);
	if (!blob->data)
		return FALSE;
	Stream_Read(s, blob->data, blob->length); /* blobData */
	return TRUE;
}

/**
 * Write License Binary Blob (LICENSE_BINARY_BLOB).\n
 * @msdn{cc240481}
 * @param s stream
 * @param blob license binary blob
 */

BOOL license_write_binary_blob(wStream* s, const LICENSE_BLOB* blob)
{
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
	UINT32 length;
	length = ModulusLength + 8;

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

/**
 * Allocate New License Binary Blob (LICENSE_BINARY_BLOB).\n
 * @msdn{cc240481}
 * @return new license binary blob
 */

LICENSE_BLOB* license_new_binary_blob(UINT16 type)
{
	LICENSE_BLOB* blob;
	blob = (LICENSE_BLOB*)calloc(1, sizeof(LICENSE_BLOB));
	if (blob)
		blob->type = type;
	return blob;
}

/**
 * Free License Binary Blob (LICENSE_BINARY_BLOB).\n
 * @msdn{cc240481}
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
 * Read License Scope List (SCOPE_LIST).\n
 * @msdn{cc241916}
 * @param s stream
 * @param scopeList scope list
 */

BOOL license_read_scope_list(wStream* s, SCOPE_LIST* scopeList)
{
	UINT32 i;
	UINT32 scopeCount;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, scopeCount); /* ScopeCount (4 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s,
	                                      4ull * scopeCount)) /* every blob is at least 4 bytes */
		return FALSE;

	scopeList->count = scopeCount;
	scopeList->array = (LICENSE_BLOB*)calloc(scopeCount, sizeof(LICENSE_BLOB));
	if (!scopeList->array)
		return FALSE;

	/* ScopeArray */
	for (i = 0; i < scopeCount; i++)
	{
		scopeList->array[i].type = BB_SCOPE_BLOB;

		if (!license_read_binary_blob(s, &scopeList->array[i]))
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
	return (SCOPE_LIST*)calloc(1, sizeof(SCOPE_LIST));
}

/**
 * Free License Scope List (SCOPE_LIST).\n
 * @msdn{cc241916}
 * @param scopeList scope list
 */

void license_free_scope_list(SCOPE_LIST* scopeList)
{
	UINT32 i;

	if (!scopeList)
		return;

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

	free(scopeList->array);
	free(scopeList);
}

BOOL license_send_client_info(rdpLicense* license, const LICENSE_BLOB* calBlob, BYTE* signature)
{
	wStream* s;

	/* Client License Information: */
	UINT32 PlatformId = PLATFORMID;
	UINT32 PreferredKeyExchangeAlg = KEY_EXCHANGE_ALG_RSA;

	s = license_send_stream_init(license);
	if (!s)
		return FALSE;

	Stream_Write_UINT32(s, PreferredKeyExchangeAlg); /* PreferredKeyExchangeAlg (4 bytes) */
	Stream_Write_UINT32(s, PlatformId);              /* PlatformId (4 bytes) */

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
	Stream_Write(s, signature, LICENSING_ENCRYPTION_KEY_LENGTH);

	return license_send(license, s, LICENSE_INFO);

error:
	Stream_Release(s);
	return FALSE;
}

/**
 * Read a LICENSE_REQUEST packet.\n
 * @msdn{cc241914}
 * @param license license module
 * @param s stream
 */

BOOL license_read_license_request_packet(rdpLicense* license, wStream* s)
{
	/* ServerRandom (32 bytes) */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 32))
		return FALSE;

	Stream_Read(s, license->ServerRandom, 32);

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
	winpr_HexDump(TAG, WLOG_DEBUG, license->ServerRandom, 32);
	license_print_product_info(license->ProductInfo);
	license_print_scope_list(license->ScopeList);
#endif
	return TRUE;
}

/*
 * Read a PLATFORM_CHALLENGE packet.\n
 * @msdn{cc241921}
 * @param license license module
 * @param s stream
 */

BOOL license_read_platform_challenge_packet(rdpLicense* license, wStream* s)
{
	BYTE macData[16];
	UINT32 ConnectFlags = 0;

	DEBUG_LICENSE("Receiving Platform Challenge Packet");

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, ConnectFlags); /* ConnectFlags, Reserved (4 bytes) */

	/* EncryptedPlatformChallenge */
	license->EncryptedPlatformChallenge->type = BB_ANY_BLOB;
	if (!license_read_binary_blob(s, license->EncryptedPlatformChallenge))
		return FALSE;
	license->EncryptedPlatformChallenge->type = BB_ENCRYPTED_DATA_BLOB;

	/* MACData (16 bytes) */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
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
	winpr_HexDump(TAG, WLOG_DEBUG, macData, 16);
#endif
	return TRUE;
}

static BOOL license_read_encrypted_blob(const rdpLicense* license, wStream* s, LICENSE_BLOB* target)
{
	UINT16 wBlobType, wBlobLen;
	BYTE* encryptedData;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, wBlobType);
	if (wBlobType != BB_ENCRYPTED_DATA_BLOB)
	{
		WLog_DBG(
		    TAG,
		    "expecting BB_ENCRYPTED_DATA_BLOB blob, probably a windows 2003 server, continuing...");
	}

	Stream_Read_UINT16(s, wBlobLen);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, wBlobLen))
		return FALSE;

	encryptedData = Stream_Pointer(s);
	Stream_Seek(s, wBlobLen);
	return license_rc4_with_licenseKey(license, encryptedData, wBlobLen, target);
}

/**
 * Read a NEW_LICENSE packet.\n
 * @msdn{cc241926}
 * @param license license module
 * @param s stream
 */

BOOL license_read_new_or_upgrade_license_packet(rdpLicense* license, wStream* s)
{
	UINT32 os_major;
	UINT32 os_minor;
	UINT32 cbScope, cbCompanyName, cbProductId, cbLicenseInfo;
	wStream* licenseStream = NULL;
	BOOL ret = FALSE;
	BYTE computedMac[16];
	LICENSE_BLOB* calBlob;

	DEBUG_LICENSE("Receiving Server New/Upgrade License Packet");

	calBlob = license_new_binary_blob(BB_DATA_BLOB);
	if (!calBlob)
		return FALSE;

	/* EncryptedLicenseInfo */
	if (!license_read_encrypted_blob(license, s, calBlob))
		goto out_free_blob;

	/* compute MAC and check it */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
		goto out_free_blob;

	if (!security_mac_data(license->MacSaltKey, calBlob->data, calBlob->length, computedMac))
		goto out_free_blob;

	if (memcmp(computedMac, Stream_Pointer(s), sizeof(computedMac)) != 0)
	{
		WLog_ERR(TAG, "new or upgrade license MAC mismatch");
		goto out_free_blob;
	}

	if (!Stream_SafeSeek(s, 16))
		goto out_free_blob;

	licenseStream = Stream_New(calBlob->data, calBlob->length);
	if (!licenseStream)
		goto out_free_blob;

	if (!Stream_CheckAndLogRequiredLength(TAG, licenseStream, 8))
		goto out_free_stream;

	Stream_Read_UINT16(licenseStream, os_minor);
	Stream_Read_UINT16(licenseStream, os_major);

	/* Scope */
	Stream_Read_UINT32(licenseStream, cbScope);
	if (!Stream_CheckAndLogRequiredLength(TAG, licenseStream, cbScope))
		goto out_free_stream;
#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "Scope:");
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Pointer(licenseStream), cbScope);
#endif
	Stream_Seek(licenseStream, cbScope);

	/* CompanyName */
	if (!Stream_CheckAndLogRequiredLength(TAG, licenseStream, 4))
		goto out_free_stream;
	Stream_Read_UINT32(licenseStream, cbCompanyName);
	if (!Stream_CheckAndLogRequiredLength(TAG, licenseStream, cbCompanyName))
		goto out_free_stream;
#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "Company name:");
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Pointer(licenseStream), cbCompanyName);
#endif
	Stream_Seek(licenseStream, cbCompanyName);

	/* productId */
	if (!Stream_CheckAndLogRequiredLength(TAG, licenseStream, 4))
		goto out_free_stream;
	Stream_Read_UINT32(licenseStream, cbProductId);
	if (!Stream_CheckAndLogRequiredLength(TAG, licenseStream, cbProductId))
		goto out_free_stream;
#ifdef WITH_DEBUG_LICENSE
	WLog_DBG(TAG, "Product id:");
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Pointer(licenseStream), cbProductId);
#endif
	Stream_Seek(licenseStream, cbProductId);

	/* licenseInfo */
	if (!Stream_CheckAndLogRequiredLength(TAG, licenseStream, 4))
		goto out_free_stream;
	Stream_Read_UINT32(licenseStream, cbLicenseInfo);
	if (!Stream_CheckAndLogRequiredLength(TAG, licenseStream, cbLicenseInfo))
		goto out_free_stream;

	license->state = LICENSE_STATE_COMPLETED;

	ret = TRUE;
	if (!license->rdp->settings->OldLicenseBehaviour)
		ret = saveCal(license->rdp->settings, Stream_Pointer(licenseStream), cbLicenseInfo,
		              license->rdp->settings->ClientHostname);

out_free_stream:
	Stream_Free(licenseStream, FALSE);
out_free_blob:
	license_free_binary_blob(calBlob);
	return ret;
}

/**
 * Read an ERROR_ALERT packet.\n
 * @msdn{cc240482}
 * @param license license module
 * @param s stream
 */

BOOL license_read_error_alert_packet(rdpLicense* license, wStream* s)
{
	UINT32 dwErrorCode;
	UINT32 dwStateTransition;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
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
 * Write a NEW_LICENSE_REQUEST packet.\n
 * @msdn{cc241918}
 * @param license license module
 * @param s stream
 */

BOOL license_write_new_license_request_packet(rdpLicense* license, wStream* s)
{
	UINT32 PlatformId = PLATFORMID;
	UINT32 PreferredKeyExchangeAlg = KEY_EXCHANGE_ALG_RSA;

	Stream_Write_UINT32(s, PreferredKeyExchangeAlg); /* PreferredKeyExchangeAlg (4 bytes) */
	Stream_Write_UINT32(s, PlatformId);              /* PlatformId (4 bytes) */
	Stream_Write(s, license->ClientRandom, 32);      /* ClientRandom (32 bytes) */

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
	WLog_DBG(TAG, "PreferredKeyExchangeAlg: 0x%08" PRIX32 "", PreferredKeyExchangeAlg);
	WLog_DBG(TAG, "ClientRandom:");
	winpr_HexDump(TAG, WLOG_DEBUG, license->ClientRandom, 32);
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

/**
 * Send a NEW_LICENSE_REQUEST packet.\n
 * @msdn{cc241918}
 * @param license license module
 */

BOOL license_answer_license_request(rdpLicense* license)
{
	wStream* s;
	BYTE* license_data = NULL;
	size_t license_size = 0;
	BOOL status;
	char* username;

	if (!license->rdp->settings->OldLicenseBehaviour)
		license_data = loadCalFile(license->rdp->settings, license->rdp->settings->ClientHostname,
		                           &license_size);

	if (license_data)
	{
		LICENSE_BLOB* calBlob = NULL;
		BYTE signature[LICENSING_ENCRYPTION_KEY_LENGTH];

		DEBUG_LICENSE("Sending Saved License Packet");

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

		status = license_send_client_info(license, calBlob, signature);
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

	license->ClientUserName->data = (BYTE*)username;
	license->ClientUserName->length = strlen(username) + 1;
	license->ClientMachineName->data = (BYTE*)license->rdp->settings->ClientHostname;
	license->ClientMachineName->length = strlen(license->rdp->settings->ClientHostname) + 1;

	status = license_write_new_license_request_packet(license, s);

	license->ClientUserName->data = NULL;
	license->ClientUserName->length = 0;
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
 * Write Client Challenge Response Packet.\n
 * @msdn{cc241922}
 * @param license license module
 * @param s stream
 * @param mac_data signature
 */

BOOL license_write_platform_challenge_response_packet(rdpLicense* license, wStream* s,
                                                      const BYTE* macData)
{
	if (!license_write_binary_blob(
	        s,
	        license->EncryptedPlatformChallengeResponse) || /* EncryptedPlatformChallengeResponse */
	    !license_write_binary_blob(s, license->EncryptedHardwareId) || /* EncryptedHWID */
	    !Stream_EnsureRemainingCapacity(s, 16))
	{
		return FALSE;
	}

	Stream_Write(s, macData, 16); /* MACData */
	return TRUE;
}

/**
 * Send Client Challenge Response Packet.\n
 * @msdn{cc241922}
 * @param license license module
 */

BOOL license_send_platform_challenge_response_packet(rdpLicense* license)
{
	wStream* s;
	wStream* challengeRespData;
	int length;
	BYTE* buffer;
	BYTE mac_data[16];
	BOOL status;

	DEBUG_LICENSE("Sending Platform Challenge Response Packet");
	s = license_send_stream_init(license);
	license->EncryptedPlatformChallenge->type = BB_DATA_BLOB;

	/* prepare the PLATFORM_CHALLENGE_RESPONSE_DATA */
	challengeRespData = Stream_New(NULL, 8 + license->PlatformChallenge->length);
	if (!challengeRespData)
		return FALSE;
	Stream_Write_UINT16(challengeRespData, 0x0100);                        /* wVersion */
	Stream_Write_UINT16(challengeRespData, OTHER_PLATFORM_CHALLENGE_TYPE); /* wClientType */
	Stream_Write_UINT16(challengeRespData, LICENSE_DETAIL_DETAIL);         /* wLicenseDetailLevel */
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
	status = security_mac_data(license->MacSaltKey, buffer, length, mac_data);
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
	if (license_write_platform_challenge_response_packet(license, s, mac_data))
		return license_send(license, s, PLATFORM_CHALLENGE_RESPONSE);

	Stream_Release(s);
	return FALSE;
}

/**
 * Send Server License Error - Valid Client Packet.\n
 * @msdn{cc241922}
 * @param license license module
 */

BOOL license_send_valid_client_error_packet(rdpRdp* rdp)
{
	rdpLicense* license = rdp->license;
	wStream* s = license_send_stream_init(license);
	if (!s)
		return FALSE;

	DEBUG_LICENSE("Sending Error Alert Packet");
	Stream_Write_UINT32(s, STATUS_VALID_CLIENT); /* dwErrorCode */
	Stream_Write_UINT32(s, ST_NO_TRANSITION);    /* dwStateTransition */

	if (license_write_binary_blob(s, license->ErrorInfo))
		return license_send(license, s, ERROR_ALERT);

	Stream_Release(s);
	return FALSE;
}

/**
 * Instantiate new license module.
 * @param rdp RDP module
 * @return new license module
 */

rdpLicense* license_new(rdpRdp* rdp)
{
	rdpLicense* license;
	license = (rdpLicense*)calloc(1, sizeof(rdpLicense));
	if (!license)
		return NULL;

	license->rdp = rdp;
	license->state = LICENSE_STATE_AWAIT;
	if (!(license->certificate = certificate_new()))
		goto out_error;
	if (!(license->ProductInfo = license_new_product_info()))
		goto out_error;
	if (!(license->ErrorInfo = license_new_binary_blob(BB_ERROR_BLOB)))
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
		license_free_binary_blob(license->KeyExchangeList);
		license_free_binary_blob(license->ServerCertificate);
		license_free_binary_blob(license->ClientUserName);
		license_free_binary_blob(license->ClientMachineName);
		license_free_binary_blob(license->PlatformChallenge);
		license_free_binary_blob(license->EncryptedPlatformChallenge);
		license_free_binary_blob(license->EncryptedPlatformChallengeResponse);
		license_free_binary_blob(license->EncryptedPremasterSecret);
		license_free_binary_blob(license->EncryptedHardwareId);
		license_free_scope_list(license->ScopeList);
		free(license);
	}
}
