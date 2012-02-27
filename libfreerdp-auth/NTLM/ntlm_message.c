/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#include <freerdp/utils/stream.h>
#include <freerdp/utils/hexdump.h>

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

void ntlm_output_version(STREAM* s)
{
	/* The following version information was observed with Windows 7 */

	stream_write_uint8(s, WINDOWS_MAJOR_VERSION_6); /* ProductMajorVersion (1 byte) */
	stream_write_uint8(s, WINDOWS_MINOR_VERSION_1); /* ProductMinorVersion (1 byte) */
	stream_write_uint16(s, 7600); /* ProductBuild (2 bytes) */
	stream_write_zero(s, 3); /* Reserved (3 bytes) */
	stream_write_uint8(s, NTLMSSP_REVISION_W2K3); /* NTLMRevisionCurrent (1 byte) */
}

void ntlm_print_negotiate_flags(uint32 flags)
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

SECURITY_STATUS ntlm_write_NegotiateMessage(NTLM_CONTEXT* context, SEC_BUFFER* buffer)
{
	STREAM* s;
	int length;
	uint32 negotiateFlags = 0;

	s = stream_new(0);
	stream_attach(s, buffer->pvBuffer, buffer->cbBuffer);

	stream_write(s, NTLM_SIGNATURE, 8); /* Signature (8 bytes) */
	stream_write_uint32(s, MESSAGE_TYPE_NEGOTIATE); /* MessageType */

	if (context->ntlm_v2)
	{
		negotiateFlags |= NTLMSSP_NEGOTIATE_56;
		negotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		negotiateFlags |= NTLMSSP_NEGOTIATE_128;
		negotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
		negotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		negotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		negotiateFlags |= NTLMSSP_NEGOTIATE_LM_KEY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		negotiateFlags |= NTLMSSP_REQUEST_TARGET;
		negotiateFlags |= NTLMSSP_NEGOTIATE_OEM;
		negotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}
	else
	{
		negotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		negotiateFlags |= NTLMSSP_NEGOTIATE_128;
		negotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		negotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		negotiateFlags |= NTLMSSP_REQUEST_TARGET;
		negotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}

	context->NegotiateFlags = negotiateFlags;

	stream_write_uint32(s, negotiateFlags); /* NegotiateFlags (4 bytes) */

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	stream_write_uint16(s, 0); /* DomainNameLen */
	stream_write_uint16(s, 0); /* DomainNameMaxLen */
	stream_write_uint32(s, 0); /* DomainNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	stream_write_uint16(s, 0); /* WorkstationLen */
	stream_write_uint16(s, 0); /* WorkstationMaxLen */
	stream_write_uint32(s, 0); /* WorkstationBufferOffset */

	if (negotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		/* Only present if NTLMSSP_NEGOTIATE_VERSION is set */
		ntlm_output_version(s);

#ifdef WITH_DEBUG_NTLM
		printf("Version (length = 8)\n");
		freerdp_hexdump((s->p - 8), 8);
		printf("\n");
#endif
	}

	length = s->p - s->data;
	buffer->cbBuffer = length;

	sspi_SecBufferAlloc(&context->NegotiateMessage, length);
	memcpy(context->NegotiateMessage.pvBuffer, buffer->pvBuffer, buffer->cbBuffer);
	context->NegotiateMessage.BufferType = buffer->BufferType;

#ifdef WITH_DEBUG_NTLM
	printf("NEGOTIATE_MESSAGE (length = %d)\n", length);
	freerdp_hexdump(s->data, length);
	printf("\n");
#endif

	context->state = NTLM_STATE_CHALLENGE;

	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_read_ChallengeMessage(NTLM_CONTEXT* context, SEC_BUFFER* buffer)
{
	uint8* p;
	STREAM* s;
	int length;
	char signature[8];
	uint32 messageType;
	uint8* start_offset;
	uint8* payload_offset;
	uint16 targetNameLen;
	uint16 targetNameMaxLen;
	uint32 targetNameBufferOffset;
	uint16 targetInfoLen;
	uint16 targetInfoMaxLen;
	uint32 targetInfoBufferOffset;

	s = stream_new(0);
	stream_attach(s, buffer->pvBuffer, buffer->cbBuffer);

	stream_read(s, signature, 8);
	stream_read_uint32(s, messageType);

	if (memcmp(signature, NTLM_SIGNATURE, 8) != 0)
	{
		printf("Unexpected NTLM signature: %s, expected:%s\n", signature, NTLM_SIGNATURE);
		return SEC_E_INVALID_TOKEN;
	}

	start_offset = s->p - 12;

	/* TargetNameFields (8 bytes) */
	stream_read_uint16(s, targetNameLen); /* TargetNameLen (2 bytes) */
	stream_read_uint16(s, targetNameMaxLen); /* TargetNameMaxLen (2 bytes) */
	stream_read_uint32(s, targetNameBufferOffset); /* TargetNameBufferOffset (4 bytes) */

	stream_read_uint32(s, context->NegotiateFlags); /* NegotiateFlags (4 bytes) */

#ifdef WITH_DEBUG_NTLM
	ntlm_print_negotiate_flags(context->NegotiateFlags);
#endif

	stream_read(s, context->ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	stream_seek(s, 8); /* Reserved (8 bytes), should be ignored */

	/* TargetInfoFields (8 bytes) */
	stream_read_uint16(s, targetInfoLen); /* TargetInfoLen (2 bytes) */
	stream_read_uint16(s, targetInfoMaxLen); /* TargetInfoMaxLen (2 bytes) */
	stream_read_uint32(s, targetInfoBufferOffset); /* TargetInfoBufferOffset (4 bytes) */

	/* only present if NTLMSSP_NEGOTIATE_VERSION is set */

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		stream_seek(s, 8); /* Version (8 bytes), can be ignored */
	}

	/* Payload (variable) */
	payload_offset = s->p;

	if (targetNameLen > 0)
	{
		p = start_offset + targetNameBufferOffset;
		sspi_SecBufferAlloc(&context->TargetName, targetNameLen);
		memcpy(context->TargetName.pvBuffer, p, targetNameLen);

#ifdef WITH_DEBUG_NTLM
		printf("TargetName (length = %d, offset = %d)\n", targetNameLen, targetNameBufferOffset);
		freerdp_hexdump(context->TargetName.pvBuffer, context->TargetName.cbBuffer);
		printf("\n");
#endif
	}

	if (targetInfoLen > 0)
	{
		p = start_offset + targetInfoBufferOffset;
		sspi_SecBufferAlloc(&context->TargetInfo, targetInfoLen);
		memcpy(context->TargetInfo.pvBuffer, p, targetInfoLen);

#ifdef WITH_DEBUG_NTLM
		printf("TargetInfo (length = %d, offset = %d)\n", targetInfoLen, targetInfoBufferOffset);
		freerdp_hexdump(context->TargetInfo.pvBuffer, context->TargetInfo.cbBuffer);
		printf("\n");
#endif

		if (context->ntlm_v2)
		{
			s->p = p;
			ntlm_input_av_pairs(context, s);
		}
	}

	length = (payload_offset - start_offset) + targetNameLen + targetInfoLen;

	sspi_SecBufferAlloc(&context->ChallengeMessage, length);
	memcpy(context->ChallengeMessage.pvBuffer, start_offset, length);

#ifdef WITH_DEBUG_NTLM
	printf("CHALLENGE_MESSAGE (length = %d)\n", length);
	freerdp_hexdump(context->ChallengeMessage.pvBuffer, context->ChallengeMessage.cbBuffer);
	printf("\n");
#endif

	/* AV_PAIRs */
	if (context->ntlm_v2)
		ntlm_populate_av_pairs(context);

	/* Timestamp */
	ntlm_generate_timestamp(context);

	/* LmChallengeResponse */
	ntlm_compute_lm_v2_response(context);

	if (context->ntlm_v2)
		memset(context->LmChallengeResponse.pvBuffer, 0, context->LmChallengeResponse.cbBuffer);

	/* NtChallengeResponse */
	ntlm_compute_ntlm_v2_response(context);

	/* KeyExchangeKey */
	ntlm_generate_key_exchange_key(context);

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
	freerdp_hexdump(context->ClientChallenge, 8);
	printf("\n");

	printf("ServerChallenge\n");
	freerdp_hexdump(context->ServerChallenge, 8);
	printf("\n");

	printf("SessionBaseKey\n");
	freerdp_hexdump(context->SessionBaseKey, 16);
	printf("\n");

	printf("KeyExchangeKey\n");
	freerdp_hexdump(context->KeyExchangeKey, 16);
	printf("\n");

	printf("ExportedSessionKey\n");
	freerdp_hexdump(context->ExportedSessionKey, 16);
	printf("\n");

	printf("RandomSessionKey\n");
	freerdp_hexdump(context->RandomSessionKey, 16);
	printf("\n");

	printf("ClientSignKey\n");
	freerdp_hexdump(context->ClientSigningKey, 16);
	printf("\n");

	printf("ClientSealingKey\n");
	freerdp_hexdump(context->ClientSealingKey, 16);
	printf("\n");

	printf("Timestamp\n");
	freerdp_hexdump(context->Timestamp, 8);
	printf("\n");
#endif

	context->state = NTLM_STATE_AUTHENTICATE;

	return SEC_I_CONTINUE_NEEDED;
}

/**
 * Send NTLMSSP AUTHENTICATE_MESSAGE.\n
 * AUTHENTICATE_MESSAGE @msdn{cc236643}
 * @param NTLM context
 * @param buffer
 */

SECURITY_STATUS ntlm_write_AuthenticateMessage(NTLM_CONTEXT* context, SEC_BUFFER* buffer)
{
	STREAM* s;
	int length;
	uint8* mic_offset = NULL;
	uint32 negotiateFlags = 0;

	uint16 DomainNameLen;
	uint16 UserNameLen;
	uint16 WorkstationLen;
	uint16 LmChallengeResponseLen;
	uint16 NtChallengeResponseLen;
	uint16 EncryptedRandomSessionKeyLen;

	uint32 PayloadBufferOffset;
	uint32 DomainNameBufferOffset;
	uint32 UserNameBufferOffset;
	uint32 WorkstationBufferOffset;
	uint32 LmChallengeResponseBufferOffset;
	uint32 NtChallengeResponseBufferOffset;
	uint32 EncryptedRandomSessionKeyBufferOffset;

	uint8* UserNameBuffer;
	uint8* DomainNameBuffer;
	uint8* WorkstationBuffer;
	uint8* EncryptedRandomSessionKeyBuffer;

	WorkstationLen = context->WorkstationLength;
	WorkstationBuffer = (uint8*) context->Workstation;

	s = stream_new(0);
	stream_attach(s, buffer->pvBuffer, buffer->cbBuffer);

	if (context->ntlm_v2 < 1)
		WorkstationLen = 0;

	DomainNameLen = context->identity.DomainLength;
	DomainNameBuffer = (uint8*) context->identity.Domain;

	UserNameLen = context->identity.UserLength;
	UserNameBuffer = (uint8*) context->identity.User;

	LmChallengeResponseLen = context->LmChallengeResponse.cbBuffer;
	NtChallengeResponseLen = context->NtChallengeResponse.cbBuffer;

	EncryptedRandomSessionKeyLen = 16;
	EncryptedRandomSessionKeyBuffer = context->EncryptedRandomSessionKey;

	if (context->ntlm_v2)
	{
		/* observed: 35 82 88 e2 (0xE2888235) */
		negotiateFlags |= NTLMSSP_NEGOTIATE_56;
		negotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		negotiateFlags |= NTLMSSP_NEGOTIATE_128;
		negotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
		negotiateFlags |= NTLMSSP_NEGOTIATE_TARGET_INFO;
		negotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		negotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		negotiateFlags |= NTLMSSP_REQUEST_TARGET;
		negotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}
	else
	{
		negotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		negotiateFlags |= NTLMSSP_NEGOTIATE_128;
		negotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		negotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		negotiateFlags |= NTLMSSP_REQUEST_TARGET;
		negotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}

	if (context->ntlm_v2)
		PayloadBufferOffset = 80; /* starting buffer offset */
	else
		PayloadBufferOffset = 64; /* starting buffer offset */

	if (negotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		PayloadBufferOffset += 8;

	DomainNameBufferOffset = PayloadBufferOffset;
	UserNameBufferOffset = DomainNameBufferOffset + DomainNameLen;
	WorkstationBufferOffset = UserNameBufferOffset + UserNameLen;
	LmChallengeResponseBufferOffset = WorkstationBufferOffset + WorkstationLen;
	NtChallengeResponseBufferOffset = LmChallengeResponseBufferOffset + LmChallengeResponseLen;
	EncryptedRandomSessionKeyBufferOffset = NtChallengeResponseBufferOffset + NtChallengeResponseLen;

	stream_write(s, NTLM_SIGNATURE, 8); /* Signature (8 bytes) */
	stream_write_uint32(s, MESSAGE_TYPE_AUTHENTICATE); /* MessageType */

	/* LmChallengeResponseFields (8 bytes) */
	stream_write_uint16(s, LmChallengeResponseLen); /* LmChallengeResponseLen */
	stream_write_uint16(s, LmChallengeResponseLen); /* LmChallengeResponseMaxLen */
	stream_write_uint32(s, LmChallengeResponseBufferOffset); /* LmChallengeResponseBufferOffset */

	/* NtChallengeResponseFields (8 bytes) */
	stream_write_uint16(s, NtChallengeResponseLen); /* NtChallengeResponseLen */
	stream_write_uint16(s, NtChallengeResponseLen); /* NtChallengeResponseMaxLen */
	stream_write_uint32(s, NtChallengeResponseBufferOffset); /* NtChallengeResponseBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	stream_write_uint16(s, DomainNameLen); /* DomainNameLen */
	stream_write_uint16(s, DomainNameLen); /* DomainNameMaxLen */
	stream_write_uint32(s, DomainNameBufferOffset); /* DomainNameBufferOffset */

	/* UserNameFields (8 bytes) */
	stream_write_uint16(s, UserNameLen); /* UserNameLen */
	stream_write_uint16(s, UserNameLen); /* UserNameMaxLen */
	stream_write_uint32(s, UserNameBufferOffset); /* UserNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	stream_write_uint16(s, WorkstationLen); /* WorkstationLen */
	stream_write_uint16(s, WorkstationLen); /* WorkstationMaxLen */
	stream_write_uint32(s, WorkstationBufferOffset); /* WorkstationBufferOffset */

	/* EncryptedRandomSessionKeyFields (8 bytes) */
	stream_write_uint16(s, EncryptedRandomSessionKeyLen); /* EncryptedRandomSessionKeyLen */
	stream_write_uint16(s, EncryptedRandomSessionKeyLen); /* EncryptedRandomSessionKeyMaxLen */
	stream_write_uint32(s, EncryptedRandomSessionKeyBufferOffset); /* EncryptedRandomSessionKeyBufferOffset */

	stream_write_uint32(s, negotiateFlags); /* NegotiateFlags (4 bytes) */

#ifdef WITH_DEBUG_NTLM
	ntlm_print_negotiate_flags(negotiateFlags);
#endif

	if (negotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		/* Only present if NTLMSSP_NEGOTIATE_VERSION is set */
		ntlm_output_version(s);

#ifdef WITH_DEBUG_NTLM
		printf("Version (length = 8)\n");
		freerdp_hexdump((s->p - 8), 8);
		printf("\n");
#endif
	}

	if (context->ntlm_v2)
	{
		/* Message Integrity Check */
		mic_offset = s->p;
		stream_write_zero(s, 16);
	}

	/* DomainName */
	if (DomainNameLen > 0)
	{
		stream_write(s, DomainNameBuffer, DomainNameLen);
#ifdef WITH_DEBUG_NTLM
		printf("DomainName (length = %d, offset = %d)\n", DomainNameLen, DomainNameBufferOffset);
		freerdp_hexdump(DomainNameBuffer, DomainNameLen);
		printf("\n");
#endif
	}

	/* UserName */
	stream_write(s, UserNameBuffer, UserNameLen);

#ifdef WITH_DEBUG_NTLM
	printf("UserName (length = %d, offset = %d)\n", UserNameLen, UserNameBufferOffset);
	freerdp_hexdump(UserNameBuffer, UserNameLen);
	printf("\n");
#endif

	/* Workstation */
	if (WorkstationLen > 0)
	{
		stream_write(s, WorkstationBuffer, WorkstationLen);
#ifdef WITH_DEBUG_NTLM
		printf("Workstation (length = %d, offset = %d)\n", WorkstationLen, WorkstationBufferOffset);
		freerdp_hexdump(WorkstationBuffer, WorkstationLen);
		printf("\n");
#endif
	}

	/* LmChallengeResponse */
	stream_write(s, context->LmChallengeResponse.pvBuffer, LmChallengeResponseLen);

#ifdef WITH_DEBUG_NTLM
	printf("LmChallengeResponse (length = %d, offset = %d)\n", LmChallengeResponseLen, LmChallengeResponseBufferOffset);
	freerdp_hexdump(context->LmChallengeResponse.pvBuffer, LmChallengeResponseLen);
	printf("\n");
#endif

	/* NtChallengeResponse */
	stream_write(s, context->NtChallengeResponse.pvBuffer, NtChallengeResponseLen);

#ifdef WITH_DEBUG_NTLM
	if (context->ntlm_v2)
	{
		ntlm_print_av_pairs(context);

		printf("targetInfo (length = %d)\n", context->TargetInfo.cbBuffer);
		freerdp_hexdump(context->TargetInfo.pvBuffer, context->TargetInfo.cbBuffer);
		printf("\n");
	}
#endif

#ifdef WITH_DEBUG_NTLM
	printf("NtChallengeResponse (length = %d, offset = %d)\n", NtChallengeResponseLen, NtChallengeResponseBufferOffset);
	freerdp_hexdump(context->NtChallengeResponse.pvBuffer, NtChallengeResponseLen);
	printf("\n");
#endif

	/* EncryptedRandomSessionKey */
	stream_write(s, EncryptedRandomSessionKeyBuffer, EncryptedRandomSessionKeyLen);

#ifdef WITH_DEBUG_NTLM
	printf("EncryptedRandomSessionKey (length = %d, offset = %d)\n", EncryptedRandomSessionKeyLen, EncryptedRandomSessionKeyBufferOffset);
	freerdp_hexdump(EncryptedRandomSessionKeyBuffer, EncryptedRandomSessionKeyLen);
	printf("\n");
#endif

	length = s->p - s->data;
	sspi_SecBufferAlloc(&context->AuthenticateMessage, length);
	memcpy(context->AuthenticateMessage.pvBuffer, s->data, length);
	buffer->cbBuffer = length;

	if (context->ntlm_v2)
	{
		/* Message Integrity Check */
		ntlm_compute_message_integrity_check(context);

		s->p = mic_offset;
		stream_write(s, context->MessageIntegrityCheck, 16);
		s->p = s->data + length;

#ifdef WITH_DEBUG_NTLM
		printf("MessageIntegrityCheck (length = 16)\n");
		freerdp_hexdump(mic_offset, 16);
		printf("\n");
#endif
	}

#ifdef WITH_DEBUG_NTLM
	printf("AUTHENTICATE_MESSAGE (length = %d)\n", length);
	freerdp_hexdump(s->data, length);
	printf("\n");
#endif

	context->state = NTLM_STATE_FINAL;

	return SEC_I_COMPLETE_NEEDED;
}
