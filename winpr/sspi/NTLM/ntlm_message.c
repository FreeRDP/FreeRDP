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

#define NTLMSSP_NEGOTIATE_56					0x80000000 /* W   (0) */
#define NTLMSSP_NEGOTIATE_KEY_EXCH				0x40000000 /* V   (1) */
#define NTLMSSP_NEGOTIATE_128					0x20000000 /* U   (2) */
#define NTLMSSP_RESERVED1					0x10000000 /* r1  (3) */
#define NTLMSSP_RESERVED2					0x08000000 /* r2  (4) */
#define NTLMSSP_RESERVED3					0x04000000 /* r3  (5) */
#define NTLMSSP_NEGOTIATE_VERSION				0x02000000 /* T   (6) */
#define NTLMSSP_RESERVED4					0x01000000 /* r4  (7) */
#define NTLMSSP_NEGOTIATE_TARGET_INFO				0x00800000 /* S   (8) */
#define NTLMSSP_RESERVEDEQUEST_NON_NT_SESSION_KEY		0x00400000 /* R   (9) */
#define NTLMSSP_RESERVED5					0x00200000 /* r5  (10) */
#define NTLMSSP_NEGOTIATE_IDENTIFY				0x00100000 /* Q   (11) */
#define NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY		0x00080000 /* P   (12) */
#define NTLMSSP_RESERVED6					0x00040000 /* r6  (13) */
#define NTLMSSP_TARGET_TYPE_SERVER				0x00020000 /* O   (14) */
#define NTLMSSP_TARGET_TYPE_DOMAIN				0x00010000 /* N   (15) */
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN				0x00008000 /* M   (16) */
#define NTLMSSP_RESERVED7					0x00004000 /* r7  (17) */
#define NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED			0x00002000 /* L   (18) */
#define NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED			0x00001000 /* K   (19) */
#define NTLMSSP_NEGOTIATE_ANONYMOUS				0x00000800 /* J   (20) */
#define NTLMSSP_RESERVED8					0x00000400 /* r8  (21) */
#define NTLMSSP_NEGOTIATE_NTLM					0x00000200 /* H   (22) */
#define NTLMSSP_RESERVED9					0x00000100 /* r9  (23) */
#define NTLMSSP_NEGOTIATE_LM_KEY				0x00000080 /* G   (24) */
#define NTLMSSP_NEGOTIATE_DATAGRAM				0x00000040 /* F   (25) */
#define NTLMSSP_NEGOTIATE_SEAL					0x00000020 /* E   (26) */
#define NTLMSSP_NEGOTIATE_SIGN					0x00000010 /* D   (27) */
#define NTLMSSP_RESERVED10					0x00000008 /* r10 (28) */
#define NTLMSSP_REQUEST_TARGET					0x00000004 /* C   (29) */
#define NTLMSSP_NEGOTIATE_OEM					0x00000002 /* B   (30) */
#define NTLMSSP_NEGOTIATE_UNICODE				0x00000001 /* A   (31) */

#define WINDOWS_MAJOR_VERSION_5		0x05
#define WINDOWS_MAJOR_VERSION_6		0x06
#define WINDOWS_MINOR_VERSION_0		0x00
#define WINDOWS_MINOR_VERSION_1		0x01
#define WINDOWS_MINOR_VERSION_2		0x02
#define NTLMSSP_REVISION_W2K3		0x0F

#define MESSAGE_TYPE_NEGOTIATE		1
#define MESSAGE_TYPE_CHALLENGE		2
#define MESSAGE_TYPE_AUTHENTICATE	3

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

/**
 * Output VERSION structure.\n
 * VERSION @msdn{cc236654}
 * @param s
 */

void ntlm_output_version(PStream s)
{
	OSVERSIONINFOA osVersionInfo;

	osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);

	GetVersionExA(&osVersionInfo);

	StreamWrite_UINT8(s, osVersionInfo.dwMajorVersion); /* ProductMajorVersion (1 byte) */
	StreamWrite_UINT8(s, osVersionInfo.dwMinorVersion); /* ProductMinorVersion (1 byte) */
	StreamWrite_UINT16(s, osVersionInfo.dwBuildNumber); /* ProductBuild (2 bytes) */
	StreamZero(s, 3); /* Reserved (3 bytes) */
	StreamWrite_UINT8(s, NTLMSSP_REVISION_W2K3); /* NTLMRevisionCurrent (1 byte) */
}

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
	BYTE Signature[8];
	UINT32 MessageType;
	UINT32 NegotiateFlags;
	UINT16 DomainNameLen;
	UINT16 DomainNameMaxLen;
	UINT32 DomainNameBufferOffset;
	UINT16 WorkstationLen;
	UINT16 WorkstationMaxLen;
	UINT32 WorkstationBufferOffset;

	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	StreamRead(s, Signature, 8);
	StreamRead_UINT32(s, MessageType);

	if (memcmp(Signature, NTLM_SIGNATURE, 8) != 0)
	{
		printf("Unexpected NTLM signature: %s, expected:%s\n", Signature, NTLM_SIGNATURE);
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	if (MessageType != MESSAGE_TYPE_NEGOTIATE)
	{
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	StreamRead_UINT32(s, NegotiateFlags); /* NegotiateFlags (4 bytes) */

	context->NegotiateFlags = NegotiateFlags;

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	StreamRead_UINT16(s, DomainNameLen); /* DomainNameLen */
	StreamRead_UINT16(s, DomainNameMaxLen); /* DomainNameMaxLen */
	StreamRead_UINT32(s, DomainNameBufferOffset); /* DomainNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	StreamRead_UINT16(s, WorkstationLen); /* WorkstationLen */
	StreamRead_UINT16(s, WorkstationMaxLen); /* WorkstationMaxLen */
	StreamRead_UINT32(s, WorkstationBufferOffset); /* WorkstationBufferOffset */

	if (NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
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
	UINT32 NegotiateFlags = 0;

	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	StreamWrite(s, NTLM_SIGNATURE, 8); /* Signature (8 bytes) */
	StreamWrite_UINT32(s, MESSAGE_TYPE_NEGOTIATE); /* MessageType */

	if (context->ntlm_v2)
	{
		NegotiateFlags |= NTLMSSP_NEGOTIATE_56;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_128;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_LM_KEY;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		NegotiateFlags |= NTLMSSP_REQUEST_TARGET;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_OEM;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}
	else
	{
		NegotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_128;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		NegotiateFlags |= NTLMSSP_REQUEST_TARGET;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}

	if (context->confidentiality)
		NegotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;

	if (context->SendVersionInfo)
		NegotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;

	context->NegotiateFlags = NegotiateFlags;

	StreamWrite_UINT32(s, NegotiateFlags); /* NegotiateFlags (4 bytes) */

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

	if (NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		/* Only present if NTLMSSP_NEGOTIATE_VERSION is set */
		ntlm_output_version(s);

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
	char Signature[8];
	UINT32 MessageType;
	BYTE* StartOffset;
	BYTE* PayloadOffset;
	UINT16 TargetNameLen;
	UINT16 TargetNameMaxLen;
	UINT32 TargetNameBufferOffset;
	UINT16 TargetInfoLen;
	UINT16 TargetInfoMaxLen;
	UINT32 TargetInfoBufferOffset;

	ntlm_generate_client_challenge(context);

	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	StreamRead(s, Signature, 8);
	StreamRead_UINT32(s, MessageType);

	if (memcmp(Signature, NTLM_SIGNATURE, 8) != 0)
	{
		printf("Unexpected NTLM signature: %s, expected:%s\n", Signature, NTLM_SIGNATURE);
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	if (MessageType != MESSAGE_TYPE_CHALLENGE)
	{
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	StartOffset = StreamGetPointer(s) - 12;

	/* TargetNameFields (8 bytes) */
	StreamRead_UINT16(s, TargetNameLen); /* TargetNameLen (2 bytes) */
	StreamRead_UINT16(s, TargetNameMaxLen); /* TargetNameMaxLen (2 bytes) */
	StreamRead_UINT32(s, TargetNameBufferOffset); /* TargetNameBufferOffset (4 bytes) */

	StreamRead_UINT32(s, context->NegotiateFlags); /* NegotiateFlags (4 bytes) */

#ifdef WITH_DEBUG_NTLM
	ntlm_print_negotiate_flags(context->NegotiateFlags);
#endif

	StreamRead(s, context->ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	StreamSeek(s, 8); /* Reserved (8 bytes), should be ignored */

	/* TargetInfoFields (8 bytes) */
	StreamRead_UINT16(s, TargetInfoLen); /* TargetInfoLen (2 bytes) */
	StreamRead_UINT16(s, TargetInfoMaxLen); /* TargetInfoMaxLen (2 bytes) */
	StreamRead_UINT32(s, TargetInfoBufferOffset); /* TargetInfoBufferOffset (4 bytes) */

	/* only present if NTLMSSP_NEGOTIATE_VERSION is set */

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		StreamSeek(s, 8); /* Version (8 bytes), can be ignored */
	}

	/* Payload (variable) */
	PayloadOffset = StreamGetPointer(s);

	if (TargetNameLen > 0)
	{
		p = StartOffset + TargetNameBufferOffset;
		sspi_SecBufferAlloc(&context->TargetName, TargetNameLen);
		CopyMemory(context->TargetName.pvBuffer, p, TargetNameLen);

#ifdef WITH_DEBUG_NTLM
		printf("TargetName (length = %d, offset = %d)\n", TargetNameLen, TargetNameBufferOffset);
		winpr_HexDump(context->TargetName.pvBuffer, context->TargetName.cbBuffer);
		printf("\n");
#endif
	}

	if (TargetInfoLen > 0)
	{
		p = StartOffset + TargetInfoBufferOffset;
		sspi_SecBufferAlloc(&context->TargetInfo, TargetInfoLen);
		CopyMemory(context->TargetInfo.pvBuffer, p, TargetInfoLen);

#ifdef WITH_DEBUG_NTLM
		printf("TargetInfo (length = %d, offset = %d)\n", TargetInfoLen, TargetInfoBufferOffset);
		winpr_HexDump(context->TargetInfo.pvBuffer, context->TargetInfo.cbBuffer);
		printf("\n");
#endif

		if (context->ntlm_v2)
		{
			StreamSetPointer(s, p);
			ntlm_input_av_pairs(context, s);
		}
	}

	length = (PayloadOffset - StartOffset) + TargetNameLen + TargetInfoLen;

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
	UINT16 TargetNameLen;
	BYTE* TargetNameBuffer;
	UINT32 TargetNameBufferOffset;
	UINT16 TargetInfoLen;
	BYTE* TargetInfoBuffer;
	UINT32 TargetInfoBufferOffset;

	/* Server Challenge */
	ntlm_generate_server_challenge(context);

	/* Timestamp */
	ntlm_generate_timestamp(context);

	/* TargetInfo */
	ntlm_populate_server_av_pairs(context);

	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	StreamWrite(s, NTLM_SIGNATURE, 8); /* Signature (8 bytes) */
	StreamWrite_UINT32(s, MESSAGE_TYPE_CHALLENGE); /* MessageType */

	if (context->NegotiateFlags & NTLMSSP_REQUEST_TARGET)
	{
		TargetNameLen = (UINT16) context->TargetName.cbBuffer;
		TargetNameBuffer = context->TargetName.pvBuffer;
	}
	else
	{
		TargetNameLen = 0;
		TargetNameBuffer = NULL;
	}

	context->NegotiateFlags |= NTLMSSP_NEGOTIATE_TARGET_INFO;

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_TARGET_INFO)
	{
		TargetInfoLen = (UINT16) context->TargetInfo.cbBuffer;
		TargetInfoBuffer = context->TargetInfo.pvBuffer;
	}
	else
	{
		TargetInfoLen = 0;
		TargetInfoBuffer = NULL;
	}

	PayloadOffset = 48;

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		PayloadOffset += 8;

	TargetNameBufferOffset = PayloadOffset;
	TargetInfoBufferOffset = TargetNameBufferOffset + TargetNameLen;

	/* TargetNameFields (8 bytes) */
	StreamWrite_UINT16(s, TargetNameLen); /* TargetNameLen (2 bytes) */
	StreamWrite_UINT16(s, TargetNameLen); /* TargetNameMaxLen (2 bytes) */
	StreamWrite_UINT32(s, TargetNameBufferOffset); /* TargetNameBufferOffset (4 bytes) */

	StreamWrite_UINT32(s, context->NegotiateFlags); /* NegotiateFlags (4 bytes) */

	StreamWrite(s, context->ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	StreamZero(s, 8); /* Reserved (8 bytes), should be ignored */

	/* TargetInfoFields (8 bytes) */
	StreamWrite_UINT16(s, TargetInfoLen); /* TargetInfoLen (2 bytes) */
	StreamWrite_UINT16(s, TargetInfoLen); /* TargetInfoMaxLen (2 bytes) */
	StreamWrite_UINT32(s, TargetInfoBufferOffset); /* TargetInfoBufferOffset (4 bytes) */

	/* only present if NTLMSSP_NEGOTIATE_VERSION is set */

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		ntlm_output_version(s); /* Version (8 bytes), can be ignored */
	}

	/* Payload (variable) */

	if (TargetNameLen > 0)
	{
		StreamWrite(s, TargetNameBuffer, TargetNameLen);
#ifdef WITH_DEBUG_NTLM
		printf("TargetName (length = %d, offset = %d)\n", TargetNameLen, TargetNameBufferOffset);
		winpr_HexDump(TargetNameBuffer, TargetNameLen);
		printf("\n");
#endif
	}

	if (TargetInfoLen > 0)
	{
		StreamWrite(s, TargetInfoBuffer, TargetInfoLen);
#ifdef WITH_DEBUG_NTLM
		printf("TargetInfo (length = %d, offset = %d)\n", TargetInfoLen, TargetInfoBufferOffset);
		winpr_HexDump(TargetInfoBuffer, TargetInfoLen);
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
	BYTE Signature[8];
	UINT32 MessageType;
	UINT32 NegotiateFlags;
	UINT16 DomainNameLen;
	UINT16 DomainNameMaxLen;
	BYTE* DomainNameBuffer;
	UINT32 DomainNameBufferOffset;
	UINT16 UserNameLen;
	UINT16 UserNameMaxLen;
	BYTE* UserNameBuffer;
	UINT32 UserNameBufferOffset;
	UINT16 WorkstationLen;
	UINT16 WorkstationMaxLen;
	BYTE* WorkstationBuffer;
	UINT32 WorkstationBufferOffset;
	UINT16 LmChallengeResponseLen;
	UINT16 LmChallengeResponseMaxLen;
	BYTE* LmChallengeResponseBuffer;
	UINT32 LmChallengeResponseBufferOffset;
	UINT16 NtChallengeResponseLen;
	UINT16 NtChallengeResponseMaxLen;
	BYTE* NtChallengeResponseBuffer;
	UINT32 NtChallengeResponseBufferOffset;
	UINT16 EncryptedRandomSessionKeyLen;
	UINT16 EncryptedRandomSessionKeyMaxLen;
	BYTE* EncryptedRandomSessionKeyBuffer;
	UINT32 EncryptedRandomSessionKeyBufferOffset;

	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	StreamRead(s, Signature, 8);
	StreamRead_UINT32(s, MessageType);

	if (memcmp(Signature, NTLM_SIGNATURE, 8) != 0)
	{
		printf("Unexpected NTLM signature: %s, expected:%s\n", Signature, NTLM_SIGNATURE);
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	if (MessageType != MESSAGE_TYPE_AUTHENTICATE)
	{
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	/* LmChallengeResponseFields (8 bytes) */
	StreamRead_UINT16(s, LmChallengeResponseLen); /* LmChallengeResponseLen */
	StreamRead_UINT16(s, LmChallengeResponseMaxLen); /* LmChallengeResponseMaxLen */
	StreamRead_UINT32(s, LmChallengeResponseBufferOffset); /* LmChallengeResponseBufferOffset */

	/* NtChallengeResponseFields (8 bytes) */
	StreamRead_UINT16(s, NtChallengeResponseLen); /* NtChallengeResponseLen */
	StreamRead_UINT16(s, NtChallengeResponseMaxLen); /* NtChallengeResponseMaxLen */
	StreamRead_UINT32(s, NtChallengeResponseBufferOffset); /* NtChallengeResponseBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	StreamRead_UINT16(s, DomainNameLen); /* DomainNameLen */
	StreamRead_UINT16(s, DomainNameMaxLen); /* DomainNameMaxLen */
	StreamRead_UINT32(s, DomainNameBufferOffset); /* DomainNameBufferOffset */

	/* UserNameFields (8 bytes) */
	StreamRead_UINT16(s, UserNameLen); /* UserNameLen */
	StreamRead_UINT16(s, UserNameMaxLen); /* UserNameMaxLen */
	StreamRead_UINT32(s, UserNameBufferOffset); /* UserNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	StreamRead_UINT16(s, WorkstationLen); /* WorkstationLen */
	StreamRead_UINT16(s, WorkstationMaxLen); /* WorkstationMaxLen */
	StreamRead_UINT32(s, WorkstationBufferOffset); /* WorkstationBufferOffset */

	/* EncryptedRandomSessionKeyFields (8 bytes) */
	StreamRead_UINT16(s, EncryptedRandomSessionKeyLen); /* EncryptedRandomSessionKeyLen */
	StreamRead_UINT16(s, EncryptedRandomSessionKeyMaxLen); /* EncryptedRandomSessionKeyMaxLen */
	StreamRead_UINT32(s, EncryptedRandomSessionKeyBufferOffset); /* EncryptedRandomSessionKeyBufferOffset */

	StreamRead_UINT32(s, NegotiateFlags); /* NegotiateFlags (4 bytes) */

	if (NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
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
	if (DomainNameLen > 0)
	{
		DomainNameBuffer = s->data + DomainNameBufferOffset;
#ifdef WITH_DEBUG_NTLM
		printf("DomainName (length = %d, offset = %d)\n", DomainNameLen, DomainNameBufferOffset);
		winpr_HexDump(DomainNameBuffer, DomainNameLen);
		printf("\n");
#endif
	}

	/* UserName */
	if (UserNameLen > 0)
	{
		UserNameBuffer = s->data + UserNameBufferOffset;
#ifdef WITH_DEBUG_NTLM
		printf("UserName (length = %d, offset = %d)\n", UserNameLen, UserNameBufferOffset);
		winpr_HexDump(UserNameBuffer, UserNameLen);
		printf("\n");
#endif
	}

	/* Workstation */
	if (WorkstationLen > 0)
	{
		WorkstationBuffer = s->data + WorkstationBufferOffset;
#ifdef WITH_DEBUG_NTLM
		printf("Workstation (length = %d, offset = %d)\n", WorkstationLen, WorkstationBufferOffset);
		winpr_HexDump(WorkstationBuffer, WorkstationLen);
		printf("\n");
#endif
	}

	/* LmChallengeResponse */
	if (LmChallengeResponseLen > 0)
	{
		LmChallengeResponseBuffer = s->data + LmChallengeResponseBufferOffset;
#ifdef WITH_DEBUG_NTLM
		printf("LmChallengeResponse (length = %d, offset = %d)\n", LmChallengeResponseLen, LmChallengeResponseBufferOffset);
		winpr_HexDump(LmChallengeResponseBuffer, LmChallengeResponseLen);
		printf("\n");
#endif
	}

	/* NtChallengeResponse */
	if (NtChallengeResponseLen > 0)
	{
		BYTE* ClientChallengeBuffer;

		NtChallengeResponseBuffer = s->data + NtChallengeResponseBufferOffset;

		ClientChallengeBuffer = NtChallengeResponseBuffer + 32;
		CopyMemory(context->ClientChallenge, ClientChallengeBuffer, 8);

#ifdef WITH_DEBUG_NTLM
		printf("NtChallengeResponse (length = %d, offset = %d)\n", NtChallengeResponseLen, NtChallengeResponseBufferOffset);
		winpr_HexDump(NtChallengeResponseBuffer, NtChallengeResponseLen);
		printf("\n");
#endif
	}

	/* EncryptedRandomSessionKey */
	if (EncryptedRandomSessionKeyLen > 0)
	{
		EncryptedRandomSessionKeyBuffer = s->data + EncryptedRandomSessionKeyBufferOffset;
		CopyMemory(context->EncryptedRandomSessionKey, EncryptedRandomSessionKeyBuffer, 16);

#ifdef WITH_DEBUG_NTLM
		printf("EncryptedRandomSessionKey (length = %d, offset = %d)\n", EncryptedRandomSessionKeyLen, EncryptedRandomSessionKeyBufferOffset);
		winpr_HexDump(EncryptedRandomSessionKeyBuffer, EncryptedRandomSessionKeyLen);
		printf("\n");
#endif
	}

	if (UserNameLen > 0)
	{
		context->identity.User = (UINT16*) malloc(UserNameLen);
		CopyMemory(context->identity.User, UserNameBuffer, UserNameLen);
		context->identity.UserLength = UserNameLen;
	}

	if (DomainNameLen > 0)
	{
		context->identity.Domain = (UINT16*) malloc(DomainNameLen);
		CopyMemory(context->identity.Domain, DomainNameBuffer, DomainNameLen);
		context->identity.DomainLength = DomainNameLen;
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
	UINT32 NegotiateFlags = 0;

	UINT16 DomainNameLen;
	UINT16 UserNameLen;
	UINT16 WorkstationLen;
	UINT16 LmChallengeResponseLen;
	UINT16 NtChallengeResponseLen;
	UINT16 EncryptedRandomSessionKeyLen;

	UINT32 PayloadBufferOffset;
	UINT32 DomainNameBufferOffset;
	UINT32 UserNameBufferOffset;
	UINT32 WorkstationBufferOffset;
	UINT32 LmChallengeResponseBufferOffset;
	UINT32 NtChallengeResponseBufferOffset;
	UINT32 EncryptedRandomSessionKeyBufferOffset;

	BYTE* UserNameBuffer;
	BYTE* DomainNameBuffer;
	BYTE* WorkstationBuffer;
	BYTE* EncryptedRandomSessionKeyBuffer;

	WorkstationLen = context->WorkstationLength;
	WorkstationBuffer = (BYTE*) context->Workstation;

	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	if (context->ntlm_v2 < 1)
		WorkstationLen = 0;

	DomainNameLen = (UINT16) context->identity.DomainLength * 2;
	DomainNameBuffer = (BYTE*) context->identity.Domain;

	UserNameLen = (UINT16) context->identity.UserLength * 2;
	UserNameBuffer = (BYTE*) context->identity.User;

	LmChallengeResponseLen = (UINT16) 24;
	NtChallengeResponseLen = (UINT16) context->NtChallengeResponse.cbBuffer;

	EncryptedRandomSessionKeyLen = 16;
	EncryptedRandomSessionKeyBuffer = context->EncryptedRandomSessionKey;

	if (context->ntlm_v2)
	{
		/* observed: 35 82 88 e2 (0xE2888235) */
		NegotiateFlags |= NTLMSSP_NEGOTIATE_56;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_128;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_TARGET_INFO;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		NegotiateFlags |= NTLMSSP_REQUEST_TARGET;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}
	else
	{
		NegotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_128;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		NegotiateFlags |= NTLMSSP_REQUEST_TARGET;
		NegotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}

	if (context->confidentiality)
		NegotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;

	if (context->ntlm_v2)
		PayloadBufferOffset = 80; /* starting buffer offset */
	else
		PayloadBufferOffset = 64; /* starting buffer offset */

	if (NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		PayloadBufferOffset += 8;

	DomainNameBufferOffset = PayloadBufferOffset;
	UserNameBufferOffset = DomainNameBufferOffset + DomainNameLen;
	WorkstationBufferOffset = UserNameBufferOffset + UserNameLen;
	LmChallengeResponseBufferOffset = WorkstationBufferOffset + WorkstationLen;
	NtChallengeResponseBufferOffset = LmChallengeResponseBufferOffset + LmChallengeResponseLen;
	EncryptedRandomSessionKeyBufferOffset = NtChallengeResponseBufferOffset + NtChallengeResponseLen;

	StreamWrite(s, NTLM_SIGNATURE, 8); /* Signature (8 bytes) */
	StreamWrite_UINT32(s, MESSAGE_TYPE_AUTHENTICATE); /* MessageType */

	/* LmChallengeResponseFields (8 bytes) */
	StreamWrite_UINT16(s, LmChallengeResponseLen); /* LmChallengeResponseLen */
	StreamWrite_UINT16(s, LmChallengeResponseLen); /* LmChallengeResponseMaxLen */
	StreamWrite_UINT32(s, LmChallengeResponseBufferOffset); /* LmChallengeResponseBufferOffset */

	/* NtChallengeResponseFields (8 bytes) */
	StreamWrite_UINT16(s, NtChallengeResponseLen); /* NtChallengeResponseLen */
	StreamWrite_UINT16(s, NtChallengeResponseLen); /* NtChallengeResponseMaxLen */
	StreamWrite_UINT32(s, NtChallengeResponseBufferOffset); /* NtChallengeResponseBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	StreamWrite_UINT16(s, DomainNameLen); /* DomainNameLen */
	StreamWrite_UINT16(s, DomainNameLen); /* DomainNameMaxLen */
	StreamWrite_UINT32(s, DomainNameBufferOffset); /* DomainNameBufferOffset */

	/* UserNameFields (8 bytes) */
	StreamWrite_UINT16(s, UserNameLen); /* UserNameLen */
	StreamWrite_UINT16(s, UserNameLen); /* UserNameMaxLen */
	StreamWrite_UINT32(s, UserNameBufferOffset); /* UserNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	StreamWrite_UINT16(s, WorkstationLen); /* WorkstationLen */
	StreamWrite_UINT16(s, WorkstationLen); /* WorkstationMaxLen */
	StreamWrite_UINT32(s, WorkstationBufferOffset); /* WorkstationBufferOffset */

	/* EncryptedRandomSessionKeyFields (8 bytes) */
	StreamWrite_UINT16(s, EncryptedRandomSessionKeyLen); /* EncryptedRandomSessionKeyLen */
	StreamWrite_UINT16(s, EncryptedRandomSessionKeyLen); /* EncryptedRandomSessionKeyMaxLen */
	StreamWrite_UINT32(s, EncryptedRandomSessionKeyBufferOffset); /* EncryptedRandomSessionKeyBufferOffset */

	StreamWrite_UINT32(s, NegotiateFlags); /* NegotiateFlags (4 bytes) */

#ifdef WITH_DEBUG_NTLM
	ntlm_print_negotiate_flags(NegotiateFlags);
#endif

	if (NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		/* Only present if NTLMSSP_NEGOTIATE_VERSION is set */
		ntlm_output_version(s);

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
	if (DomainNameLen > 0)
	{
		StreamWrite(s, DomainNameBuffer, DomainNameLen);
#ifdef WITH_DEBUG_NTLM
		printf("DomainName (length = %d, offset = %d)\n", DomainNameLen, DomainNameBufferOffset);
		winpr_HexDump(DomainNameBuffer, DomainNameLen);
		printf("\n");
#endif
	}

	/* UserName */
	StreamWrite(s, UserNameBuffer, UserNameLen);

#ifdef WITH_DEBUG_NTLM
	printf("UserName (length = %d, offset = %d)\n", UserNameLen, UserNameBufferOffset);
	winpr_HexDump(UserNameBuffer, UserNameLen);
	printf("\n");
#endif

	/* Workstation */
	if (WorkstationLen > 0)
	{
		StreamWrite(s, WorkstationBuffer, WorkstationLen);
#ifdef WITH_DEBUG_NTLM
		printf("Workstation (length = %d, offset = %d)\n", WorkstationLen, WorkstationBufferOffset);
		winpr_HexDump(WorkstationBuffer, WorkstationLen);
		printf("\n");
#endif
	}

	/* LmChallengeResponse */

	if (context->LmCompatibilityLevel < 2)
	{
		StreamWrite(s, context->LmChallengeResponse.pvBuffer, LmChallengeResponseLen);

#ifdef WITH_DEBUG_NTLM
		printf("LmChallengeResponse (length = %d, offset = %d)\n", LmChallengeResponseLen, LmChallengeResponseBufferOffset);
		winpr_HexDump(context->LmChallengeResponse.pvBuffer, LmChallengeResponseLen);
		printf("\n");
#endif
	}
	else
	{
		StreamZero(s, LmChallengeResponseLen);
	}

	/* NtChallengeResponse */
	StreamWrite(s, context->NtChallengeResponse.pvBuffer, NtChallengeResponseLen);

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
	printf("NtChallengeResponse (length = %d, offset = %d)\n", NtChallengeResponseLen, NtChallengeResponseBufferOffset);
	winpr_HexDump(context->NtChallengeResponse.pvBuffer, NtChallengeResponseLen);
	printf("\n");
#endif

	/* EncryptedRandomSessionKey */
	StreamWrite(s, EncryptedRandomSessionKeyBuffer, EncryptedRandomSessionKeyLen);

#ifdef WITH_DEBUG_NTLM
	printf("EncryptedRandomSessionKey (length = %d, offset = %d)\n", EncryptedRandomSessionKeyLen, EncryptedRandomSessionKeyBufferOffset);
	winpr_HexDump(EncryptedRandomSessionKeyBuffer, EncryptedRandomSessionKeyLen);
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
/* Modeline for vim. Don't delete */
/* vim: cindent:noet:sw=8:ts=8 */
