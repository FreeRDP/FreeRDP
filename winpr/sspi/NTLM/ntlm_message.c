/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package (Message)
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "ntlm.h"
#include "../sspi.h"

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>

#include "ntlm_compute.h"

#include "ntlm_message.h"

static const char NTLM_SIGNATURE[] = "NTLMSSP";

static const char* const NTLM_NEGOTIATE_STRINGS[] =
{
	"NTLMSSP_NEGOTIATE_56",
	"NTLMSSP_NEGOTIATE_KEY_EXCH",
	"NTLMSSP_NEGOTIATE_128",
	"NTLMSSP_RESERVED1",
	"NTLMSSP_RESERVED2",
	"NTLMSSP_RESERVED3",
	"NTLMSSP_NEGOTIATE_VERSION",
	"NTLMSSP_RESERVED4",
	"NTLMSSP_NEGOTIATE_TARGET_INFO",
	"NTLMSSP_REQUEST_NON_NT_SESSION_KEY",
	"NTLMSSP_RESERVED5",
	"NTLMSSP_NEGOTIATE_IDENTIFY",
	"NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY",
	"NTLMSSP_RESERVED6",
	"NTLMSSP_TARGET_TYPE_SERVER",
	"NTLMSSP_TARGET_TYPE_DOMAIN",
	"NTLMSSP_NEGOTIATE_ALWAYS_SIGN",
	"NTLMSSP_RESERVED7",
	"NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED",
	"NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED",
	"NTLMSSP_NEGOTIATE_ANONYMOUS",
	"NTLMSSP_RESERVED8",
	"NTLMSSP_NEGOTIATE_NTLM",
	"NTLMSSP_RESERVED9",
	"NTLMSSP_NEGOTIATE_LM_KEY",
	"NTLMSSP_NEGOTIATE_DATAGRAM",
	"NTLMSSP_NEGOTIATE_SEAL",
	"NTLMSSP_NEGOTIATE_SIGN",
	"NTLMSSP_RESERVED10",
	"NTLMSSP_REQUEST_TARGET",
	"NTLMSSP_NEGOTIATE_OEM",
	"NTLMSSP_NEGOTIATE_UNICODE"
};

void ntlm_print_negotiate_flags(UINT32 flags)
{
	int i;
	const char* str;

	printf("negotiateFlags \"0x%08X\"{\n", flags);

	for (i = 31; i >= 0; i--)
	{
		if ((flags >> i) & 1)
		{
			str = NTLM_NEGOTIATE_STRINGS[(31 - i)];
			printf("\t%s (%d),\n", str, (31 - i));
		}
	}

	printf("}\n");
}

SECURITY_STATUS ntlm_read_NegotiateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	PStream s;
	int length;
	NTLM_NEGOTIATE_MESSAGE message;

	ZeroMemory(&message, sizeof(message));
	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	StreamRead(s, message.Signature, 8);
	StreamRead_UINT32(s, message.MessageType);

	if (memcmp(message.Signature, NTLM_SIGNATURE, 8) != 0)
	{
		printf("Unexpected NTLM signature: %s, expected:%s\n", message.Signature, NTLM_SIGNATURE);
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	if (message.MessageType != MESSAGE_TYPE_NEGOTIATE)
	{
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	StreamRead_UINT32(s, message.NegotiateFlags); /* NegotiateFlags (4 bytes) */

	context->NegotiateFlags = message.NegotiateFlags;

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	StreamRead_UINT16(s, message.DomainName.Len); /* DomainNameLen */
	StreamRead_UINT16(s, message.DomainName.MaxLen); /* DomainNameMaxLen */
	StreamRead_UINT32(s, message.DomainName.BufferOffset); /* DomainNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	StreamRead_UINT16(s, message.Workstation.Len); /* WorkstationLen */
	StreamRead_UINT16(s, message.Workstation.MaxLen); /* WorkstationMaxLen */
	StreamRead_UINT32(s, message.Workstation.BufferOffset); /* WorkstationBufferOffset */

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		/* Only present if NTLMSSP_NEGOTIATE_VERSION is set */
		StreamSeek(s, 8); /* Version (8 bytes) */
	}

	length = StreamSize(s);
	buffer->cbBuffer = length;

	sspi_SecBufferAlloc(&context->NegotiateMessage, length);
	CopyMemory(context->NegotiateMessage.pvBuffer, buffer->pvBuffer, buffer->cbBuffer);
	context->NegotiateMessage.BufferType = buffer->BufferType;

#ifdef WITH_DEBUG_NTLM
	printf("NEGOTIATE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(s->data, length);
	printf("\n");
#endif

	context->state = NTLM_STATE_CHALLENGE;

	PStreamFreeDetach(s);

	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_write_NegotiateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	PStream s;
	int length;
	NTLM_NEGOTIATE_MESSAGE message;

	ZeroMemory(&message, sizeof(message));
	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	StreamWrite(s, NTLM_SIGNATURE, 8); /* Signature (8 bytes) */
	StreamWrite_UINT32(s, MESSAGE_TYPE_NEGOTIATE); /* MessageType */

	if (context->ntlm_v2)
	{
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_56;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_128;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_LM_KEY;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		message.NegotiateFlags |= NTLMSSP_REQUEST_TARGET;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_OEM;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}
	else
	{
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_128;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		message.NegotiateFlags |= NTLMSSP_REQUEST_TARGET;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}

	if (context->confidentiality)
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;

	if (context->SendVersionInfo)
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;

	context->NegotiateFlags = message.NegotiateFlags;

	StreamWrite_UINT32(s, message.NegotiateFlags); /* NegotiateFlags (4 bytes) */

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	StreamWrite_UINT16(s, 0); /* DomainNameLen */
	StreamWrite_UINT16(s, 0); /* DomainNameMaxLen */
	StreamWrite_UINT32(s, 0); /* DomainNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	StreamWrite_UINT16(s, 0); /* WorkstationLen */
	StreamWrite_UINT16(s, 0); /* WorkstationMaxLen */
	StreamWrite_UINT32(s, 0); /* WorkstationBufferOffset */

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		/* Only present if NTLMSSP_NEGOTIATE_VERSION is set */

		ntlm_get_version_info(&(message.Version));
		ntlm_write_version_info(s, &(message.Version));

#ifdef WITH_DEBUG_NTLM
		printf("Version (length = 8)\n");
		winpr_HexDump((s->p - 8), 8);
		printf("\n");
#endif
	}

	length = StreamSize(s);
	buffer->cbBuffer = length;

	sspi_SecBufferAlloc(&context->NegotiateMessage, length);
	CopyMemory(context->NegotiateMessage.pvBuffer, buffer->pvBuffer, buffer->cbBuffer);
	context->NegotiateMessage.BufferType = buffer->BufferType;

#ifdef WITH_DEBUG_NTLM
	printf("NEGOTIATE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(s->data, length);
	printf("\n");
#endif

	context->state = NTLM_STATE_CHALLENGE;

	PStreamFreeDetach(s);

	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_read_ChallengeMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	BYTE* p;
	PStream s;
	int length;
	PBYTE StartOffset;
	PBYTE PayloadOffset;
	NTLM_CHALLENGE_MESSAGE message;

	ntlm_generate_client_challenge(context);

	ZeroMemory(&message, sizeof(message));
	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	StreamRead(s, message.Signature, 8);
	StreamRead_UINT32(s, message.MessageType);

	if (memcmp(message.Signature, NTLM_SIGNATURE, 8) != 0)
	{
		printf("Unexpected NTLM signature: %s, expected:%s\n", message.Signature, NTLM_SIGNATURE);
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	if (message.MessageType != MESSAGE_TYPE_CHALLENGE)
	{
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	StartOffset = StreamGetPointer(s) - 12;

	/* TargetNameFields (8 bytes) */
	StreamRead_UINT16(s, message.TargetName.Len); /* TargetNameLen (2 bytes) */
	StreamRead_UINT16(s, message.TargetName.MaxLen); /* TargetNameMaxLen (2 bytes) */
	StreamRead_UINT32(s, message.TargetName.BufferOffset); /* TargetNameBufferOffset (4 bytes) */

	StreamRead_UINT32(s, context->NegotiateFlags); /* NegotiateFlags (4 bytes) */

#ifdef WITH_DEBUG_NTLM
	ntlm_print_negotiate_flags(context->NegotiateFlags);
#endif

	StreamRead(s, context->ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	StreamSeek(s, 8); /* Reserved (8 bytes), should be ignored */

	/* TargetInfoFields (8 bytes) */
	StreamRead_UINT16(s, message.TargetInfo.Len); /* TargetInfoLen (2 bytes) */
	StreamRead_UINT16(s, message.TargetInfo.MaxLen); /* TargetInfoMaxLen (2 bytes) */
	StreamRead_UINT32(s, message.TargetInfo.BufferOffset); /* TargetInfoBufferOffset (4 bytes) */

	/* only present if NTLMSSP_NEGOTIATE_VERSION is set */

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		StreamSeek(s, 8); /* Version (8 bytes), can be ignored */
	}

	/* Payload (variable) */
	PayloadOffset = StreamGetPointer(s);

	if (message.TargetName.Len > 0)
	{
		p = StartOffset + message.TargetName.BufferOffset;
		sspi_SecBufferAlloc(&context->TargetName, message.TargetName.Len);
		CopyMemory(context->TargetName.pvBuffer, p, message.TargetName.Len);

#ifdef WITH_DEBUG_NTLM
		printf("TargetName (length = %d, offset = %d)\n", message.TargetName.Len, message.TargetName.BufferOffset);
		winpr_HexDump(context->TargetName.pvBuffer, context->TargetName.cbBuffer);
		printf("\n");
#endif
	}

	if (message.TargetInfo.Len > 0)
	{
		p = StartOffset + message.TargetInfo.BufferOffset;
		sspi_SecBufferAlloc(&context->TargetInfo, message.TargetInfo.Len);
		CopyMemory(context->TargetInfo.pvBuffer, p, message.TargetInfo.Len);

#ifdef WITH_DEBUG_NTLM
		printf("TargetInfo (length = %d, offset = %d)\n", message.TargetInfo.Len, message.TargetInfo.BufferOffset);
		winpr_HexDump(context->TargetInfo.pvBuffer, context->TargetInfo.cbBuffer);
		printf("\n");
#endif

		if (context->ntlm_v2)
		{
			StreamSetPointer(s, p);
			ntlm_input_av_pairs(context, s);
		}
	}

	length = (PayloadOffset - StartOffset) + message.TargetName.Len + message.TargetInfo.Len;

	sspi_SecBufferAlloc(&context->ChallengeMessage, length);
	CopyMemory(context->ChallengeMessage.pvBuffer, StartOffset, length);

#ifdef WITH_DEBUG_NTLM
	printf("CHALLENGE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(context->ChallengeMessage.pvBuffer, context->ChallengeMessage.cbBuffer);
	printf("\n");
#endif

	/* AV_PAIRs */
	if (context->ntlm_v2)
		ntlm_populate_av_pairs(context);

	/* Timestamp */
	ntlm_generate_timestamp(context);

	/* LmChallengeResponse */

	if (context->LmCompatibilityLevel < 2)
		ntlm_compute_lm_v2_response(context);

	/* NtChallengeResponse */
	ntlm_compute_ntlm_v2_response(context);

	/* KeyExchangeKey */
	ntlm_generate_key_exchange_key(context);

	/* RandomSessionKey */
	ntlm_generate_random_session_key(context);

	/* ExportedSessionKey */
	ntlm_generate_exported_session_key(context);

	/* EncryptedRandomSessionKey */
	ntlm_encrypt_random_session_key(context);

	/* Generate signing keys */
	ntlm_generate_client_signing_key(context);
	ntlm_generate_server_signing_key(context);

	/* Generate sealing keys */
	ntlm_generate_client_sealing_key(context);
	ntlm_generate_server_sealing_key(context);

	/* Initialize RC4 seal state using client sealing key */
	ntlm_init_rc4_seal_states(context);

#ifdef WITH_DEBUG_NTLM
	printf("ClientChallenge\n");
	winpr_HexDump(context->ClientChallenge, 8);
	printf("\n");

	printf("ServerChallenge\n");
	winpr_HexDump(context->ServerChallenge, 8);
	printf("\n");

	printf("SessionBaseKey\n");
	winpr_HexDump(context->SessionBaseKey, 16);
	printf("\n");

	printf("KeyExchangeKey\n");
	winpr_HexDump(context->KeyExchangeKey, 16);
	printf("\n");

	printf("ExportedSessionKey\n");
	winpr_HexDump(context->ExportedSessionKey, 16);
	printf("\n");

	printf("RandomSessionKey\n");
	winpr_HexDump(context->RandomSessionKey, 16);
	printf("\n");

	printf("ClientSigningKey\n");
	winpr_HexDump(context->ClientSigningKey, 16);
	printf("\n");

	printf("ClientSealingKey\n");
	winpr_HexDump(context->ClientSealingKey, 16);
	printf("\n");

	printf("ServerSigningKey\n");
	winpr_HexDump(context->ServerSigningKey, 16);
	printf("\n");

	printf("ServerSealingKey\n");
	winpr_HexDump(context->ServerSealingKey, 16);
	printf("\n");

	printf("Timestamp\n");
	winpr_HexDump(context->Timestamp, 8);
	printf("\n");
#endif

	context->state = NTLM_STATE_AUTHENTICATE;

	PStreamFreeDetach(s);

	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_write_ChallengeMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	PStream s;
	int length;
	UINT32 PayloadOffset;
	NTLM_CHALLENGE_MESSAGE message;

	/* Server Challenge */
	ntlm_generate_server_challenge(context);

	/* Timestamp */
	ntlm_generate_timestamp(context);

	/* TargetInfo */
	ntlm_populate_server_av_pairs(context);

	ZeroMemory(&message, sizeof(message));
	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	StreamWrite(s, NTLM_SIGNATURE, 8); /* Signature (8 bytes) */
	StreamWrite_UINT32(s, MESSAGE_TYPE_CHALLENGE); /* MessageType */

	if (context->NegotiateFlags & NTLMSSP_REQUEST_TARGET)
	{
		message.TargetName.Len = (UINT16) context->TargetName.cbBuffer;
		message.TargetName.Buffer = context->TargetName.pvBuffer;
	}
	else
	{
		message.TargetName.Len = 0;
		message.TargetName.Buffer = NULL;
	}

	context->NegotiateFlags |= NTLMSSP_NEGOTIATE_TARGET_INFO;

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_TARGET_INFO)
	{
		message.TargetInfo.Len = (UINT16) context->TargetInfo.cbBuffer;
		message.TargetInfo.Buffer = context->TargetInfo.pvBuffer;
	}
	else
	{
		message.TargetInfo.Len = 0;
		message.TargetInfo.Buffer = NULL;
	}

	PayloadOffset = 48;

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		PayloadOffset += 8;

	message.TargetName.BufferOffset = PayloadOffset;
	message.TargetInfo.BufferOffset = message.TargetName.BufferOffset + message.TargetName.Len;

	/* TargetNameFields (8 bytes) */
	StreamWrite_UINT16(s, message.TargetName.Len); /* TargetNameLen (2 bytes) */
	StreamWrite_UINT16(s, message.TargetName.Len); /* TargetNameMaxLen (2 bytes) */
	StreamWrite_UINT32(s, message.TargetName.BufferOffset); /* TargetNameBufferOffset (4 bytes) */

	StreamWrite_UINT32(s, context->NegotiateFlags); /* NegotiateFlags (4 bytes) */

	StreamWrite(s, context->ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	StreamZero(s, 8); /* Reserved (8 bytes), should be ignored */

	/* TargetInfoFields (8 bytes) */
	StreamWrite_UINT16(s, message.TargetInfo.Len); /* TargetInfoLen (2 bytes) */
	StreamWrite_UINT16(s, message.TargetInfo.Len); /* TargetInfoMaxLen (2 bytes) */
	StreamWrite_UINT32(s, message.TargetInfo.BufferOffset); /* TargetInfoBufferOffset (4 bytes) */

	/* only present if NTLMSSP_NEGOTIATE_VERSION is set */

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		ntlm_get_version_info(&(message.Version));
		ntlm_write_version_info(s, &(message.Version)); /* Version (8 bytes), can be ignored */
	}

	/* Payload (variable) */

	if (message.TargetName.Len > 0)
	{
		StreamWrite(s, message.TargetName.Buffer, message.TargetName.Len);
#ifdef WITH_DEBUG_NTLM
		printf("TargetName (length = %d, offset = %d)\n", message.TargetName.Len, message.TargetName.BufferOffset);
		winpr_HexDump(message.TargetName.Buffer, message.TargetName.Len);
		printf("\n");
#endif
	}

	if (message.TargetInfo.Len > 0)
	{
		StreamWrite(s, message.TargetInfo.Buffer, message.TargetInfo.Len);
#ifdef WITH_DEBUG_NTLM
		printf("TargetInfo (length = %d, offset = %d)\n", message.TargetInfo.Len, message.TargetInfo.BufferOffset);
		winpr_HexDump(message.TargetInfo.Buffer, message.TargetInfo.Len);
		printf("\n");
#endif
	}

	length = StreamSize(s);
	buffer->cbBuffer = length;

	sspi_SecBufferAlloc(&context->ChallengeMessage, length);
	CopyMemory(context->ChallengeMessage.pvBuffer, s->data, length);

#ifdef WITH_DEBUG_NTLM
	printf("CHALLENGE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(context->ChallengeMessage.pvBuffer, context->ChallengeMessage.cbBuffer);
	printf("\n");
#endif

	context->state = NTLM_STATE_AUTHENTICATE;

	PStreamFreeDetach(s);

	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_read_AuthenticateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	PStream s;
	int length;
	NTLM_AUTHENTICATE_MESSAGE message;

	ZeroMemory(&message, sizeof(message));
	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	StreamRead(s, message.Signature, 8);
	StreamRead_UINT32(s, message.MessageType);

	if (memcmp(message.Signature, NTLM_SIGNATURE, 8) != 0)
	{
		printf("Unexpected NTLM signature: %s, expected:%s\n", message.Signature, NTLM_SIGNATURE);
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	if (message.MessageType != MESSAGE_TYPE_AUTHENTICATE)
	{
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	/* LmChallengeResponseFields (8 bytes) */
	StreamRead_UINT16(s, message.LmChallengeResponse.Len); /* LmChallengeResponseLen */
	StreamRead_UINT16(s, message.LmChallengeResponse.MaxLen); /* LmChallengeResponseMaxLen */
	StreamRead_UINT32(s, message.LmChallengeResponse.BufferOffset); /* LmChallengeResponseBufferOffset */

	/* NtChallengeResponseFields (8 bytes) */
	StreamRead_UINT16(s, message.NtChallengeResponse.Len); /* NtChallengeResponseLen */
	StreamRead_UINT16(s, message.NtChallengeResponse.MaxLen); /* NtChallengeResponseMaxLen */
	StreamRead_UINT32(s, message.NtChallengeResponse.BufferOffset); /* NtChallengeResponseBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	StreamRead_UINT16(s, message.DomainName.Len); /* DomainNameLen */
	StreamRead_UINT16(s, message.DomainName.MaxLen); /* DomainNameMaxLen */
	StreamRead_UINT32(s, message.DomainName.BufferOffset); /* DomainNameBufferOffset */

	/* UserNameFields (8 bytes) */
	StreamRead_UINT16(s, message.UserName.Len); /* UserNameLen */
	StreamRead_UINT16(s, message.UserName.MaxLen); /* UserNameMaxLen */
	StreamRead_UINT32(s, message.UserName.BufferOffset); /* UserNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	StreamRead_UINT16(s, message.Workstation.Len); /* WorkstationLen */
	StreamRead_UINT16(s, message.Workstation.MaxLen); /* WorkstationMaxLen */
	StreamRead_UINT32(s, message.Workstation.BufferOffset); /* WorkstationBufferOffset */

	/* EncryptedRandomSessionKeyFields (8 bytes) */
	StreamRead_UINT16(s, message.EncryptedRandomSessionKey.Len); /* EncryptedRandomSessionKeyLen */
	StreamRead_UINT16(s, message.EncryptedRandomSessionKey.MaxLen); /* EncryptedRandomSessionKeyMaxLen */
	StreamRead_UINT32(s, message.EncryptedRandomSessionKey.BufferOffset); /* EncryptedRandomSessionKeyBufferOffset */

	StreamRead_UINT32(s, message.NegotiateFlags); /* NegotiateFlags (4 bytes) */

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		/* Only present if NTLMSSP_NEGOTIATE_VERSION is set */

#ifdef WITH_DEBUG_NTLM
		printf("Version (length = 8)\n");
		winpr_HexDump(s->p, 8);
		printf("\n");
#endif

		StreamSeek(s, 8); /* Version (8 bytes) */
	}

	length = StreamSize(s);
	sspi_SecBufferAlloc(&context->AuthenticateMessage, length);
	CopyMemory(context->AuthenticateMessage.pvBuffer, s->data, length);
	buffer->cbBuffer = length;

#ifdef WITH_DEBUG_NTLM
	printf("AUTHENTICATE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(s->data, length);
	printf("\n");
#endif

	/* DomainName */
	if (message.DomainName.Len > 0)
	{
		message.DomainName.Buffer = s->data + message.DomainName.BufferOffset;
#ifdef WITH_DEBUG_NTLM
		printf("DomainName (length = %d, offset = %d)\n", message.DomainName.Len, message.DomainName.BufferOffset);
		winpr_HexDump(message.DomainName.Buffer, message.DomainName.Len);
		printf("\n");
#endif
	}

	/* UserName */
	if (message.UserName.Len > 0)
	{
		message.UserName.Buffer = s->data + message.UserName.BufferOffset;
#ifdef WITH_DEBUG_NTLM
		printf("UserName (length = %d, offset = %d)\n", message.UserName.Len, message.UserName.BufferOffset);
		winpr_HexDump(message.UserName.Buffer, message.UserName.Len);
		printf("\n");
#endif
	}

	/* Workstation */
	if (message.Workstation.Len > 0)
	{
		message.Workstation.Buffer = s->data + message.Workstation.BufferOffset;
#ifdef WITH_DEBUG_NTLM
		printf("Workstation (length = %d, offset = %d)\n", message.Workstation.Len, message.Workstation.BufferOffset);
		winpr_HexDump(message.Workstation.Buffer, message.Workstation.Len);
		printf("\n");
#endif
	}

	/* LmChallengeResponse */
	if (message.LmChallengeResponse.Len > 0)
	{
		message.LmChallengeResponse.Buffer = s->data + message.LmChallengeResponse.BufferOffset;
#ifdef WITH_DEBUG_NTLM
		printf("LmChallengeResponse (length = %d, offset = %d)\n", message.LmChallengeResponse.Len, message.LmChallengeResponse.BufferOffset);
		winpr_HexDump(message.LmChallengeResponse.Buffer, message.LmChallengeResponse.Len);
		printf("\n");
#endif
	}

	/* NtChallengeResponse */
	if (message.NtChallengeResponse.Len > 0)
	{
		BYTE* ClientChallengeBuffer;

		message.NtChallengeResponse.Buffer = s->data + message.NtChallengeResponse.BufferOffset;

		ClientChallengeBuffer = message.NtChallengeResponse.Buffer + 32;
		CopyMemory(context->ClientChallenge, ClientChallengeBuffer, 8);

#ifdef WITH_DEBUG_NTLM
		printf("NtChallengeResponse (length = %d, offset = %d)\n", message.NtChallengeResponse.Len, message.NtChallengeResponse.BufferOffset);
		winpr_HexDump(message.NtChallengeResponse.Buffer, message.NtChallengeResponse.Len);
		printf("\n");
#endif
	}

	/* EncryptedRandomSessionKey */
	if (message.EncryptedRandomSessionKey.Len > 0)
	{
		message.EncryptedRandomSessionKey.Buffer = s->data + message.EncryptedRandomSessionKey.BufferOffset;
		CopyMemory(context->EncryptedRandomSessionKey, message.EncryptedRandomSessionKey.Buffer, 16);

#ifdef WITH_DEBUG_NTLM
		printf("EncryptedRandomSessionKey (length = %d, offset = %d)\n", message.EncryptedRandomSessionKey.Len, message.EncryptedRandomSessionKey.BufferOffset);
		winpr_HexDump(message.EncryptedRandomSessionKey.Buffer, message.EncryptedRandomSessionKey.Len);
		printf("\n");
#endif
	}

	if (message.UserName.Len > 0)
	{
		context->identity.User = (UINT16*) malloc(message.UserName.Len);
		CopyMemory(context->identity.User, message.UserName.Buffer, message.UserName.Len);
		context->identity.UserLength = message.UserName.Len;
	}

	if (message.DomainName.Len > 0)
	{
		context->identity.Domain = (UINT16*) malloc(message.DomainName.Len);
		CopyMemory(context->identity.Domain, message.DomainName.Buffer, message.DomainName.Len);
		context->identity.DomainLength = message.DomainName.Len;
	}

	/* LmChallengeResponse */

	if (context->LmCompatibilityLevel < 2)
		ntlm_compute_lm_v2_response(context);

	/* NtChallengeResponse */
	ntlm_compute_ntlm_v2_response(context);

	/* KeyExchangeKey */
	ntlm_generate_key_exchange_key(context);

	/* EncryptedRandomSessionKey */
	ntlm_decrypt_random_session_key(context);

	/* ExportedSessionKey */
	ntlm_generate_exported_session_key(context);

	/* Generate signing keys */
	ntlm_generate_client_signing_key(context);
	ntlm_generate_server_signing_key(context);

	/* Generate sealing keys */
	ntlm_generate_client_sealing_key(context);
	ntlm_generate_server_sealing_key(context);

	/* Initialize RC4 seal state */
	ntlm_init_rc4_seal_states(context);

#ifdef WITH_DEBUG_NTLM
	printf("ClientChallenge\n");
	winpr_HexDump(context->ClientChallenge, 8);
	printf("\n");

	printf("ServerChallenge\n");
	winpr_HexDump(context->ServerChallenge, 8);
	printf("\n");

	printf("SessionBaseKey\n");
	winpr_HexDump(context->SessionBaseKey, 16);
	printf("\n");

	printf("KeyExchangeKey\n");
	winpr_HexDump(context->KeyExchangeKey, 16);
	printf("\n");

	printf("ExportedSessionKey\n");
	winpr_HexDump(context->ExportedSessionKey, 16);
	printf("\n");

	printf("RandomSessionKey\n");
	winpr_HexDump(context->RandomSessionKey, 16);
	printf("\n");

	printf("ClientSigningKey\n");
	winpr_HexDump(context->ClientSigningKey, 16);
	printf("\n");

	printf("ClientSealingKey\n");
	winpr_HexDump(context->ClientSealingKey, 16);
	printf("\n");

	printf("ServerSigningKey\n");
	winpr_HexDump(context->ServerSigningKey, 16);
	printf("\n");

	printf("ServerSealingKey\n");
	winpr_HexDump(context->ServerSealingKey, 16);
	printf("\n");

	printf("Timestamp\n");
	winpr_HexDump(context->Timestamp, 8);
	printf("\n");
#endif

	context->state = NTLM_STATE_FINAL;

	PStreamFreeDetach(s);

	return SEC_I_COMPLETE_NEEDED;
}

/**
 * Send NTLMSSP AUTHENTICATE_MESSAGE.\n
 * AUTHENTICATE_MESSAGE @msdn{cc236643}
 * @param NTLM context
 * @param buffer
 */

SECURITY_STATUS ntlm_write_AuthenticateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	PStream s;
	int length;
	BYTE* MicOffset = NULL;
	UINT32 PayloadBufferOffset;
	NTLM_AUTHENTICATE_MESSAGE message;

	ZeroMemory(&message, sizeof(message));
	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	message.Workstation.Len = context->WorkstationLength;
	message.Workstation.Buffer = (BYTE*) context->Workstation;

	if (context->ntlm_v2 < 1)
		message.Workstation.Len = 0;

	message.DomainName.Len = (UINT16) context->identity.DomainLength * 2;
	message.DomainName.Buffer = (BYTE*) context->identity.Domain;

	message.UserName.Len = (UINT16) context->identity.UserLength * 2;
	message.UserName.Buffer = (BYTE*) context->identity.User;

	message.LmChallengeResponse.Len = (UINT16) 24;
	message.NtChallengeResponse.Len = (UINT16) context->NtChallengeResponse.cbBuffer;

	message.EncryptedRandomSessionKey.Len = 16;
	message.EncryptedRandomSessionKey.Buffer = context->EncryptedRandomSessionKey;

	if (context->ntlm_v2)
	{
		/* observed: 35 82 88 e2 (0xE2888235) */
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_56;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_128;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_TARGET_INFO;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		message.NegotiateFlags |= NTLMSSP_REQUEST_TARGET;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}
	else
	{
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_128;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		message.NegotiateFlags |= NTLMSSP_REQUEST_TARGET;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}

	if (context->confidentiality)
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;

	if (context->ntlm_v2)
		PayloadBufferOffset = 80; /* starting buffer offset */
	else
		PayloadBufferOffset = 64; /* starting buffer offset */

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		PayloadBufferOffset += 8;

	message.DomainName.BufferOffset = PayloadBufferOffset;
	message.UserName.BufferOffset = message.DomainName.BufferOffset + message.DomainName.Len;
	message.Workstation.BufferOffset = message.UserName.BufferOffset + message.UserName.Len;
	message.LmChallengeResponse.BufferOffset = message.Workstation.BufferOffset + message.Workstation.Len;
	message.NtChallengeResponse.BufferOffset = message.LmChallengeResponse.BufferOffset + message.LmChallengeResponse.Len;
	message.EncryptedRandomSessionKey.BufferOffset = message.NtChallengeResponse.BufferOffset + message.NtChallengeResponse.Len;

	StreamWrite(s, NTLM_SIGNATURE, 8); /* Signature (8 bytes) */
	StreamWrite_UINT32(s, MESSAGE_TYPE_AUTHENTICATE); /* MessageType */

	/* LmChallengeResponseFields (8 bytes) */
	StreamWrite_UINT16(s, message.LmChallengeResponse.Len); /* LmChallengeResponseLen */
	StreamWrite_UINT16(s, message.LmChallengeResponse.Len); /* LmChallengeResponseMaxLen */
	StreamWrite_UINT32(s, message.LmChallengeResponse.BufferOffset); /* LmChallengeResponseBufferOffset */

	/* NtChallengeResponseFields (8 bytes) */
	StreamWrite_UINT16(s, message.NtChallengeResponse.Len); /* NtChallengeResponseLen */
	StreamWrite_UINT16(s, message.NtChallengeResponse.Len); /* NtChallengeResponseMaxLen */
	StreamWrite_UINT32(s, message.NtChallengeResponse.BufferOffset); /* NtChallengeResponseBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	StreamWrite_UINT16(s, message.DomainName.Len); /* DomainNameLen */
	StreamWrite_UINT16(s, message.DomainName.Len); /* DomainNameMaxLen */
	StreamWrite_UINT32(s, message.DomainName.BufferOffset); /* DomainNameBufferOffset */

	/* UserNameFields (8 bytes) */
	StreamWrite_UINT16(s, message.UserName.Len); /* UserNameLen */
	StreamWrite_UINT16(s, message.UserName.Len); /* UserNameMaxLen */
	StreamWrite_UINT32(s, message.UserName.BufferOffset); /* UserNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	StreamWrite_UINT16(s, message.Workstation.Len); /* WorkstationLen */
	StreamWrite_UINT16(s, message.Workstation.Len); /* WorkstationMaxLen */
	StreamWrite_UINT32(s, message.Workstation.BufferOffset); /* WorkstationBufferOffset */

	/* EncryptedRandomSessionKeyFields (8 bytes) */
	StreamWrite_UINT16(s, message.EncryptedRandomSessionKey.Len); /* EncryptedRandomSessionKeyLen */
	StreamWrite_UINT16(s, message.EncryptedRandomSessionKey.Len); /* EncryptedRandomSessionKeyMaxLen */
	StreamWrite_UINT32(s, message.EncryptedRandomSessionKey.BufferOffset); /* EncryptedRandomSessionKeyBufferOffset */

	StreamWrite_UINT32(s, message.NegotiateFlags); /* NegotiateFlags (4 bytes) */

#ifdef WITH_DEBUG_NTLM
	ntlm_print_negotiate_flags(message.NegotiateFlags);
#endif

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		/* Only present if NTLMSSP_NEGOTIATE_VERSION is set */

		ntlm_get_version_info(&(message.Version));
		ntlm_write_version_info(s, &(message.Version));

#ifdef WITH_DEBUG_NTLM
		printf("Version (length = 8)\n");
		winpr_HexDump((s->p - 8), 8);
		printf("\n");
#endif
	}

	if (context->ntlm_v2)
	{
		/* Message Integrity Check */
		MicOffset = s->p;
		StreamZero(s, 16);
	}

	/* DomainName */
	if (message.DomainName.Len > 0)
	{
		StreamWrite(s, message.DomainName.Buffer, message.DomainName.Len);
#ifdef WITH_DEBUG_NTLM
		printf("DomainName (length = %d, offset = %d)\n", message.DomainName.Len, message.DomainName.BufferOffset);
		winpr_HexDump(message.DomainName.Buffer, message.DomainName.Len);
		printf("\n");
#endif
	}

	/* UserName */
	StreamWrite(s, message.UserName.Buffer, message.UserName.Len);

#ifdef WITH_DEBUG_NTLM
	printf("UserName (length = %d, offset = %d)\n", message.UserName.Len, message.UserName.BufferOffset);
	winpr_HexDump(message.UserName.Buffer, message.UserName.Len);
	printf("\n");
#endif

	/* Workstation */
	if (message.Workstation.Len > 0)
	{
		StreamWrite(s, message.Workstation.Buffer, message.Workstation.Len);
#ifdef WITH_DEBUG_NTLM
		printf("Workstation (length = %d, offset = %d)\n", message.Workstation.Len, message.Workstation.BufferOffset);
		winpr_HexDump(message.Workstation.Buffer, message.Workstation.Len);
		printf("\n");
#endif
	}

	/* LmChallengeResponse */

	if (context->LmCompatibilityLevel < 2)
	{
		StreamWrite(s, context->LmChallengeResponse.pvBuffer, message.LmChallengeResponse.Len);

#ifdef WITH_DEBUG_NTLM
		printf("LmChallengeResponse (length = %d, offset = %d)\n", message.LmChallengeResponse.Len, message.LmChallengeResponse.BufferOffset);
		winpr_HexDump(context->LmChallengeResponse.pvBuffer, message.LmChallengeResponse.Len);
		printf("\n");
#endif
	}
	else
	{
		StreamZero(s, message.LmChallengeResponse.Len);
	}

	/* NtChallengeResponse */
	StreamWrite(s, context->NtChallengeResponse.pvBuffer, message.NtChallengeResponse.Len);

#ifdef WITH_DEBUG_NTLM
	if (context->ntlm_v2)
	{
		ntlm_print_av_pairs(context);

		printf("targetInfo (length = %d)\n", (int) context->TargetInfo.cbBuffer);
		winpr_HexDump(context->TargetInfo.pvBuffer, context->TargetInfo.cbBuffer);
		printf("\n");
	}
#endif

#ifdef WITH_DEBUG_NTLM
	printf("NtChallengeResponse (length = %d, offset = %d)\n", message.NtChallengeResponse.Len, message.NtChallengeResponse.BufferOffset);
	winpr_HexDump(context->NtChallengeResponse.pvBuffer, message.NtChallengeResponse.Len);
	printf("\n");
#endif

	/* EncryptedRandomSessionKey */
	StreamWrite(s, message.EncryptedRandomSessionKey.Buffer, message.EncryptedRandomSessionKey.Len);

#ifdef WITH_DEBUG_NTLM
	printf("EncryptedRandomSessionKey (length = %d, offset = %d)\n", message.EncryptedRandomSessionKey.Len, message.EncryptedRandomSessionKey.BufferOffset);
	winpr_HexDump(message.EncryptedRandomSessionKey.Buffer, message.EncryptedRandomSessionKey.Len);
	printf("\n");
#endif

	length = StreamSize(s);
	sspi_SecBufferAlloc(&context->AuthenticateMessage, length);
	CopyMemory(context->AuthenticateMessage.pvBuffer, s->data, length);
	buffer->cbBuffer = length;

	if (context->ntlm_v2)
	{
		/* Message Integrity Check */
		ntlm_compute_message_integrity_check(context);

		s->p = MicOffset;
		StreamWrite(s, context->MessageIntegrityCheck, 16);
		s->p = s->data + length;

#ifdef WITH_DEBUG_NTLM
		printf("MessageIntegrityCheck (length = 16)\n");
		winpr_HexDump(MicOffset, 16);
		printf("\n");
#endif
	}

#ifdef WITH_DEBUG_NTLM
	printf("AUTHENTICATE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(s->data, length);
	printf("\n");
#endif

	context->state = NTLM_STATE_FINAL;

	PStreamFreeDetach(s);

	return SEC_I_COMPLETE_NEEDED;
}
