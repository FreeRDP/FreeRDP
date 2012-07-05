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

static const char NTLM_SIGNATURE[8] = "NTLMSSP\0";

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

void ntlm_read_message_header(PStream s, NTLM_MESSAGE_HEADER* header)
{
	StreamRead(s, header->Signature, sizeof(NTLM_SIGNATURE));
	StreamRead_UINT32(s, header->MessageType);
}

void ntlm_write_message_header(PStream s, NTLM_MESSAGE_HEADER* header)
{
	StreamWrite(s, header->Signature, sizeof(NTLM_SIGNATURE));
	StreamWrite_UINT32(s, header->MessageType);
}

void ntlm_populate_message_header(NTLM_MESSAGE_HEADER* header, UINT32 MessageType)
{
	CopyMemory(header->Signature, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE));
	header->MessageType = MessageType;
}

BOOL ntlm_validate_message_header(PStream s, NTLM_MESSAGE_HEADER* header, UINT32 MessageType)
{
	if (memcmp(header->Signature, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE)) != 0)
	{
		printf("Unexpected NTLM signature: %s, expected:%s\n", header->Signature, NTLM_SIGNATURE);
		return FALSE;
	}

	if (header->MessageType != MessageType)
	{
		printf("Unexpected NTLM message type: %d, expected: %d\n", header->MessageType, MessageType);
		return FALSE;
	}

	return TRUE;
}

void ntlm_read_message_fields(PStream s, NTLM_MESSAGE_FIELDS* fields)
{
	StreamRead_UINT16(s, fields->Len); /* Len (2 bytes) */
	StreamRead_UINT16(s, fields->MaxLen); /* MaxLen (2 bytes) */
	StreamRead_UINT32(s, fields->BufferOffset); /* BufferOffset (4 bytes) */
}

void ntlm_write_message_fields(PStream s, NTLM_MESSAGE_FIELDS* fields)
{
	if (fields->MaxLen < 1)
		fields->MaxLen = fields->Len;

	StreamWrite_UINT16(s, fields->Len); /* Len (2 bytes) */
	StreamWrite_UINT16(s, fields->MaxLen); /* MaxLen (2 bytes) */
	StreamWrite_UINT32(s, fields->BufferOffset); /* BufferOffset (4 bytes) */
}

void ntlm_read_message_fields_buffer(PStream s, NTLM_MESSAGE_FIELDS* fields)
{
	if (fields->Len > 0)
	{
		fields->Buffer = malloc(fields->Len);
		StreamSetPosition(s, fields->BufferOffset);
		StreamRead(s, fields->Buffer, fields->Len);
	}
}

void ntlm_write_message_fields_buffer(PStream s, NTLM_MESSAGE_FIELDS* fields)
{
	if (fields->Len > 0)
	{
		StreamSetPosition(s, fields->BufferOffset);
		StreamWrite(s, fields->Buffer, fields->Len);
	}
}

void ntlm_print_message_fields(NTLM_MESSAGE_FIELDS* fields, const char* name)
{
	printf("%s (Len: %d MaxLen: %d BufferOffset: %d)\n",
			name, fields->Len, fields->MaxLen, fields->BufferOffset);

	if (fields->Len > 0)
		winpr_HexDump(fields->Buffer, fields->Len);

	printf("\n");
}

SECURITY_STATUS ntlm_read_NegotiateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	PStream s;
	int length;
	NTLM_NEGOTIATE_MESSAGE message;

	ZeroMemory(&message, sizeof(message));
	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	ntlm_read_message_header(s, (NTLM_MESSAGE_HEADER*) &message);

	if (!ntlm_validate_message_header(s, (NTLM_MESSAGE_HEADER*) &message, MESSAGE_TYPE_NEGOTIATE))
	{
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	StreamRead_UINT32(s, message.NegotiateFlags); /* NegotiateFlags (4 bytes) */

	if (!((message.NegotiateFlags & NTLMSSP_REQUEST_TARGET) &&
			(message.NegotiateFlags & NTLMSSP_NEGOTIATE_NTLM) &&
			(message.NegotiateFlags & NTLMSSP_NEGOTIATE_ALWAYS_SIGN) &&
			(message.NegotiateFlags & NTLMSSP_NEGOTIATE_UNICODE)))
	{
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	context->NegotiateFlags = message.NegotiateFlags;

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	ntlm_read_message_fields(s, &(message.DomainName));

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	ntlm_read_message_fields(s, &(message.Workstation));

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_read_version_info(s, &(message.Version)); /* Version (8 bytes) */

	length = StreamGetPosition(s);
	buffer->cbBuffer = length;

	sspi_SecBufferAlloc(&context->NegotiateMessage, length);
	CopyMemory(context->NegotiateMessage.pvBuffer, buffer->pvBuffer, buffer->cbBuffer);
	context->NegotiateMessage.BufferType = buffer->BufferType;

#ifdef WITH_DEBUG_NTLM
	printf("NEGOTIATE_MESSAGE (length = %d)\n", (int) context->NegotiateMessage.cbBuffer);
	winpr_HexDump(context->NegotiateMessage.pvBuffer, context->NegotiateMessage.cbBuffer);
	printf("\n");

	ntlm_print_negotiate_flags(message.NegotiateFlags);

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message.Version));
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

	ntlm_populate_message_header((NTLM_MESSAGE_HEADER*) &message, MESSAGE_TYPE_NEGOTIATE);

	if (context->NTLMv2)
	{
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_56;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_LM_KEY;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_OEM;
	}

	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_128;
	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
	message.NegotiateFlags |= NTLMSSP_REQUEST_TARGET;
	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_get_version_info(&(message.Version));

	if (context->confidentiality)
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;

	if (context->SendVersionInfo)
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;

	context->NegotiateFlags = message.NegotiateFlags;

	/* Message Header (12 bytes) */
	ntlm_write_message_header(s, (NTLM_MESSAGE_HEADER*) &message);

	StreamWrite_UINT32(s, message.NegotiateFlags); /* NegotiateFlags (4 bytes) */

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	ntlm_write_message_fields(s, &(message.DomainName));

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	ntlm_write_message_fields(s, &(message.Workstation));

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_write_version_info(s, &(message.Version));

	length = StreamGetPosition(s);
	buffer->cbBuffer = length;

	sspi_SecBufferAlloc(&context->NegotiateMessage, length);
	CopyMemory(context->NegotiateMessage.pvBuffer, buffer->pvBuffer, buffer->cbBuffer);
	context->NegotiateMessage.BufferType = buffer->BufferType;

#ifdef WITH_DEBUG_NTLM
	printf("NEGOTIATE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(s->buffer, length);
	printf("\n");

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message.Version));
#endif

	context->state = NTLM_STATE_CHALLENGE;

	PStreamFreeDetach(s);

	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_read_ChallengeMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	PStream s;
	int length;
	PBYTE StartOffset;
	PBYTE PayloadOffset;
	NTLM_AV_PAIR* AvTimestamp;
	NTLM_CHALLENGE_MESSAGE message;

	ntlm_generate_client_challenge(context);

	ZeroMemory(&message, sizeof(message));
	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	StartOffset = StreamPointer(s);

	ntlm_read_message_header(s, (NTLM_MESSAGE_HEADER*) &message);

	if (!ntlm_validate_message_header(s, (NTLM_MESSAGE_HEADER*) &message, MESSAGE_TYPE_CHALLENGE))
	{
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	/* TargetNameFields (8 bytes) */
	ntlm_read_message_fields(s, &(message.TargetName));

	StreamRead_UINT32(s, message.NegotiateFlags); /* NegotiateFlags (4 bytes) */
	context->NegotiateFlags = message.NegotiateFlags;

	StreamRead(s, message.ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	CopyMemory(context->ServerChallenge, message.ServerChallenge, 8);

	StreamRead(s, message.Reserved, 8); /* Reserved (8 bytes), should be ignored */

	/* TargetInfoFields (8 bytes) */
	ntlm_read_message_fields(s, &(message.TargetInfo));

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_read_version_info(s, &(message.Version)); /* Version (8 bytes) */

	/* Payload (variable) */
	PayloadOffset = StreamPointer(s);

	if (message.TargetName.Len > 0)
		ntlm_read_message_fields_buffer(s, &(message.TargetName));

	if (message.TargetInfo.Len > 0)
	{
		ntlm_read_message_fields_buffer(s, &(message.TargetInfo));

		context->ChallengeTargetInfo.pvBuffer = message.TargetInfo.Buffer;
		context->ChallengeTargetInfo.cbBuffer = message.TargetInfo.Len;

		AvTimestamp = ntlm_av_pair_get((NTLM_AV_PAIR*) message.TargetInfo.Buffer, MsvAvTimestamp);

		if (AvTimestamp != NULL)
		{
			if (context->NTLMv2)
				context->UseMIC = TRUE;

			CopyMemory(context->ChallengeTimestamp, ntlm_av_pair_get_value_pointer(AvTimestamp), 8);
		}
	}

	length = (PayloadOffset - StartOffset) + message.TargetName.Len + message.TargetInfo.Len;

	sspi_SecBufferAlloc(&context->ChallengeMessage, length);
	CopyMemory(context->ChallengeMessage.pvBuffer, StartOffset, length);

#ifdef WITH_DEBUG_NTLM
	printf("CHALLENGE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(context->ChallengeMessage.pvBuffer, context->ChallengeMessage.cbBuffer);
	printf("\n");

	ntlm_print_negotiate_flags(context->NegotiateFlags);

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message.Version));

	ntlm_print_message_fields(&(message.TargetName), "TargetName");
	ntlm_print_message_fields(&(message.TargetInfo), "TargetInfo");
#endif
	/* AV_PAIRs */

	if (context->NTLMv2)
	{
		ntlm_construct_authenticate_target_info(context);
		context->ChallengeTargetInfo.pvBuffer = context->AuthenticateTargetInfo.pvBuffer;
		context->ChallengeTargetInfo.cbBuffer = context->AuthenticateTargetInfo.cbBuffer;
	}

	/* Timestamp */
	ntlm_generate_timestamp(context);

	/* LmChallengeResponse */
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

	ZeroMemory(&message, sizeof(message));
	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	/* Version */
	ntlm_get_version_info(&(message.Version));

	/* Server Challenge */
	ntlm_generate_server_challenge(context);

	/* Timestamp */
	ntlm_generate_timestamp(context);

	/* TargetInfo */
	ntlm_construct_challenge_target_info(context);

	/* ServerChallenge */
	CopyMemory(message.ServerChallenge, context->ServerChallenge, 8);

	message.NegotiateFlags = context->NegotiateFlags;

	ntlm_populate_message_header((NTLM_MESSAGE_HEADER*) &message, MESSAGE_TYPE_CHALLENGE);

	/* Message Header (12 bytes) */
	ntlm_write_message_header(s, (NTLM_MESSAGE_HEADER*) &message);

	if (message.NegotiateFlags & NTLMSSP_REQUEST_TARGET)
	{
		message.TargetName.Len = (UINT16) context->TargetName.cbBuffer;
		message.TargetName.Buffer = context->TargetName.pvBuffer;
	}

	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_TARGET_INFO;

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_TARGET_INFO)
	{
		message.TargetInfo.Len = (UINT16) context->ChallengeTargetInfo.cbBuffer;
		message.TargetInfo.Buffer = context->ChallengeTargetInfo.pvBuffer;
	}

	PayloadOffset = 48;

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		PayloadOffset += 8;

	message.TargetName.BufferOffset = PayloadOffset;
	message.TargetInfo.BufferOffset = message.TargetName.BufferOffset + message.TargetName.Len;

	/* TargetNameFields (8 bytes) */
	ntlm_write_message_fields(s, &(message.TargetName));

	StreamWrite_UINT32(s, message.NegotiateFlags); /* NegotiateFlags (4 bytes) */

	StreamWrite(s, message.ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	StreamWrite(s, message.Reserved, 8); /* Reserved (8 bytes), should be ignored */

	/* TargetInfoFields (8 bytes) */
	ntlm_write_message_fields(s, &(message.TargetInfo));

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_write_version_info(s, &(message.Version)); /* Version (8 bytes) */

	/* Payload (variable) */

	if (message.NegotiateFlags & NTLMSSP_REQUEST_TARGET)
		ntlm_write_message_fields_buffer(s, &(message.TargetName));

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_TARGET_INFO)
		ntlm_write_message_fields_buffer(s, &(message.TargetInfo));

	length = StreamGetPosition(s);
	buffer->cbBuffer = length;

	sspi_SecBufferAlloc(&context->ChallengeMessage, length);
	CopyMemory(context->ChallengeMessage.pvBuffer, s->buffer, length);

#ifdef WITH_DEBUG_NTLM
	printf("CHALLENGE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(context->ChallengeMessage.pvBuffer, context->ChallengeMessage.cbBuffer);
	printf("\n");

	ntlm_print_negotiate_flags(message.NegotiateFlags);

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message.Version));

	ntlm_print_message_fields(&(message.TargetName), "TargetName");
	ntlm_print_message_fields(&(message.TargetInfo), "TargetInfo");
#endif

	context->state = NTLM_STATE_AUTHENTICATE;

	PStreamFreeDetach(s);

	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_read_AuthenticateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	PStream s;
	int length;
	UINT32 flags;
	UINT32 MicOffset;
	NTLM_AV_PAIR* AvFlags;
	NTLMv2_RESPONSE response;
	UINT32 PayloadBufferOffset;
	NTLM_AUTHENTICATE_MESSAGE message;

	flags = 0;
	MicOffset = 0;
	AvFlags = NULL;

	ZeroMemory(&message, sizeof(message));
	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	ntlm_read_message_header(s, (NTLM_MESSAGE_HEADER*) &message);

	if (!ntlm_validate_message_header(s, (NTLM_MESSAGE_HEADER*) &message, MESSAGE_TYPE_AUTHENTICATE))
	{
		PStreamFreeDetach(s);
		return SEC_E_INVALID_TOKEN;
	}

	/* LmChallengeResponseFields (8 bytes) */
	ntlm_read_message_fields(s, &(message.LmChallengeResponse));

	/* NtChallengeResponseFields (8 bytes) */
	ntlm_read_message_fields(s, &(message.NtChallengeResponse));

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	ntlm_read_message_fields(s, &(message.DomainName));

	/* UserNameFields (8 bytes) */
	ntlm_read_message_fields(s, &(message.UserName));

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	ntlm_read_message_fields(s, &(message.Workstation));

	/* EncryptedRandomSessionKeyFields (8 bytes) */
	ntlm_read_message_fields(s, &(message.EncryptedRandomSessionKey));

	StreamRead_UINT32(s, message.NegotiateFlags); /* NegotiateFlags (4 bytes) */

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_read_version_info(s, &(message.Version)); /* Version (8 bytes) */

	PayloadBufferOffset = StreamGetPosition(s);

	/* DomainName */
	ntlm_read_message_fields_buffer(s, &(message.DomainName));

	/* UserName */
	ntlm_read_message_fields_buffer(s, &(message.UserName));

	/* Workstation */
	ntlm_read_message_fields_buffer(s, &(message.Workstation));

	/* LmChallengeResponse */
	ntlm_read_message_fields_buffer(s, &(message.LmChallengeResponse));

	/* NtChallengeResponse */
	ntlm_read_message_fields_buffer(s, &(message.NtChallengeResponse));

	if (message.NtChallengeResponse.Len > 0)
	{
		PStream s = PStreamAllocAttach(message.NtChallengeResponse.Buffer, message.NtChallengeResponse.Len);
		ntlm_read_ntlm_v2_response(s, &response);
		PStreamFreeDetach(s);

		context->NtChallengeResponse.pvBuffer = message.NtChallengeResponse.Buffer;
		context->NtChallengeResponse.cbBuffer = message.NtChallengeResponse.Len;

		context->ChallengeTargetInfo.pvBuffer = (void*) response.Challenge.AvPairs;
		context->ChallengeTargetInfo.cbBuffer = message.NtChallengeResponse.Len - (28 + 16);

		CopyMemory(context->ClientChallenge, response.Challenge.ClientChallenge, 8);

		AvFlags = ntlm_av_pair_get(response.Challenge.AvPairs, MsvAvFlags);

		if (AvFlags != NULL)
			flags = *((UINT32*) ntlm_av_pair_get_value_pointer(AvFlags));
	}

	/* EncryptedRandomSessionKey */
	ntlm_read_message_fields_buffer(s, &(message.EncryptedRandomSessionKey));
	CopyMemory(context->EncryptedRandomSessionKey, message.EncryptedRandomSessionKey.Buffer, 16);

	length = StreamGetPosition(s);
	sspi_SecBufferAlloc(&context->AuthenticateMessage, length);
	CopyMemory(context->AuthenticateMessage.pvBuffer, s->buffer, length);
	buffer->cbBuffer = length;

	StreamSetPosition(s, PayloadBufferOffset);

	if (flags & MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK)
	{
		MicOffset = StreamGetPosition(s);
		StreamRead(s, message.MessageIntegrityCheck, 16);
		PayloadBufferOffset += 16;
	}

#ifdef WITH_DEBUG_NTLM
	printf("AUTHENTICATE_MESSAGE (length = %d)\n", (int) context->AuthenticateMessage.cbBuffer);
	winpr_HexDump(context->AuthenticateMessage.pvBuffer, context->AuthenticateMessage.cbBuffer);
	printf("\n");

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message.Version));

	ntlm_print_message_fields(&(message.DomainName), "DomainName");
	ntlm_print_message_fields(&(message.UserName), "UserName");
	ntlm_print_message_fields(&(message.Workstation), "Workstation");
	ntlm_print_message_fields(&(message.LmChallengeResponse), "LmChallengeResponse");
	ntlm_print_message_fields(&(message.NtChallengeResponse), "NtChallengeResponse");
	ntlm_print_message_fields(&(message.EncryptedRandomSessionKey), "EncryptedRandomSessionKey");

	ntlm_print_av_pair_list(response.Challenge.AvPairs);

	if (flags & MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK)
	{
		printf("MessageIntegrityCheck:\n");
		winpr_HexDump(message.MessageIntegrityCheck, 16);
	}
#endif

	if (message.UserName.Len > 0)
	{
		context->identity.User = (UINT16*) malloc(message.UserName.Len);
		CopyMemory(context->identity.User, message.UserName.Buffer, message.UserName.Len);
		context->identity.UserLength = message.UserName.Len / 2;
	}

	if (message.DomainName.Len > 0)
	{
		context->identity.Domain = (UINT16*) malloc(message.DomainName.Len);
		CopyMemory(context->identity.Domain, message.DomainName.Buffer, message.DomainName.Len);
		context->identity.DomainLength = message.DomainName.Len / 2;
	}

	/* LmChallengeResponse */
	ntlm_compute_lm_v2_response(context);

	/* NtChallengeResponse */
	ntlm_compute_ntlm_v2_response(context);

	/* KeyExchangeKey */
	ntlm_generate_key_exchange_key(context);

	/* EncryptedRandomSessionKey */
	ntlm_decrypt_random_session_key(context);

	/* ExportedSessionKey */
	ntlm_generate_exported_session_key(context);

	if (flags & MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK)
	{
		ZeroMemory(&((PBYTE) context->AuthenticateMessage.pvBuffer)[MicOffset], 16);
		ntlm_compute_message_integrity_check(context);
		CopyMemory(&((PBYTE) context->AuthenticateMessage.pvBuffer)[MicOffset], message.MessageIntegrityCheck, 16);

		if (memcmp(context->MessageIntegrityCheck, message.MessageIntegrityCheck, 16) != 0)
		{
			printf("Message Integrity Check (MIC) verification failed!\n");

			printf("Expected MIC:\n");
			winpr_HexDump(context->MessageIntegrityCheck, 16);
			printf("Actual MIC:\n");
			winpr_HexDump(message.MessageIntegrityCheck, 16);

			return SEC_E_MESSAGE_ALTERED;
		}
	}

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
	UINT32 MicOffset = 0;
	UINT32 PayloadBufferOffset;
	NTLM_AUTHENTICATE_MESSAGE message;

	ZeroMemory(&message, sizeof(message));
	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	if (context->NTLMv2)
	{
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_56;
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
	}

	if (context->UseMIC)
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_TARGET_INFO;

	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_128;
	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
	message.NegotiateFlags |= NTLMSSP_REQUEST_TARGET;
	message.NegotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_get_version_info(&(message.Version));

	message.Workstation.Len = context->Workstation.Length;
	message.Workstation.Buffer = (BYTE*) context->Workstation.Buffer;

	if (!context->NTLMv2)
		message.Workstation.Len = 0;

	if (message.Workstation.Len > 0)
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED;

	message.DomainName.Len = (UINT16) context->identity.DomainLength * 2;
	message.DomainName.Buffer = (BYTE*) context->identity.Domain;

	if (message.DomainName.Len > 0)
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED;

	message.UserName.Len = (UINT16) context->identity.UserLength * 2;
	message.UserName.Buffer = (BYTE*) context->identity.User;

	message.LmChallengeResponse.Len = (UINT16) context->LmChallengeResponse.cbBuffer;
	message.LmChallengeResponse.Buffer = (BYTE*) context->LmChallengeResponse.pvBuffer;

	message.NtChallengeResponse.Len = (UINT16) context->NtChallengeResponse.cbBuffer;
	message.NtChallengeResponse.Buffer = (BYTE*) context->NtChallengeResponse.pvBuffer;

	message.EncryptedRandomSessionKey.Len = 16;
	message.EncryptedRandomSessionKey.Buffer = context->EncryptedRandomSessionKey;

	if (context->confidentiality)
		message.NegotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;

	PayloadBufferOffset = 64;

	if (context->UseMIC)
		PayloadBufferOffset += 16; /* Message Integrity Check */

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		PayloadBufferOffset += 8;

	message.DomainName.BufferOffset = PayloadBufferOffset;
	message.UserName.BufferOffset = message.DomainName.BufferOffset + message.DomainName.Len;
	message.Workstation.BufferOffset = message.UserName.BufferOffset + message.UserName.Len;
	message.LmChallengeResponse.BufferOffset = message.Workstation.BufferOffset + message.Workstation.Len;
	message.NtChallengeResponse.BufferOffset = message.LmChallengeResponse.BufferOffset + message.LmChallengeResponse.Len;
	message.EncryptedRandomSessionKey.BufferOffset = message.NtChallengeResponse.BufferOffset + message.NtChallengeResponse.Len;

	ntlm_populate_message_header((NTLM_MESSAGE_HEADER*) &message, MESSAGE_TYPE_AUTHENTICATE);

	/* Message Header (12 bytes) */
	ntlm_write_message_header(s, (NTLM_MESSAGE_HEADER*) &message);

	/* LmChallengeResponseFields (8 bytes) */
	ntlm_write_message_fields(s, &(message.LmChallengeResponse));

	/* NtChallengeResponseFields (8 bytes) */
	ntlm_write_message_fields(s, &(message.NtChallengeResponse));

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	ntlm_write_message_fields(s, &(message.DomainName));

	/* UserNameFields (8 bytes) */
	ntlm_write_message_fields(s, &(message.UserName));

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	ntlm_write_message_fields(s, &(message.Workstation));

	/* EncryptedRandomSessionKeyFields (8 bytes) */
	ntlm_write_message_fields(s, &(message.EncryptedRandomSessionKey));

	StreamWrite_UINT32(s, message.NegotiateFlags); /* NegotiateFlags (4 bytes) */

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_write_version_info(s, &(message.Version));

	if (context->UseMIC)
	{
		/* Message Integrity Check */
		MicOffset = StreamGetPosition(s);
		StreamZero(s, 16);
	}

	/* DomainName */
	ntlm_write_message_fields_buffer(s, &(message.DomainName));

	/* UserName */
	ntlm_write_message_fields_buffer(s, &(message.UserName));

	/* Workstation */
	ntlm_write_message_fields_buffer(s, &(message.Workstation));

	/* LmChallengeResponse */
	ntlm_write_message_fields_buffer(s, &(message.LmChallengeResponse));

	/* NtChallengeResponse */
	ntlm_write_message_fields_buffer(s, &(message.NtChallengeResponse));

	/* EncryptedRandomSessionKey */
	ntlm_write_message_fields_buffer(s, &(message.EncryptedRandomSessionKey));

	length = StreamGetPosition(s);
	sspi_SecBufferAlloc(&context->AuthenticateMessage, length);
	CopyMemory(context->AuthenticateMessage.pvBuffer, s->buffer, length);
	buffer->cbBuffer = length;

	if (context->UseMIC)
	{
		/* Message Integrity Check */
		ntlm_compute_message_integrity_check(context);

		StreamSetPosition(s, MicOffset);
		StreamWrite(s, context->MessageIntegrityCheck, 16);
		StreamSetPosition(s, length);
	}

#ifdef WITH_DEBUG_NTLM
	printf("AUTHENTICATE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(s->buffer, length);
	printf("\n");

	ntlm_print_negotiate_flags(message.NegotiateFlags);

	if (message.NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message.Version));

	ntlm_print_message_fields(&(message.DomainName), "DomainName");
	ntlm_print_message_fields(&(message.UserName), "UserName");
	ntlm_print_message_fields(&(message.Workstation), "Workstation");
	ntlm_print_message_fields(&(message.LmChallengeResponse), "LmChallengeResponse");
	ntlm_print_message_fields(&(message.NtChallengeResponse), "NtChallengeResponse");
	ntlm_print_message_fields(&(message.EncryptedRandomSessionKey), "EncryptedRandomSessionKey");

	if (context->UseMIC)
	{
		printf("MessageIntegrityCheck (length = 16)\n");
		winpr_HexDump(context->MessageIntegrityCheck, 16);
		printf("\n");
	}
#endif

	context->state = NTLM_STATE_FINAL;

	PStreamFreeDetach(s);

	return SEC_I_COMPLETE_NEEDED;
}
