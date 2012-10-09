/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Client Info
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

#include <freerdp/utils/unicode.h>

#include "timezone.h"

#include "info.h"

#define INFO_TYPE_LOGON			0x00000000
#define INFO_TYPE_LOGON_LONG		0x00000001
#define INFO_TYPE_LOGON_PLAIN_NOTIFY	0x00000002
#define INFO_TYPE_LOGON_EXTENDED_INF	0x00000003

/*
static const char* const INFO_TYPE_LOGON_STRINGS[] =
{
	"Logon Info V1",
	"Logon Info V2",
	"Logon Plain Notify",
	"Logon Extended Info"
};
*/

/**
 * Read Server Auto Reconnect Cookie (ARC_SC_PRIVATE_PACKET).\n
 * @msdn{cc240540}
 * @param s stream
 * @param settings settings
 */

void rdp_read_server_auto_reconnect_cookie(STREAM* s, rdpSettings* settings)
{
	ARC_SC_PRIVATE_PACKET* autoReconnectCookie;
	autoReconnectCookie = settings->server_auto_reconnect_cookie;

	stream_read_UINT32(s, autoReconnectCookie->cbLen); /* cbLen (4 bytes) */
	stream_read_UINT32(s, autoReconnectCookie->version); /* version (4 bytes) */
	stream_read_UINT32(s, autoReconnectCookie->logonId); /* LogonId (4 bytes) */
	stream_read(s, autoReconnectCookie->arcRandomBits, 16); /* arcRandomBits (16 bytes) */
}

/**
 * Read Client Auto Reconnect Cookie (ARC_CS_PRIVATE_PACKET).\n
 * @msdn{cc240541}
 * @param s stream
 * @param settings settings
 */

BOOL rdp_read_client_auto_reconnect_cookie(STREAM* s, rdpSettings* settings)
{
	ARC_CS_PRIVATE_PACKET* autoReconnectCookie;
	autoReconnectCookie = settings->client_auto_reconnect_cookie;

	if (stream_get_left(s) < 28)
		return FALSE;

	stream_read_UINT32(s, autoReconnectCookie->cbLen); /* cbLen (4 bytes) */
	stream_read_UINT32(s, autoReconnectCookie->version); /* version (4 bytes) */
	stream_read_UINT32(s, autoReconnectCookie->logonId); /* LogonId (4 bytes) */
	stream_read(s, autoReconnectCookie->securityVerifier, 16); /* SecurityVerifier */

	return TRUE;
}

/**
 * Write Client Auto Reconnect Cookie (ARC_CS_PRIVATE_PACKET).\n
 * @msdn{cc240541}
 * @param s stream
 * @param settings settings
 */

void rdp_write_client_auto_reconnect_cookie(STREAM* s, rdpSettings* settings)
{
	ARC_CS_PRIVATE_PACKET* autoReconnectCookie;
	autoReconnectCookie = settings->client_auto_reconnect_cookie;

	stream_write_UINT32(s, autoReconnectCookie->cbLen); /* cbLen (4 bytes) */
	stream_write_UINT32(s, autoReconnectCookie->version); /* version (4 bytes) */
	stream_write_UINT32(s, autoReconnectCookie->logonId); /* LogonId (4 bytes) */
	stream_write(s, autoReconnectCookie->securityVerifier, 16); /* SecurityVerifier */
}

/**
 * Read Extended Info Packet (TS_EXTENDED_INFO_PACKET).\n
 * @msdn{cc240476}
 * @param s stream
 * @param settings settings
 */

BOOL rdp_read_extended_info_packet(STREAM* s, rdpSettings* settings)
{
	UINT16 clientAddressFamily;
	UINT16 cbClientAddress;
	UINT16 cbClientDir;
	UINT16 cbAutoReconnectLen;

	stream_read_UINT16(s, clientAddressFamily); /* clientAddressFamily */
	stream_read_UINT16(s, cbClientAddress); /* cbClientAddress */

	settings->ipv6 = (clientAddressFamily == ADDRESS_FAMILY_INET6 ? TRUE : FALSE);

	if (stream_get_left(s) < cbClientAddress)
		return FALSE;

	freerdp_UnicodeToAsciiAlloc((WCHAR*) stream_get_tail(s), &settings->ip_address, cbClientAddress / 2);
	stream_seek(s, cbClientAddress);

	stream_read_UINT16(s, cbClientDir); /* cbClientDir */

	if (stream_get_left(s) < cbClientDir)
		return FALSE;

	if (settings->client_dir)
		free(settings->client_dir);

	freerdp_UnicodeToAsciiAlloc((WCHAR*) stream_get_tail(s), &settings->client_dir, cbClientDir / 2);
	stream_seek(s, cbClientDir);

	if (!rdp_read_client_time_zone(s, settings))
		return FALSE;

	stream_seek_UINT32(s); /* clientSessionId, should be set to 0 */
	stream_read_UINT32(s, settings->performance_flags); /* performanceFlags */

	stream_read_UINT16(s, cbAutoReconnectLen); /* cbAutoReconnectLen */

	if (cbAutoReconnectLen > 0)
		return rdp_read_client_auto_reconnect_cookie(s, settings); /* autoReconnectCookie */

	/* reserved1 (2 bytes) */
	/* reserved2 (2 bytes) */

	return TRUE;
}

/**
 * Write Extended Info Packet (TS_EXTENDED_INFO_PACKET).\n
 * @msdn{cc240476}
 * @param s stream
 * @param settings settings
 */

void rdp_write_extended_info_packet(STREAM* s, rdpSettings* settings)
{
	int clientAddressFamily;
	WCHAR* clientAddress;
	int cbClientAddress;
	WCHAR* clientDir;
	int cbClientDir;
	int cbAutoReconnectLen;

	clientAddressFamily = settings->ipv6 ? ADDRESS_FAMILY_INET6 : ADDRESS_FAMILY_INET;

	cbClientAddress = freerdp_AsciiToUnicodeAlloc(settings->ip_address, &clientAddress, 0) * 2;

	cbClientDir = freerdp_AsciiToUnicodeAlloc(settings->client_dir, &clientDir, 0) * 2;

	cbAutoReconnectLen = (int) settings->client_auto_reconnect_cookie->cbLen;

	stream_write_UINT16(s, clientAddressFamily); /* clientAddressFamily */

	stream_write_UINT16(s, cbClientAddress + 2); /* cbClientAddress */

	if (cbClientAddress > 0)
		stream_write(s, clientAddress, cbClientAddress); /* clientAddress */
	stream_write_UINT16(s, 0);

	stream_write_UINT16(s, cbClientDir + 2); /* cbClientDir */

	if (cbClientDir > 0)
		stream_write(s, clientDir, cbClientDir); /* clientDir */
	stream_write_UINT16(s, 0);

	rdp_write_client_time_zone(s, settings); /* clientTimeZone */

	stream_write_UINT32(s, 0); /* clientSessionId, should be set to 0 */
	stream_write_UINT32(s, settings->performance_flags); /* performanceFlags */

	stream_write_UINT16(s, cbAutoReconnectLen); /* cbAutoReconnectLen */

	if (cbAutoReconnectLen > 0)
		rdp_write_client_auto_reconnect_cookie(s, settings); /* autoReconnectCookie */

	/* reserved1 (2 bytes) */
	/* reserved2 (2 bytes) */

	free(clientAddress);
	free(clientDir);
}

/**
 * Read Info Packet (TS_INFO_PACKET).\n
 * @msdn{cc240475}
 * @param s stream
 * @param settings settings
 */

BOOL rdp_read_info_packet(STREAM* s, rdpSettings* settings)
{
	UINT32 flags;
	UINT16 cbDomain;
	UINT16 cbUserName;
	UINT16 cbPassword;
	UINT16 cbAlternateShell;
	UINT16 cbWorkingDir;

	stream_seek_UINT32(s); /* CodePage */
	stream_read_UINT32(s, flags); /* flags */

	settings->autologon = ((flags & INFO_AUTOLOGON) ? TRUE : FALSE);
	settings->remote_app = ((flags & INFO_RAIL) ? TRUE : FALSE);
	settings->console_audio = ((flags & INFO_REMOTECONSOLEAUDIO) ? TRUE : FALSE);
	settings->compression = ((flags & INFO_COMPRESSION) ? TRUE : FALSE);

	stream_read_UINT16(s, cbDomain); /* cbDomain */
	stream_read_UINT16(s, cbUserName); /* cbUserName */
	stream_read_UINT16(s, cbPassword); /* cbPassword */
	stream_read_UINT16(s, cbAlternateShell); /* cbAlternateShell */
	stream_read_UINT16(s, cbWorkingDir); /* cbWorkingDir */

	if (stream_get_left(s) < cbDomain + 2)
		return FALSE;

	if (cbDomain > 0)
	{
		freerdp_UnicodeToAsciiAlloc((WCHAR*) stream_get_tail(s), &settings->domain, cbDomain / 2);
		stream_seek(s, cbDomain);
	}
	stream_seek(s, 2);

	if (stream_get_left(s) < cbUserName + 2)
		return FALSE;

	if (cbUserName > 0)
	{
		freerdp_UnicodeToAsciiAlloc((WCHAR*) stream_get_tail(s), &settings->username, cbUserName / 2);
		stream_seek(s, cbUserName);
	}
	stream_seek(s, 2);

	if (stream_get_left(s) < cbPassword + 2)
		return FALSE;

	if (cbPassword > 0)
	{
		freerdp_UnicodeToAsciiAlloc((WCHAR*) stream_get_tail(s), &settings->password, cbPassword / 2);
		stream_seek(s, cbPassword);
	}
	stream_seek(s, 2);

	if (stream_get_left(s) < cbAlternateShell + 2)
		return FALSE;

	if (cbAlternateShell > 0)
	{
		freerdp_UnicodeToAsciiAlloc((WCHAR*) stream_get_tail(s), &settings->shell, cbAlternateShell / 2);
		stream_seek(s, cbAlternateShell);
	}
	stream_seek(s, 2);

	if (stream_get_left(s) < cbWorkingDir + 2)
		return FALSE;

	if (cbWorkingDir > 0)
	{
		freerdp_UnicodeToAsciiAlloc((WCHAR*) stream_get_tail(s), &settings->directory, cbWorkingDir / 2);
		stream_seek(s, cbWorkingDir);
	}
	stream_seek(s, 2);

	if (settings->rdp_version >= 5)
		return rdp_read_extended_info_packet(s, settings); /* extraInfo */

	return TRUE;
}

/**
 * Write Info Packet (TS_INFO_PACKET).\n
 * @msdn{cc240475}
 * @param s stream
 * @param settings settings
 */

void rdp_write_info_packet(STREAM* s, rdpSettings* settings)
{
	UINT32 flags;
	WCHAR* domain;
	int cbDomain;
	WCHAR* userName;
	int cbUserName;
	WCHAR* password;
	int cbPassword;
	WCHAR* alternateShell;
	int cbAlternateShell;
	WCHAR* workingDir;
	int cbWorkingDir;
	BOOL usedPasswordCookie = FALSE;

	flags = INFO_MOUSE |
		INFO_UNICODE |
		INFO_LOGONERRORS |
		INFO_LOGONNOTIFY |
		INFO_MAXIMIZESHELL |
		INFO_ENABLEWINDOWSKEY |
		INFO_DISABLECTRLALTDEL;

	if (settings->audio_capture)
		flags |= RNS_INFO_AUDIOCAPTURE;

	if (!settings->audio_playback)
		flags |= INFO_NOAUDIOPLAYBACK;

	if (settings->autologon)
		flags |= INFO_AUTOLOGON;

	if (settings->remote_app)
		flags |= INFO_RAIL;

	if (settings->console_audio)
		flags |= INFO_REMOTECONSOLEAUDIO;

	if (settings->compression)
		flags |= INFO_COMPRESSION | INFO_PACKET_COMPR_TYPE_RDP6;

	if (settings->domain)
	{
		cbDomain = freerdp_AsciiToUnicodeAlloc(settings->domain, &domain, 0) * 2;
	}
	else
	{
		domain = NULL;
		cbDomain = 0;
	}

	cbUserName = freerdp_AsciiToUnicodeAlloc(settings->username, &userName, 0) * 2;

	if (settings->password_cookie && settings->password_cookie_length > 0)
	{
		usedPasswordCookie = TRUE;
		password = (WCHAR*) settings->password_cookie;
		cbPassword = settings->password_cookie_length - 2;	/* Strip double zero termination */
	}
	else
	{
		cbPassword = freerdp_AsciiToUnicodeAlloc(settings->password, &password, 0) * 2;
	}

	cbAlternateShell = freerdp_AsciiToUnicodeAlloc(settings->shell, &alternateShell, 0) * 2;

	cbWorkingDir = freerdp_AsciiToUnicodeAlloc(settings->directory, &workingDir, 0) * 2;

	stream_write_UINT32(s, 0); /* CodePage */
	stream_write_UINT32(s, flags); /* flags */

	stream_write_UINT16(s, cbDomain); /* cbDomain */
	stream_write_UINT16(s, cbUserName); /* cbUserName */
	stream_write_UINT16(s, cbPassword); /* cbPassword */
	stream_write_UINT16(s, cbAlternateShell); /* cbAlternateShell */
	stream_write_UINT16(s, cbWorkingDir); /* cbWorkingDir */

	if (cbDomain > 0)
		stream_write(s, domain, cbDomain);
	stream_write_UINT16(s, 0);

	if (cbUserName > 0)
		stream_write(s, userName, cbUserName);
	stream_write_UINT16(s, 0);

	if (cbPassword > 0)
		stream_write(s, password, cbPassword);
	stream_write_UINT16(s, 0);

	if (cbAlternateShell > 0)
		stream_write(s, alternateShell, cbAlternateShell);
	stream_write_UINT16(s, 0);

	if (cbWorkingDir > 0)
		stream_write(s, workingDir, cbWorkingDir);
	stream_write_UINT16(s, 0);

	free(domain);
	free(userName);
	free(alternateShell);
	free(workingDir);

	if (!usedPasswordCookie)
		free(password);

	if (settings->rdp_version >= 5)
		rdp_write_extended_info_packet(s, settings); /* extraInfo */
}

/**
 * Read Client Info PDU (CLIENT_INFO_PDU).\n
 * @msdn{cc240474}
 * @param rdp RDP module
 * @param s stream
 */

BOOL rdp_recv_client_info(rdpRdp* rdp, STREAM* s)
{
	UINT16 length;
	UINT16 channelId;
	UINT16 securityFlags;

	if (!rdp_read_header(rdp, s, &length, &channelId))
		return FALSE;

	rdp_read_security_header(s, &securityFlags);
	if ((securityFlags & SEC_INFO_PKT) == 0)
		return FALSE;

	if (rdp->settings->encryption)
	{
		if (securityFlags & SEC_REDIRECTION_PKT)
		{
			printf("Error: SEC_REDIRECTION_PKT unsupported\n");
			return FALSE;
		}
		if (securityFlags & SEC_ENCRYPT)
		{
			if (!rdp_decrypt(rdp, s, length - 4, securityFlags))
			{
				printf("rdp_decrypt failed\n");
				return FALSE;
			}
		}
	}

	return rdp_read_info_packet(s, rdp->settings);
}

/**
 * Send Client Info PDU (CLIENT_INFO_PDU).\n
 * @msdn{cc240474}
 * @param rdp RDP module
 */

BOOL rdp_send_client_info(rdpRdp* rdp)
{
	STREAM* s;

	//rdp->settings->crypt_flags |= SEC_INFO_PKT;
	rdp->sec_flags |= SEC_INFO_PKT;
	s = rdp_send_stream_init(rdp);
	rdp_write_info_packet(s, rdp->settings);
	return rdp_send(rdp, s, MCS_GLOBAL_CHANNEL_ID);
}

void rdp_recv_logon_info_v1(rdpRdp* rdp, STREAM* s)
{
	UINT32 cbDomain;
	UINT32 cbUserName;

	stream_read_UINT32(s, cbDomain); /* cbDomain (4 bytes) */
	stream_seek(s, 52); /* domain (52 bytes) */
	stream_read_UINT32(s, cbUserName); /* cbUserName (4 bytes) */
	stream_seek(s, 512); /* userName (512 bytes) */
	stream_seek_UINT32(s); /* sessionId (4 bytes) */
}

void rdp_recv_logon_info_v2(rdpRdp* rdp, STREAM* s)
{
	UINT32 cbDomain;
	UINT32 cbUserName;

	stream_seek_UINT16(s); /* version (2 bytes) */
	stream_seek_UINT32(s); /* size (4 bytes) */
	stream_seek_UINT32(s); /* sessionId (4 bytes) */
	stream_read_UINT32(s, cbDomain); /* cbDomain (4 bytes) */
	stream_read_UINT32(s, cbUserName); /* cbUserName (4 bytes) */
	stream_seek(s, 558); /* pad */
	stream_seek(s, cbDomain); /* domain */
	stream_seek(s, cbUserName); /* userName */
}

void rdp_recv_logon_plain_notify(rdpRdp* rdp, STREAM* s)
{
	stream_seek(s, 576); /* pad */
}

void rdp_recv_logon_error_info(rdpRdp* rdp, STREAM* s)
{
	UINT32 errorNotificationType;
	UINT32 errorNotificationData;

	stream_read_UINT32(s, errorNotificationType); /* errorNotificationType (4 bytes) */
	stream_read_UINT32(s, errorNotificationData); /* errorNotificationData (4 bytes) */
}

void rdp_recv_logon_info_extended(rdpRdp* rdp, STREAM* s)
{
	UINT32 cbFieldData;
	UINT32 fieldsPresent;
	UINT16 Length;

	stream_read_UINT16(s, Length); /* The total size in bytes of this structure */
	stream_read_UINT32(s, fieldsPresent); /* fieldsPresent (4 bytes) */

	/* logonFields */

	if (fieldsPresent & LOGON_EX_AUTORECONNECTCOOKIE)
	{
		stream_read_UINT32(s, cbFieldData); /* cbFieldData (4 bytes) */
		rdp_read_server_auto_reconnect_cookie(s, rdp->settings);
	}

	if (fieldsPresent & LOGON_EX_LOGONERRORS)
	{
		stream_read_UINT32(s, cbFieldData); /* cbFieldData (4 bytes) */
		rdp_recv_logon_error_info(rdp, s);
	}

	stream_seek(s, 570); /* pad */
}

BOOL rdp_recv_save_session_info(rdpRdp* rdp, STREAM* s)
{
	UINT32 infoType;

	stream_read_UINT32(s, infoType); /* infoType (4 bytes) */

	//printf("%s\n", INFO_TYPE_LOGON_STRINGS[infoType]);

	switch (infoType)
	{
		case INFO_TYPE_LOGON:
			rdp_recv_logon_info_v1(rdp, s);
			break;

		case INFO_TYPE_LOGON_LONG:
			rdp_recv_logon_info_v2(rdp, s);
			break;

		case INFO_TYPE_LOGON_PLAIN_NOTIFY:
			rdp_recv_logon_plain_notify(rdp, s);
			break;

		case INFO_TYPE_LOGON_EXTENDED_INF:
			rdp_recv_logon_info_extended(rdp, s);
			break;

		default:
			break;
	}

	return TRUE;
}

