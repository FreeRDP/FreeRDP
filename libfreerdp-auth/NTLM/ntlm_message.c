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
	}

	length = s->p - s->data;
	buffer->cbBuffer = length;

	//freerdp_blob_alloc(&context->negotiate_message, length);
	//memcpy(context->negotiate_message.data, s->data, length);

	context->state = NTLM_STATE_CHALLENGE;

	return SEC_I_CONTINUE_NEEDED;
}
