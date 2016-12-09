/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Client Info
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <freerdp/crypto/crypto.h>
#include <freerdp/log.h>
#include <freerdp/session.h>
#include <stdio.h>

#include "timezone.h"

#include "info.h"

#define TAG FREERDP_TAG("core.info")

static const char* const INFO_TYPE_LOGON_STRINGS[4] =
{
	"Logon Info V1",
	"Logon Info V2",
	"Logon Plain Notify",
	"Logon Extended Info"
};

BOOL rdp_compute_client_auto_reconnect_cookie(rdpRdp* rdp)
{
	WINPR_HMAC_CTX hmac;
	BYTE ClientRandom[32];
	BYTE AutoReconnectRandom[32];
	ARC_SC_PRIVATE_PACKET* serverCookie;
	ARC_CS_PRIVATE_PACKET* clientCookie;
	rdpSettings* settings = rdp->settings;

	serverCookie = settings->ServerAutoReconnectCookie;
	clientCookie = settings->ClientAutoReconnectCookie;

	clientCookie->cbLen = 28;
	clientCookie->version = serverCookie->version;
	clientCookie->logonId = serverCookie->logonId;
	ZeroMemory(clientCookie->securityVerifier, 16);

	ZeroMemory(AutoReconnectRandom, sizeof(AutoReconnectRandom));
	CopyMemory(AutoReconnectRandom, serverCookie->arcRandomBits, 16);

	ZeroMemory(ClientRandom, sizeof(ClientRandom));

	if (settings->SelectedProtocol == PROTOCOL_RDP)
		CopyMemory(ClientRandom, settings->ClientRandom, settings->ClientRandomLength);

	/* SecurityVerifier = HMAC_MD5(AutoReconnectRandom, ClientRandom) */

	if (!winpr_HMAC_Init(&hmac, WINPR_MD_MD5, AutoReconnectRandom, 16))
		return FALSE;
	if (!winpr_HMAC_Update(&hmac, ClientRandom, 32))
		return FALSE;
	if (!winpr_HMAC_Final(&hmac, clientCookie->securityVerifier, 16))
		return FALSE;

	return TRUE;
}

/**
 * Read Server Auto Reconnect Cookie (ARC_SC_PRIVATE_PACKET).\n
 * @msdn{cc240540}
 * @param s stream
 * @param settings settings
 */

BOOL rdp_read_server_auto_reconnect_cookie(rdpRdp* rdp, wStream* s, logon_info_ex *info)
{
	BYTE* p;
	ARC_SC_PRIVATE_PACKET* autoReconnectCookie;
	rdpSettings* settings = rdp->settings;

	autoReconnectCookie = settings->ServerAutoReconnectCookie;

	if (Stream_GetRemainingLength(s) < 28)
		return FALSE;

	Stream_Read_UINT32(s, autoReconnectCookie->cbLen); /* cbLen (4 bytes) */
	if (autoReconnectCookie->cbLen != 28)
	{
		WLog_ERR(TAG, "ServerAutoReconnectCookie.cbLen != 28");
		return FALSE;
	}

	Stream_Read_UINT32(s, autoReconnectCookie->version); /* Version (4 bytes) */
	Stream_Read_UINT32(s, autoReconnectCookie->logonId); /* LogonId (4 bytes) */
	Stream_Read(s, autoReconnectCookie->arcRandomBits, 16); /* ArcRandomBits (16 bytes) */

	p = autoReconnectCookie->arcRandomBits;

	WLog_DBG(TAG, "ServerAutoReconnectCookie: Version: %d LogonId: %d SecurityVerifier: "
			"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			autoReconnectCookie->version, autoReconnectCookie->logonId,
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
			p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);

	info->LogonId = autoReconnectCookie->logonId;
	CopyMemory(info->ArcRandomBits, p, 16);

	if ((settings->PrintReconnectCookie) && (autoReconnectCookie->cbLen > 0))
	{
		char* base64;
		base64 = crypto_base64_encode((BYTE*) autoReconnectCookie, sizeof(ARC_SC_PRIVATE_PACKET));
		WLog_INFO(TAG, "Reconnect-cookie: %s", base64);
		free(base64);
	}

	return TRUE;
}

/**
 * Read Client Auto Reconnect Cookie (ARC_CS_PRIVATE_PACKET).\n
 * @msdn{cc240541}
 * @param s stream
 * @param settings settings
 */

BOOL rdp_read_client_auto_reconnect_cookie(rdpRdp* rdp, wStream* s)
{
	ARC_CS_PRIVATE_PACKET* autoReconnectCookie;
	rdpSettings* settings = rdp->settings;

	autoReconnectCookie = settings->ClientAutoReconnectCookie;

	if (Stream_GetRemainingLength(s) < 28)
		return FALSE;

	Stream_Read_UINT32(s, autoReconnectCookie->cbLen); /* cbLen (4 bytes) */
	Stream_Read_UINT32(s, autoReconnectCookie->version); /* version (4 bytes) */
	Stream_Read_UINT32(s, autoReconnectCookie->logonId); /* LogonId (4 bytes) */
	Stream_Read(s, autoReconnectCookie->securityVerifier, 16); /* SecurityVerifier */

	return TRUE;
}

/**
 * Write Client Auto Reconnect Cookie (ARC_CS_PRIVATE_PACKET).\n
 * @msdn{cc240541}
 * @param s stream
 * @param settings settings
 */

void rdp_write_client_auto_reconnect_cookie(rdpRdp* rdp, wStream* s)
{
	BYTE* p;
	ARC_CS_PRIVATE_PACKET* autoReconnectCookie;
	rdpSettings* settings = rdp->settings;

	autoReconnectCookie = settings->ClientAutoReconnectCookie;

	p = autoReconnectCookie->securityVerifier;

	WLog_DBG(TAG, "ClientAutoReconnectCookie: Version: %d LogonId: %d ArcRandomBits: "
			"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			autoReconnectCookie->version, autoReconnectCookie->logonId,
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
			p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);

	Stream_Write_UINT32(s, autoReconnectCookie->cbLen); /* cbLen (4 bytes) */
	Stream_Write_UINT32(s, autoReconnectCookie->version); /* version (4 bytes) */
	Stream_Write_UINT32(s, autoReconnectCookie->logonId); /* LogonId (4 bytes) */
	Stream_Write(s, autoReconnectCookie->securityVerifier, 16); /* SecurityVerifier (16 bytes) */
}

/**
 * Read Extended Info Packet (TS_EXTENDED_INFO_PACKET).\n
 * @msdn{cc240476}
 * @param s stream
 * @param settings settings
 */

BOOL rdp_read_extended_info_packet(rdpRdp* rdp, wStream* s)
{
	UINT16 clientAddressFamily;
	UINT16 cbClientAddress;
	UINT16 cbClientDir;
	UINT16 cbAutoReconnectLen;
	rdpSettings* settings = rdp->settings;
	WCHAR* wstr;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT16(s, clientAddressFamily); /* clientAddressFamily (2 bytes) */
	Stream_Read_UINT16(s, cbClientAddress); /* cbClientAddress (2 bytes) */

	/* cbClientAddress is the size in bytes of the character data in the clientAddress field.
	 * This size includes the length of the mandatory null terminator.
	 * The maximum allowed value is 80 bytes
	 * Note: Although according to [MS-RDPBCGR 2.2.1.11.1.1.1] the null terminator
	 * is mandatory, connections via Microsoft's TS Gateway set cbClientAddress to 0.
	 */

	if ((cbClientAddress % 2) || cbClientAddress > 80)
	{
		WLog_ERR(TAG, "protocol error: invalid cbClientAddress value: %u", cbClientAddress);
		return FALSE;
	}

	settings->IPv6Enabled = (clientAddressFamily == ADDRESS_FAMILY_INET6 ? TRUE : FALSE);

	if (Stream_GetRemainingLength(s) < cbClientAddress)
		return FALSE;

	if (settings->ClientAddress)
	{
		free(settings->ClientAddress);
		settings->ClientAddress = NULL;
	}

	if (cbClientAddress)
	{
		wstr = (WCHAR*) Stream_Pointer(s);
		if (wstr[cbClientAddress / 2 - 1])
		{
			WLog_ERR(TAG, "protocol error: clientAddress must be null terminated");
			return FALSE;
		}
		if (ConvertFromUnicode(CP_UTF8, 0, wstr, -1, &settings->ClientAddress, 0, NULL, NULL) < 1)
		{
			WLog_ERR(TAG, "failed to convert client address");
			return FALSE;
		}
		Stream_Seek(s, cbClientAddress);
		WLog_DBG(TAG, "rdp client address: [%s]", settings->ClientAddress);
	}

	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16(s, cbClientDir); /* cbClientDir (2 bytes) */

	/* cbClientDir is the size in bytes of the character data in the clientDir field.
	 * This size includes the length of the mandatory null terminator.
	 * The maximum allowed value is 512 bytes.
	 * Note: Although according to [MS-RDPBCGR 2.2.1.11.1.1.1] the null terminator
	 * is mandatory the Microsoft Android client (starting with version 8.1.31.44)
	 * sets cbClientDir to 0.
	 */

	if ((cbClientDir % 2) || cbClientDir > 512)
	{
		WLog_ERR(TAG, "protocol error: invalid cbClientDir value: %u", cbClientDir);
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) < cbClientDir)
		return FALSE;

	if (settings->ClientDir)
	{
		free(settings->ClientDir);
		settings->ClientDir = NULL;
	}

	if (cbClientDir)
	{
		wstr = (WCHAR*) Stream_Pointer(s);
		if (wstr[cbClientDir / 2 - 1])
		{
			WLog_ERR(TAG, "protocol error: clientDir must be null terminated");
			return FALSE;
		}
		if (ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(s), -1, &settings->ClientDir, 0, NULL, NULL) < 1)
		{
			WLog_ERR(TAG, "failed to convert client directory");
			return FALSE;
		}
		Stream_Seek(s, cbClientDir);
		WLog_DBG(TAG, "rdp client dir: [%s]", settings->ClientDir);
	}

	/**
	 * down below all fields are optional but if one field is not present,
	 * then all of the subsequent fields also MUST NOT be present.
	 */

	/* optional: clientTimeZone (172 bytes) */
	if (Stream_GetRemainingLength(s) == 0)
		return TRUE;
	if (!rdp_read_client_time_zone(s, settings))
		return FALSE;

	/* optional: clientSessionId (4 bytes), should be set to 0 */
	if (Stream_GetRemainingLength(s) == 0)
		return TRUE;
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;
	Stream_Seek_UINT32(s);

	/* optional: performanceFlags (4 bytes) */
	if (Stream_GetRemainingLength(s) == 0)
		return TRUE;
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;
	Stream_Read_UINT32(s, settings->PerformanceFlags);
	freerdp_performance_flags_split(settings);

	/* optional: cbAutoReconnectLen (2 bytes) */
	if (Stream_GetRemainingLength(s) == 0)
		return TRUE;
	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;
	Stream_Read_UINT16(s, cbAutoReconnectLen);

	/* optional: autoReconnectCookie (28 bytes) */
	/* must be present if cbAutoReconnectLen is > 0 */
	if (cbAutoReconnectLen > 0)
		return rdp_read_client_auto_reconnect_cookie(rdp, s);

	/* TODO */

	/* reserved1 (2 bytes) */
	/* reserved2 (2 bytes) */
	/* cbDynamicDSTTimeZoneKeyName (2 bytes) */
	/* dynamicDSTTimeZoneKeyName (variable) */
	/* dynamicDaylightTimeDisabled (2 bytes) */

	return TRUE;
}

/**
 * Write Extended Info Packet (TS_EXTENDED_INFO_PACKET).\n
 * @msdn{cc240476}
 * @param s stream
 * @param settings settings
 */

void rdp_write_extended_info_packet(rdpRdp* rdp, wStream* s)
{
	int clientAddressFamily;
	WCHAR* clientAddress = NULL;
	int cbClientAddress;
	WCHAR* clientDir = NULL;
	int cbClientDir;
	int cbAutoReconnectCookie;
	rdpSettings* settings = rdp->settings;

	clientAddressFamily = settings->IPv6Enabled ? ADDRESS_FAMILY_INET6 : ADDRESS_FAMILY_INET;

	cbClientAddress = ConvertToUnicode(CP_UTF8, 0, settings->ClientAddress, -1, &clientAddress, 0) * 2;

	cbClientDir = ConvertToUnicode(CP_UTF8, 0, settings->ClientDir, -1, &clientDir, 0) * 2;

	cbAutoReconnectCookie = (int) settings->ServerAutoReconnectCookie->cbLen;

	Stream_Write_UINT16(s, clientAddressFamily); /* clientAddressFamily (2 bytes) */

	Stream_Write_UINT16(s, cbClientAddress + 2); /* cbClientAddress (2 bytes) */

	if (cbClientAddress > 0)
		Stream_Write(s, clientAddress, cbClientAddress); /* clientAddress */
	Stream_Write_UINT16(s, 0);

	Stream_Write_UINT16(s, cbClientDir + 2); /* cbClientDir (2 bytes) */

	if (cbClientDir > 0)
		Stream_Write(s, clientDir, cbClientDir); /* clientDir */
	Stream_Write_UINT16(s, 0);

	rdp_write_client_time_zone(s, settings); /* clientTimeZone (172 bytes) */

	Stream_Write_UINT32(s, 0); /* clientSessionId (4 bytes), should be set to 0 */

	freerdp_performance_flags_make(settings);
	Stream_Write_UINT32(s, settings->PerformanceFlags); /* performanceFlags (4 bytes) */

	Stream_Write_UINT16(s, cbAutoReconnectCookie); /* cbAutoReconnectCookie (2 bytes) */

	if (cbAutoReconnectCookie > 0)
	{
		rdp_compute_client_auto_reconnect_cookie(rdp);

		rdp_write_client_auto_reconnect_cookie(rdp, s); /* autoReconnectCookie */

		Stream_Write_UINT16(s, 0); /* reserved1 (2 bytes) */
		Stream_Write_UINT16(s, 0); /* reserved2 (2 bytes) */
	}

	free(clientAddress);
	free(clientDir);
}

/**
 * Read Info Packet (TS_INFO_PACKET).\n
 * @msdn{cc240475}
 * @param s stream
 * @param settings settings
 */

BOOL rdp_read_info_packet(rdpRdp* rdp, wStream* s)
{
	UINT32 flags;
	UINT16 cbDomain;
	UINT16 cbUserName;
	UINT16 cbPassword;
	UINT16 cbAlternateShell;
	UINT16 cbWorkingDir;
	UINT32 CompressionLevel;
	rdpSettings* settings = rdp->settings;
	WCHAR* wstr;

	if (Stream_GetRemainingLength(s) < 18)
		return FALSE;

	Stream_Seek_UINT32(s); /* CodePage (4 bytes ) */
	Stream_Read_UINT32(s, flags); /* flags (4 bytes) */

	settings->AudioCapture = ((flags & INFO_AUDIOCAPTURE) ? TRUE : FALSE);
	settings->AudioPlayback = ((flags & INFO_NOAUDIOPLAYBACK) ? FALSE : TRUE);
	settings->AutoLogonEnabled = ((flags & INFO_AUTOLOGON) ? TRUE : FALSE);
	settings->RemoteApplicationMode = ((flags & INFO_RAIL) ? TRUE : FALSE);
	settings->RemoteConsoleAudio = ((flags & INFO_REMOTECONSOLEAUDIO) ? TRUE : FALSE);
	settings->CompressionEnabled = ((flags & INFO_COMPRESSION) ? TRUE : FALSE);

	if (flags & INFO_COMPRESSION)
	{
		CompressionLevel = ((flags & 0x00001E00) >> 9);
		settings->CompressionLevel = CompressionLevel;
	}

	if (!(flags & INFO_UNICODE))
	{
		WLog_ERR(TAG, "Client without INFO_UNICODE flag: this is currently not supported");
		return FALSE;
	}

	Stream_Read_UINT16(s, cbDomain); /* cbDomain (2 bytes) */
	Stream_Read_UINT16(s, cbUserName); /* cbUserName (2 bytes) */
	Stream_Read_UINT16(s, cbPassword); /* cbPassword (2 bytes) */
	Stream_Read_UINT16(s, cbAlternateShell); /* cbAlternateShell (2 bytes) */
	Stream_Read_UINT16(s, cbWorkingDir); /* cbWorkingDir (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t) (cbDomain + 2))
		return FALSE;

	if (cbDomain > 0)
	{
		/* cbDomain is the size in bytes of the character data in the Domain field.
		 * This size excludes (!) the length of the mandatory null terminator.
		 * Maximum value including the mandatory null terminator: 512
		 */
		if ((cbDomain % 2) || cbDomain > 512)
		{
			WLog_ERR(TAG, "protocol error: invalid cbDomain value: %u", cbDomain);
			return FALSE;
		}
		wstr = (WCHAR*) Stream_Pointer(s);
		if (wstr[cbDomain / 2])
		{
			WLog_ERR(TAG, "protocol error: Domain must be null terminated");
			return FALSE;
		}
		if (ConvertFromUnicode(CP_UTF8, 0, wstr, -1, &settings->Domain, 0, NULL, NULL) < 1)
		{
			WLog_ERR(TAG, "failed to convert Domain string");
			return FALSE;
		}
		Stream_Seek(s, cbDomain);
	}
	Stream_Seek(s, 2);

	if (Stream_GetRemainingLength(s) < (size_t) (cbUserName + 2))
		return FALSE;

	if (cbUserName > 0)
	{
		/* cbUserName is the size in bytes of the character data in the UserName field.
		 * This size excludes (!) the length of the mandatory null terminator.
		 * Maximum value including the mandatory null terminator: 512
		 */
		if ((cbUserName % 2) || cbUserName > 512)
		{
			WLog_ERR(TAG, "protocol error: invalid cbUserName value: %u", cbUserName);
			return FALSE;
		}
		wstr = (WCHAR*) Stream_Pointer(s);
		if (wstr[cbUserName / 2])
		{
			WLog_ERR(TAG, "protocol error: UserName must be null terminated");
			return FALSE;
		}
		if (ConvertFromUnicode(CP_UTF8, 0, wstr, -1, &settings->Username, 0, NULL, NULL) < 1)
		{
			WLog_ERR(TAG, "failed to convert UserName string");
			return FALSE;
		}
		Stream_Seek(s, cbUserName);
	}
	Stream_Seek(s, 2);

	if (Stream_GetRemainingLength(s) < (size_t) (cbPassword + 2))
		return FALSE;

	if (cbPassword > 0)
	{
		/* cbPassword is the size in bytes of the character data in the Password field.
		 * This size excludes (!) the length of the mandatory null terminator.
		 * Maximum value including the mandatory null terminator: 512
		 */
		if ((cbPassword % 2) || cbPassword > 512)
		{
			WLog_ERR(TAG, "protocol error: invalid cbPassword value: %u", cbPassword);
			return FALSE;
		}
		wstr = (WCHAR*) Stream_Pointer(s);
		if (wstr[cbPassword / 2])
		{
			WLog_ERR(TAG, "protocol error: Password must be null terminated");
			return FALSE;
		}
		if (ConvertFromUnicode(CP_UTF8, 0, wstr, -1, &settings->Password, 0, NULL, NULL) < 1)
		{
			WLog_ERR(TAG, "failed to convert Password string");
			return FALSE;
		}
		Stream_Seek(s, cbPassword);
	}
	Stream_Seek(s, 2);

	if (Stream_GetRemainingLength(s) < (size_t) (cbAlternateShell + 2))
		return FALSE;

	if (cbAlternateShell > 0)
	{
		/* cbAlternateShell is the size in bytes of the character data in the AlternateShell field.
		 * This size excludes (!) the length of the mandatory null terminator.
		 * Maximum value including the mandatory null terminator: 512
		 */
		if ((cbAlternateShell % 2) || cbAlternateShell > 512)
		{
			WLog_ERR(TAG, "protocol error: invalid cbAlternateShell value: %u", cbAlternateShell);
			return FALSE;
		}
		wstr = (WCHAR*) Stream_Pointer(s);
		if (wstr[cbAlternateShell / 2])
		{
			WLog_ERR(TAG, "protocol error: AlternateShell must be null terminated");
			return FALSE;
		}
		if (ConvertFromUnicode(CP_UTF8, 0, wstr, -1, &settings->AlternateShell, 0, NULL, NULL) < 1)
		{
			WLog_ERR(TAG, "failed to convert AlternateShell string");
			return FALSE;
		}
		Stream_Seek(s, cbAlternateShell);
	}
	Stream_Seek(s, 2);

	if (Stream_GetRemainingLength(s) < (size_t) (cbWorkingDir + 2))
		return FALSE;

	if (cbWorkingDir > 0)
	{
		/* cbWorkingDir is the size in bytes of the character data in the WorkingDir field.
		 * This size excludes (!) the length of the mandatory null terminator.
		 * Maximum value including the mandatory null terminator: 512
		 */
		if ((cbWorkingDir % 2) || cbWorkingDir > 512)
		{
			WLog_ERR(TAG, "protocol error: invalid cbWorkingDir value: %u", cbWorkingDir);
			return FALSE;
		}
		wstr = (WCHAR*) Stream_Pointer(s);
		if (wstr[cbWorkingDir / 2])
		{
			WLog_ERR(TAG, "protocol error: WorkingDir must be null terminated");
			return FALSE;
		}
		if (ConvertFromUnicode(CP_UTF8, 0, wstr, -1, &settings->ShellWorkingDirectory, 0, NULL, NULL) < 1)
		{
			WLog_ERR(TAG, "failed to convert AlternateShell string");
			return FALSE;
		}
		Stream_Seek(s, cbWorkingDir);
	}
	Stream_Seek(s, 2);

	if (settings->RdpVersion >= 5)
		return rdp_read_extended_info_packet(rdp, s); /* extraInfo */

	return TRUE;
}

/**
 * Write Info Packet (TS_INFO_PACKET).\n
 * @msdn{cc240475}
 * @param s stream
 * @param settings settings
 */

void rdp_write_info_packet(rdpRdp* rdp, wStream* s)
{
	UINT32 flags;
	WCHAR* domainW = NULL;
	int cbDomain = 0;
	WCHAR* userNameW = NULL;
	int cbUserName = 0;
	WCHAR* passwordW = NULL;
	int cbPassword = 0;
	WCHAR* alternateShellW = NULL;
	int cbAlternateShell = 0;
	WCHAR* workingDirW = NULL;
	int cbWorkingDir = 0;
	BOOL usedPasswordCookie = FALSE;
	rdpSettings* settings = rdp->settings;

	flags = INFO_MOUSE |
		INFO_UNICODE |
		INFO_LOGONERRORS |
		INFO_LOGONNOTIFY |
		INFO_MAXIMIZESHELL |
		INFO_ENABLEWINDOWSKEY |
		INFO_DISABLECTRLALTDEL;

	if (settings->AudioCapture)
		flags |= INFO_AUDIOCAPTURE;

	if (!settings->AudioPlayback)
		flags |= INFO_NOAUDIOPLAYBACK;

	if (settings->VideoDisable)
		flags |= INFO_VIDEO_DISABLE;

	if (settings->AutoLogonEnabled)
		flags |= INFO_AUTOLOGON;

	if (settings->RemoteApplicationMode)
		flags |= INFO_RAIL;

	if (settings->RemoteConsoleAudio)
		flags |= INFO_REMOTECONSOLEAUDIO;

	if (settings->HiDefRemoteApp)
		flags |= INFO_HIDEF_RAIL_SUPPORTED;

	if (settings->CompressionEnabled)
	{
		flags |= INFO_COMPRESSION;
		flags |= ((settings->CompressionLevel << 9) & 0x00001E00);
	}

	if (settings->Domain)
	{
		cbDomain = ConvertToUnicode(CP_UTF8, 0, settings->Domain, -1, &domainW, 0) * 2;
	}
	else
	{
		domainW = NULL;
		cbDomain = 0;
	}

	if (!settings->RemoteAssistanceMode)
	{
		cbUserName = ConvertToUnicode(CP_UTF8, 0, settings->Username, -1, &userNameW, 0) * 2;
	}
	else
	{
		/* user name provided by the expert for connecting to the novice computer */
		cbUserName = ConvertToUnicode(CP_UTF8, 0, settings->Username, -1, &userNameW, 0) * 2;
	}

	if (!settings->RemoteAssistanceMode)
	{
		if (settings->RedirectionPassword && settings->RedirectionPasswordLength > 0)
		{
			usedPasswordCookie = TRUE;
			passwordW = (WCHAR*) settings->RedirectionPassword;
			cbPassword = settings->RedirectionPasswordLength - 2; /* Strip double zero termination */
		}
		else
		{
			cbPassword = ConvertToUnicode(CP_UTF8, 0, settings->Password, -1, &passwordW, 0) * 2;
		}
	}
	else
	{
		/* This field MUST be filled with "*" */
		cbPassword = ConvertToUnicode(CP_UTF8, 0, "*", -1, &passwordW, 0) * 2;
	}

	if (!settings->RemoteAssistanceMode)
	{
		cbAlternateShell = ConvertToUnicode(CP_UTF8, 0, settings->AlternateShell, -1, &alternateShellW, 0) * 2;
	}
	else
	{
		if (settings->RemoteAssistancePassStub)
		{
			/* This field MUST be filled with "*" */
			cbAlternateShell = ConvertToUnicode(CP_UTF8, 0, "*", -1, &alternateShellW, 0) * 2;
		}
		else
		{
			/* This field must contain the remote assistance password */
			cbAlternateShell = ConvertToUnicode(CP_UTF8, 0, settings->RemoteAssistancePassword, -1, &alternateShellW, 0) * 2;
		}
	}

	if (!settings->RemoteAssistanceMode)
	{
		cbWorkingDir = ConvertToUnicode(CP_UTF8, 0, settings->ShellWorkingDirectory, -1, &workingDirW, 0) * 2;
	}
	else
	{
		/* Remote Assistance Session Id */
		cbWorkingDir = ConvertToUnicode(CP_UTF8, 0, settings->RemoteAssistanceSessionId, -1, &workingDirW, 0) * 2;
	}

	Stream_Write_UINT32(s, 0); /* CodePage (4 bytes) */
	Stream_Write_UINT32(s, flags); /* flags (4 bytes) */

	Stream_Write_UINT16(s, cbDomain); /* cbDomain (2 bytes) */
	Stream_Write_UINT16(s, cbUserName); /* cbUserName (2 bytes) */
	Stream_Write_UINT16(s, cbPassword); /* cbPassword (2 bytes) */
	Stream_Write_UINT16(s, cbAlternateShell); /* cbAlternateShell (2 bytes) */
	Stream_Write_UINT16(s, cbWorkingDir); /* cbWorkingDir (2 bytes) */

	if (cbDomain > 0)
		Stream_Write(s, domainW, cbDomain);
	Stream_Write_UINT16(s, 0);

	if (cbUserName > 0)
		Stream_Write(s, userNameW, cbUserName);
	Stream_Write_UINT16(s, 0);

	if (cbPassword > 0)
		Stream_Write(s, passwordW, cbPassword);
	Stream_Write_UINT16(s, 0);

	if (cbAlternateShell > 0)
		Stream_Write(s, alternateShellW, cbAlternateShell);
	Stream_Write_UINT16(s, 0);

	if (cbWorkingDir > 0)
		Stream_Write(s, workingDirW, cbWorkingDir);
	Stream_Write_UINT16(s, 0);

	free(domainW);
	free(userNameW);
	free(alternateShellW);
	free(workingDirW);

	if (!usedPasswordCookie)
		free(passwordW);

	if (settings->RdpVersion >= 5)
		rdp_write_extended_info_packet(rdp, s); /* extraInfo */
}

/**
 * Read Client Info PDU (CLIENT_INFO_PDU).\n
 * @msdn{cc240474}
 * @param rdp RDP module
 * @param s stream
 */

BOOL rdp_recv_client_info(rdpRdp* rdp, wStream* s)
{
	UINT16 length;
	UINT16 channelId;
	UINT16 securityFlags;

	if (!rdp_read_header(rdp, s, &length, &channelId))
		return FALSE;

	if (!rdp_read_security_header(s, &securityFlags))
		return FALSE;

	if ((securityFlags & SEC_INFO_PKT) == 0)
		return FALSE;

	if (rdp->settings->UseRdpSecurityLayer)
	{
		if (securityFlags & SEC_REDIRECTION_PKT)
		{
			WLog_ERR(TAG, "Error: SEC_REDIRECTION_PKT unsupported");
			return FALSE;
		}

		if (securityFlags & SEC_ENCRYPT)
		{
			if (!rdp_decrypt(rdp, s, length - 4, securityFlags))
			{
				WLog_ERR(TAG, "rdp_decrypt failed");
				return FALSE;
			}
		}
	}

	return rdp_read_info_packet(rdp, s);
}

/**
 * Send Client Info PDU (CLIENT_INFO_PDU).\n
 * @msdn{cc240474}
 * @param rdp RDP module
 */

BOOL rdp_send_client_info(rdpRdp* rdp)
{
	wStream* s;
	BOOL status;

	rdp->sec_flags |= SEC_INFO_PKT;

	s = Stream_New(NULL, 2048);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	rdp_init_stream(rdp, s);

	rdp_write_info_packet(rdp, s);

	status = rdp_send(rdp, s, MCS_GLOBAL_CHANNEL_ID);

	Stream_Free(s, TRUE);

	return status;
}

BOOL rdp_recv_logon_info_v1(rdpRdp* rdp, wStream* s, logon_info *info)
{
	UINT32 cbDomain;
	UINT32 cbUserName;
	WCHAR* wstr;

	ZeroMemory(info, sizeof(*info));

	if (Stream_GetRemainingLength(s) < 576)
		return FALSE;

	Stream_Read_UINT32(s, cbDomain); /* cbDomain (4 bytes) */

	/* cbDomain is the size of the Unicode character data (including the mandatory
	 * null terminator) in bytes present in the fixed-length (52 bytes) Domain field
	 */
	if (cbDomain)
	{
		if ((cbDomain % 2) || cbDomain > 52)
		{
			WLog_ERR(TAG, "protocol error: invalid cbDomain value: %lu", cbDomain);
			goto fail;
		}
		wstr = (WCHAR*) Stream_Pointer(s);
		if (wstr[cbDomain / 2 - 1])
		{
			WLog_ERR(TAG, "protocol error: Domain must be null terminated");
			goto fail;
		}
		if (ConvertFromUnicode(CP_UTF8, 0, wstr, -1, &info->domain, 0, NULL, FALSE) < 1)
		{
			WLog_ERR(TAG, "failed to convert the Domain string");
			goto fail;
		}
	}
	Stream_Seek(s, 52); /* domain (52 bytes) */


	Stream_Read_UINT32(s, cbUserName); /* cbUserName (4 bytes) */

	/* cbUserName is the size of the Unicode character data (including the mandatory
	 * null terminator) in bytes present in the fixed-length (512 bytes) UserName field.
	 */
	if (cbUserName)
	{
		if ((cbUserName % 2) || cbUserName > 512)
		{
			WLog_ERR(TAG, "protocol error: invalid cbUserName value: %lu", cbUserName);
			goto fail;
		}
		wstr = (WCHAR*) Stream_Pointer(s);
		if (wstr[cbUserName / 2 - 1])
		{
			WLog_ERR(TAG, "protocol error: UserName must be null terminated");
			goto fail;
		}
		if (ConvertFromUnicode(CP_UTF8, 0, wstr, -1, &info->username, 0, NULL, FALSE) < 1)
		{
			WLog_ERR(TAG, "failed to convert the UserName string");
			goto fail;
		}
	}
	Stream_Seek(s, 512); /* userName (512 bytes) */

	Stream_Read_UINT32(s, info->sessionId); /* SessionId (4 bytes) */

	WLog_DBG(TAG, "LogonInfoV1: SessionId: 0x%04X UserName: [%s] Domain: [%s]",
		 info->sessionId, info->username, info->domain);

	return TRUE;

fail:
	free(info->username);
	info->username = NULL;
	free(info->domain);
	info->domain = NULL;
	return FALSE;
}

BOOL rdp_recv_logon_info_v2(rdpRdp* rdp, wStream* s, logon_info *info)
{
	UINT16 Version;
	UINT32 Size;
	UINT32 cbDomain;
	UINT32 cbUserName;
	WCHAR* wstr;

	ZeroMemory(info, sizeof(*info));

	if (Stream_GetRemainingLength(s) < 576)
		return FALSE;

	Stream_Read_UINT16(s, Version); /* Version (2 bytes) */
	Stream_Read_UINT32(s, Size); /* Size (4 bytes) */
	Stream_Read_UINT32(s, info->sessionId); /* SessionId (4 bytes) */
	Stream_Read_UINT32(s, cbDomain); /* cbDomain (4 bytes) */
	Stream_Read_UINT32(s, cbUserName); /* cbUserName (4 bytes) */
	Stream_Seek(s, 558); /* pad (558 bytes) */


	/* cbDomain is the size in bytes of the Unicode character data in the Domain field.
	 * The size of the mandatory null terminator is include in this value.
	 * Note: Since MS-RDPBCGR 2.2.10.1.1.2 does not mention any size limits we assume
	 *       that the maximum value is 52 bytes, according to the fixed size of the
	 *       Domain field in the Logon Info Version 1 (TS_LOGON_INFO) structure.
	 */
	if (cbDomain)
	{
		if ((cbDomain % 2) || cbDomain > 52)
		{
			WLog_ERR(TAG, "protocol error: invalid cbDomain value: %lu", cbDomain);
			goto fail;
		}
		if (Stream_GetRemainingLength(s) < (size_t) cbDomain)
		{
			WLog_ERR(TAG, "insufficient remaining stream length");
			goto fail;
		}
		wstr = (WCHAR*) Stream_Pointer(s);
		if (wstr[cbDomain / 2 - 1])
		{
			WLog_ERR(TAG, "protocol error: Domain field must be null terminated");
			goto fail;
		}
		if (ConvertFromUnicode(CP_UTF8, 0, wstr, -1, &info->domain, 0, NULL, FALSE) < 1)
		{
			WLog_ERR(TAG, "failed to convert the Domain string");
			goto fail;
		}
	}
	Stream_Seek(s, cbDomain); /* domain */

	/* cbUserName is the size in bytes of the Unicode character data in the UserName field.
	 * The size of the mandatory null terminator is include in this value.
	 * Note: Since MS-RDPBCGR 2.2.10.1.1.2 does not mention any size limits we assume
	 *       that the maximum value is 512 bytes, according to the fixed size of the
	 *       Username field in the Logon Info Version 1 (TS_LOGON_INFO) structure.
	 */
	if (cbUserName)
	{
		if ((cbUserName % 2) || cbUserName < 2 || cbUserName > 512)
		{
			WLog_ERR(TAG, "protocol error: invalid cbUserName value: %lu", cbUserName);
			goto fail;
		}
		if (Stream_GetRemainingLength(s) < (size_t) cbUserName)
		{
			WLog_ERR(TAG, "insufficient remaining stream length");
			goto fail;
		}
		wstr = (WCHAR*) Stream_Pointer(s);
		if (wstr[cbUserName / 2 - 1])
		{
			WLog_ERR(TAG, "protocol error: UserName field must be null terminated");
			goto fail;
		}
		if (ConvertFromUnicode(CP_UTF8, 0, wstr, -1, &info->username, 0, NULL, FALSE) < 1)
		{
			WLog_ERR(TAG, "failed to convert the Domain string");
			goto fail;
		}
	}
	Stream_Seek(s, cbUserName); /* userName */

	WLog_DBG(TAG, "LogonInfoV2: SessionId: 0x%04X UserName: [%s] Domain: [%s]",
		 info->sessionId, info->username, info->domain);

	return TRUE;

fail:
	free(info->username);
	info->username = NULL;
	free(info->domain);
	info->domain = NULL;
	return FALSE;
}

BOOL rdp_recv_logon_plain_notify(rdpRdp* rdp, wStream* s)
{
	if (Stream_GetRemainingLength(s) < 576)
		return FALSE;

	Stream_Seek(s, 576); /* pad (576 bytes) */

	WLog_DBG(TAG, "LogonPlainNotify");

	return TRUE;
}

BOOL rdp_recv_logon_error_info(rdpRdp* rdp, wStream* s, logon_info_ex *info)
{
	UINT32 errorNotificationType;
	UINT32 errorNotificationData;

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT32(s, errorNotificationType); /* errorNotificationType (4 bytes) */
	Stream_Read_UINT32(s, errorNotificationData); /* errorNotificationData (4 bytes) */

	WLog_DBG(TAG, "LogonErrorInfo: Data: 0x%04X Type: 0x%04X",
			errorNotificationData, errorNotificationType);

	IFCALL(rdp->instance->LogonErrorInfo, rdp->instance, errorNotificationData, errorNotificationType);

	info->ErrorNotificationType = errorNotificationType;
	info->ErrorNotificationData = errorNotificationData;
	return TRUE;
}

BOOL rdp_recv_logon_info_extended(rdpRdp* rdp, wStream* s, logon_info_ex *info)
{
	UINT32 cbFieldData;
	UINT32 fieldsPresent;
	UINT16 Length;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Read_UINT16(s, Length); /* Length (2 bytes) */
	Stream_Read_UINT32(s, fieldsPresent); /* fieldsPresent (4 bytes) */

	if ((Length < 6) || (Stream_GetRemainingLength(s) < (Length - 6)))
		return FALSE;

	WLog_DBG(TAG, "LogonInfoExtended: fieldsPresent: 0x%04X", fieldsPresent);

	/* logonFields */

	if (fieldsPresent & LOGON_EX_AUTORECONNECTCOOKIE)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;

		info->haveCookie = TRUE;
		Stream_Read_UINT32(s, cbFieldData); /* cbFieldData (4 bytes) */

		if (Stream_GetRemainingLength(s) < cbFieldData)
			return FALSE;

		if (!rdp_read_server_auto_reconnect_cookie(rdp, s, info))
			return FALSE;
	}

	if (fieldsPresent & LOGON_EX_LOGONERRORS)
	{
		info->haveErrorInfo = TRUE;
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;

		Stream_Read_UINT32(s, cbFieldData); /* cbFieldData (4 bytes) */

		if (Stream_GetRemainingLength(s) < cbFieldData)
			return FALSE;

		if (!rdp_recv_logon_error_info(rdp, s, info))
			return FALSE;
	}

	if (Stream_GetRemainingLength(s) < 570)
		return FALSE;

	Stream_Seek(s, 570); /* pad (570 bytes) */

	return TRUE;
}

BOOL rdp_recv_save_session_info(rdpRdp* rdp, wStream* s)
{
	UINT32 infoType;
	BOOL status;
	logon_info logonInfo;
	logon_info_ex logonInfoEx;
	rdpContext *context = rdp->context;
	rdpUpdate *update = rdp->context->update;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, infoType); /* infoType (4 bytes) */

	switch (infoType)
	{
		case INFO_TYPE_LOGON:
			ZeroMemory(&logonInfo, sizeof(logonInfo));
			status = rdp_recv_logon_info_v1(rdp, s, &logonInfo);
			if (status && update->SaveSessionInfo)
				status = update->SaveSessionInfo(context, infoType, &logonInfo);
			free(logonInfo.domain);
			free(logonInfo.username);
			break;

		case INFO_TYPE_LOGON_LONG:
			ZeroMemory(&logonInfo, sizeof(logonInfo));
			status = rdp_recv_logon_info_v2(rdp, s, &logonInfo);
			if (status && update->SaveSessionInfo)
				status = update->SaveSessionInfo(context, infoType, &logonInfo);
			free(logonInfo.domain);
			free(logonInfo.username);
			break;

		case INFO_TYPE_LOGON_PLAIN_NOTIFY:
			status = rdp_recv_logon_plain_notify(rdp, s);
			if (status && update->SaveSessionInfo)
				status = update->SaveSessionInfo(context, infoType, NULL);
			break;

		case INFO_TYPE_LOGON_EXTENDED_INF:
			ZeroMemory(&logonInfoEx, sizeof(logonInfoEx));
			status = rdp_recv_logon_info_extended(rdp, s, &logonInfoEx);
			if (status && update->SaveSessionInfo)
				status = update->SaveSessionInfo(context, infoType, &logonInfoEx);
			break;

		default:
			WLog_ERR(TAG, "Unhandled saveSessionInfo type 0x%x", infoType);
			status = TRUE;
			break;
	}

	if (!status)
	{
		WLog_DBG(TAG, "SaveSessionInfo error: infoType: %s (%d)",
				infoType < 4 ? INFO_TYPE_LOGON_STRINGS[infoType % 4] : "Unknown", infoType);
	}

	return status;
}

static BOOL rdp_write_logon_info_v1(wStream *s, logon_info *info)
{
	int sz = 4 + 52 + 4 + 512 + 4;
	int len;
	WCHAR *wString = NULL;

	if (!Stream_EnsureRemainingCapacity(s, sz))
		return FALSE;

	/* domain */
	len = ConvertToUnicode(CP_UTF8, 0, info->domain, -1, &wString, 0);
	if (len < 0)
		return FALSE;
	len *= 2;
	if (len > 52)
		return FALSE;

	Stream_Write_UINT32(s, len);
	Stream_Write(s, wString, len);
	Stream_Seek(s, 52 - len);
	free(wString);

	/* username */
	len = ConvertToUnicode(CP_UTF8, 0, info->username, -1, &wString, 0);
	if (len < 0)
		return FALSE;

	len *= 2;
	if (len > 512)
		return FALSE;

	Stream_Write_UINT32(s, len);
	Stream_Write(s, wString, len);
	Stream_Seek(s, 512 - len);
	free(wString);

	/* sessionId */
	Stream_Write_UINT32(s, info->sessionId);

	return TRUE;
}

static BOOL rdp_write_logon_info_v2(wStream *s, logon_info *info)
{
	int Size = 2 + 4 + 4 + 4 + 4 + 558;
	int domainLen, usernameLen, len;
	WCHAR *wString;

	if (!Stream_EnsureRemainingCapacity(s, Size))
		return FALSE;

	Stream_Write_UINT16(s, SAVE_SESSION_PDU_VERSION_ONE);
	Stream_Write_UINT32(s, Size);
	Stream_Write_UINT32(s, info->sessionId);

	domainLen = strlen(info->domain);
	Stream_Write_UINT32(s, (domainLen + 1) * 2);

	usernameLen = strlen(info->username);
	Stream_Write_UINT32(s, (usernameLen + 1) * 2);

	Stream_Seek(s, 558);

	len = ConvertToUnicode(CP_UTF8, 0, info->domain, -1, &wString, 0);
	if (len < 0)
		return FALSE;

	Stream_Write(s, wString, len * 2);
	free(wString);

	len = ConvertToUnicode(CP_UTF8, 0, info->username, -1, &wString, 0);
	if (len < 0)
		return FALSE;

	Stream_Write(s, wString, len * 2);
	free(wString);
	return TRUE;
}

static BOOL rdp_write_logon_info_plain(wStream *s)
{
	if (!Stream_EnsureRemainingCapacity(s, 576))
		return FALSE;

	Stream_Seek(s, 576);
	return TRUE;
}

static BOOL rdp_write_logon_info_ex(wStream *s, logon_info_ex *info)
{
	UINT32 FieldsPresent = 0;
	UINT16 Size = 2 + 4 + 570;

	if (info->haveCookie)
	{
		FieldsPresent |= LOGON_EX_AUTORECONNECTCOOKIE;
		Size += 28;
	}

	if (info->haveErrorInfo)
	{
		FieldsPresent |= LOGON_EX_LOGONERRORS;
		Size += 8;
	}

	if (!Stream_EnsureRemainingCapacity(s, Size))
		return FALSE;

	Stream_Write_UINT16(s, Size);
	Stream_Write_UINT32(s, FieldsPresent);

	if (info->haveCookie)
	{
		Stream_Write_UINT32(s, 28);							/* cbFieldData (4 bytes) */

		Stream_Write_UINT32(s, 28); 						/* cbLen (4 bytes) */
		Stream_Write_UINT32(s, AUTO_RECONNECT_VERSION_1); 	/* Version (4 bytes) */
		Stream_Write_UINT32(s, info->LogonId);				/* LogonId (4 bytes) */
		Stream_Write(s, info->ArcRandomBits, 16);			/* ArcRandomBits (16 bytes) */
	}

	if (info->haveErrorInfo)
	{
		Stream_Write_UINT32(s, 8);							/* cbFieldData (4 bytes) */

		Stream_Write_UINT32(s, info->ErrorNotificationType); /* ErrorNotificationType (4 bytes) */
		Stream_Write_UINT32(s, info->ErrorNotificationData); /* ErrorNotificationData (4 bytes) */
	}

	Stream_Seek(s, 570);
	return TRUE;
}

BOOL rdp_send_save_session_info(rdpContext *context, UINT32 type, void *data)
{
	wStream *s;
	BOOL status;
	rdpRdp *rdp = context->rdp;

	s = rdp_data_pdu_init(rdp);
	if (!s)
		return FALSE;

	Stream_Write_UINT32(s, type);

	switch (type)
	{
	case INFO_TYPE_LOGON:
		status = rdp_write_logon_info_v1(s, (logon_info *)data);
		break;
	case INFO_TYPE_LOGON_LONG:
		status = rdp_write_logon_info_v2(s, (logon_info *)data);
		break;
	case INFO_TYPE_LOGON_PLAIN_NOTIFY:
		status = rdp_write_logon_info_plain(s);
		break;
	case INFO_TYPE_LOGON_EXTENDED_INF:
		status = rdp_write_logon_info_ex(s, (logon_info_ex *)data);
		break;
	default:
		WLog_ERR(TAG, "saveSessionInfo type 0x%x not handled", type);
		status = FALSE;
		break;
	}

	if (status)
		status = rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SAVE_SESSION_INFO, rdp->mcs->userId);
	else
		Stream_Free(s, TRUE);
	return status;
}
