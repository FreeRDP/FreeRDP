/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NTLM Security Package (AV_PAIRs)
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

#include "ntlm_av_pairs.h"

const char* const AV_PAIRS_STRINGS[] =
{
	"MsvAvEOL",
	"MsvAvNbComputerName",
	"MsvAvNbDomainName",
	"MsvAvDnsComputerName",
	"MsvAvDnsDomainName",
	"MsvAvDnsTreeName",
	"MsvAvFlags",
	"MsvAvTimestamp",
	"MsvAvRestrictions",
	"MsvAvTargetName",
	"MsvChannelBindings"
};

/**
 * Input array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 * @param s
 */

void ntlm_input_av_pairs(NTLM_CONTEXT* context, STREAM* s)
{
	AV_ID AvId;
	uint16 AvLen;
	uint8* value;
	AV_PAIRS* av_pairs = context->av_pairs;

#ifdef WITH_DEBUG_NTLM
	printf("AV_PAIRS = {\n");
#endif

	do
	{
		value = NULL;
		stream_read_uint16(s, AvId);
		stream_read_uint16(s, AvLen);

		if (AvLen > 0)
		{
			if (AvId != MsvAvFlags)
			{
				value = xmalloc(AvLen);
				stream_read(s, value, AvLen);
			}
			else
			{
				stream_read_uint32(s, av_pairs->Flags);
			}
		}

		switch (AvId)
		{
			case MsvAvNbComputerName:
				av_pairs->NbComputerName.length = AvLen;
				av_pairs->NbComputerName.value = value;
				break;

			case MsvAvNbDomainName:
				av_pairs->NbDomainName.length = AvLen;
				av_pairs->NbDomainName.value = value;
				break;

			case MsvAvDnsComputerName:
				av_pairs->DnsComputerName.length = AvLen;
				av_pairs->DnsComputerName.value = value;
				break;

			case MsvAvDnsDomainName:
				av_pairs->DnsDomainName.length = AvLen;
				av_pairs->DnsDomainName.value = value;
				break;

			case MsvAvDnsTreeName:
				av_pairs->DnsTreeName.length = AvLen;
				av_pairs->DnsTreeName.value = value;
				break;

			case MsvAvTimestamp:
				av_pairs->Timestamp.length = AvLen;
				av_pairs->Timestamp.value = value;
				break;

			case MsvAvRestrictions:
				av_pairs->Restrictions.length = AvLen;
				av_pairs->Restrictions.value = value;
				break;

			case MsvAvTargetName:
				av_pairs->TargetName.length = AvLen;
				av_pairs->TargetName.value = value;
				break;

			case MsvChannelBindings:
				av_pairs->ChannelBindings.length = AvLen;
				av_pairs->ChannelBindings.value = value;
				break;

			default:
				if (value != NULL)
					xfree(value);
				break;
		}

#ifdef WITH_DEBUG_NTLM
		if (AvId < 10)
			printf("\tAvId: %s, AvLen: %d\n", AV_PAIRS_STRINGS[AvId], AvLen);
		else
			printf("\tAvId: %s, AvLen: %d\n", "Unknown", AvLen);

		freerdp_hexdump(value, AvLen);
#endif
	}
	while (AvId != MsvAvEOL);

#ifdef WITH_DEBUG_NTLM
	printf("}\n");
#endif
}

/**
 * Output array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 * @param s
 */

void ntlm_output_av_pairs(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	STREAM* s;
	AV_PAIRS* av_pairs = context->av_pairs;

	s = stream_new(0);
	stream_attach(s, buffer->pvBuffer, buffer->cbBuffer);

	if (av_pairs->NbDomainName.length > 0)
	{
		stream_write_uint16(s, MsvAvNbDomainName); /* AvId */
		stream_write_uint16(s, av_pairs->NbDomainName.length); /* AvLen */
		stream_write(s, av_pairs->NbDomainName.value, av_pairs->NbDomainName.length); /* Value */
	}

	if (av_pairs->NbComputerName.length > 0)
	{
		stream_write_uint16(s, MsvAvNbComputerName); /* AvId */
		stream_write_uint16(s, av_pairs->NbComputerName.length); /* AvLen */
		stream_write(s, av_pairs->NbComputerName.value, av_pairs->NbComputerName.length); /* Value */
	}

	if (av_pairs->DnsDomainName.length > 0)
	{
		stream_write_uint16(s, MsvAvDnsDomainName); /* AvId */
		stream_write_uint16(s, av_pairs->DnsDomainName.length); /* AvLen */
		stream_write(s, av_pairs->DnsDomainName.value, av_pairs->DnsDomainName.length); /* Value */
	}

	if (av_pairs->DnsComputerName.length > 0)
	{
		stream_write_uint16(s, MsvAvDnsComputerName); /* AvId */
		stream_write_uint16(s, av_pairs->DnsComputerName.length); /* AvLen */
		stream_write(s, av_pairs->DnsComputerName.value, av_pairs->DnsComputerName.length); /* Value */
	}

	if (av_pairs->DnsTreeName.length > 0)
	{
		stream_write_uint16(s, MsvAvDnsTreeName); /* AvId */
		stream_write_uint16(s, av_pairs->DnsTreeName.length); /* AvLen */
		stream_write(s, av_pairs->DnsTreeName.value, av_pairs->DnsTreeName.length); /* Value */
	}

	if (av_pairs->Timestamp.length > 0)
	{
		stream_write_uint16(s, MsvAvTimestamp); /* AvId */
		stream_write_uint16(s, av_pairs->Timestamp.length); /* AvLen */
		stream_write(s, av_pairs->Timestamp.value, av_pairs->Timestamp.length); /* Value */
	}

	if (av_pairs->Flags > 0)
	{
		stream_write_uint16(s, MsvAvFlags); /* AvId */
		stream_write_uint16(s, 4); /* AvLen */
		stream_write_uint32(s, av_pairs->Flags); /* Value */
	}

	if (av_pairs->Restrictions.length > 0)
	{
		stream_write_uint16(s, MsvAvRestrictions); /* AvId */
		stream_write_uint16(s, av_pairs->Restrictions.length); /* AvLen */
		stream_write(s, av_pairs->Restrictions.value, av_pairs->Restrictions.length); /* Value */
	}

	if (av_pairs->ChannelBindings.length > 0)
	{
		stream_write_uint16(s, MsvChannelBindings); /* AvId */
		stream_write_uint16(s, av_pairs->ChannelBindings.length); /* AvLen */
		stream_write(s, av_pairs->ChannelBindings.value, av_pairs->ChannelBindings.length); /* Value */
	}

	if (av_pairs->TargetName.length > 0)
	{
		stream_write_uint16(s, MsvAvTargetName); /* AvId */
		stream_write_uint16(s, av_pairs->TargetName.length); /* AvLen */
		stream_write(s, av_pairs->TargetName.value, av_pairs->TargetName.length); /* Value */
	}

	/* This indicates the end of the AV_PAIR array */
	stream_write_uint16(s, MsvAvEOL); /* AvId */
	stream_write_uint16(s, 0); /* AvLen */

	if (context->ntlm_v2)
	{
		stream_write_zero(s, 8);
	}

	xfree(s);
}

/**
 * Compute AV_PAIRs length.\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 */

int ntlm_compute_av_pairs_length(NTLM_CONTEXT* context)
{
	int length = 0;
	AV_PAIRS* av_pairs = context->av_pairs;

	if (av_pairs->NbDomainName.length > 0)
		length += av_pairs->NbDomainName.length + 4;

	if (av_pairs->NbComputerName.length > 0)
		length += av_pairs->NbComputerName.length + 4;

	if (av_pairs->DnsDomainName.length > 0)
		length += av_pairs->DnsDomainName.length + 4;

	if (av_pairs->DnsComputerName.length > 0)
		length += av_pairs->DnsComputerName.length + 4;

	if (av_pairs->DnsTreeName.length > 0)
		length += av_pairs->DnsTreeName.length + 4;

	if (av_pairs->Timestamp.length > 0)
		length += av_pairs->Timestamp.length;

	if (av_pairs->Flags > 0)
		length += 4 + 4;

	if (av_pairs->Restrictions.length > 0)
		length += av_pairs->Restrictions.length + 4;

	if (av_pairs->ChannelBindings.length > 0)
		length += av_pairs->ChannelBindings.length + 4;

	if (av_pairs->TargetName.length > 0)
		length += av_pairs->TargetName.length + 4;

	length += 4;

	if (context->ntlm_v2)
		length += 8;

	return length;
}

/**
 * Populate array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 */

void ntlm_populate_av_pairs(NTLM_CONTEXT* context)
{
	int length;
	AV_PAIRS* av_pairs = context->av_pairs;

	/* MsvAvFlags */
	av_pairs->Flags = 0x00000002; /* Indicates the present of a Message Integrity Check (MIC) */

	/* Restriction_Encoding */
	ntlm_output_restriction_encoding(context);

	/* TargetName */
	ntlm_output_target_name(context);

	/* ChannelBindings */
	ntlm_output_channel_bindings(context);

	length = ntlm_compute_av_pairs_length(context);
	sspi_SecBufferAlloc(&context->TargetInfo, length);
	ntlm_output_av_pairs(context, &context->TargetInfo);
}

/**
 * Populate array of AV_PAIRs (server).\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 */

char* test_NbDomainName = "FREERDP";
char* test_NbComputerName = "FREERDP";
char* test_DnsDomainName = "FreeRDP";
char* test_DnsComputerName = "FreeRDP";

void ntlm_populate_server_av_pairs(NTLM_CONTEXT* context)
{
	int length;
	size_t size;
	AV_PAIRS* av_pairs = context->av_pairs;

	av_pairs->NbDomainName.value = (uint8*) freerdp_uniconv_out(context->uniconv, test_NbDomainName, &size);
	av_pairs->NbDomainName.length = (uint16) size;

	av_pairs->NbComputerName.value = (uint8*) freerdp_uniconv_out(context->uniconv, test_NbComputerName, &size);
	av_pairs->NbComputerName.length = (uint16) size;

	av_pairs->DnsDomainName.value = (uint8*) freerdp_uniconv_out(context->uniconv, test_DnsDomainName, &size);
	av_pairs->DnsDomainName.length = (uint16) size;

	av_pairs->DnsComputerName.value = (uint8*) freerdp_uniconv_out(context->uniconv, test_DnsComputerName, &size);
	av_pairs->DnsComputerName.length = (uint16) size;

	length = ntlm_compute_av_pairs_length(context) + 4;
	sspi_SecBufferAlloc(&context->TargetInfo, length);
	ntlm_output_av_pairs(context, &context->TargetInfo);
}

/**
 * Print array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 */

void ntlm_print_av_pairs(NTLM_CONTEXT* context)
{
	AV_PAIRS* av_pairs = context->av_pairs;

	printf("AV_PAIRS = {\n");

	if (av_pairs->NbDomainName.length > 0)
	{
		printf("\tAvId: MsvAvNbDomainName AvLen: %d\n", av_pairs->NbDomainName.length);
		freerdp_hexdump(av_pairs->NbDomainName.value, av_pairs->NbDomainName.length);
	}

	if (av_pairs->NbComputerName.length > 0)
	{
		printf("\tAvId: MsvAvNbComputerName AvLen: %d\n", av_pairs->NbComputerName.length);
		freerdp_hexdump(av_pairs->NbComputerName.value, av_pairs->NbComputerName.length);
	}

	if (av_pairs->DnsDomainName.length > 0)
	{
		printf("\tAvId: MsvAvDnsDomainName AvLen: %d\n", av_pairs->DnsDomainName.length);
		freerdp_hexdump(av_pairs->DnsDomainName.value, av_pairs->DnsDomainName.length);
	}

	if (av_pairs->DnsComputerName.length > 0)
	{
		printf("\tAvId: MsvAvDnsComputerName AvLen: %d\n", av_pairs->DnsComputerName.length);
		freerdp_hexdump(av_pairs->DnsComputerName.value, av_pairs->DnsComputerName.length);
	}

	if (av_pairs->DnsTreeName.length > 0)
	{
		printf("\tAvId: MsvAvDnsTreeName AvLen: %d\n", av_pairs->DnsTreeName.length);
		freerdp_hexdump(av_pairs->DnsTreeName.value, av_pairs->DnsTreeName.length);
	}

	if (av_pairs->Timestamp.length > 0)
	{
		printf("\tAvId: MsvAvTimestamp AvLen: %d\n", av_pairs->Timestamp.length);
		freerdp_hexdump(av_pairs->Timestamp.value, av_pairs->Timestamp.length);
	}

	if (av_pairs->Flags > 0)
	{
		printf("\tAvId: MsvAvFlags AvLen: %d\n", 4);
		printf("0x%08X\n", av_pairs->Flags);
	}

	if (av_pairs->Restrictions.length > 0)
	{
		printf("\tAvId: MsvAvRestrictions AvLen: %d\n", av_pairs->Restrictions.length);
		freerdp_hexdump(av_pairs->Restrictions.value, av_pairs->Restrictions.length);
	}

	if (av_pairs->ChannelBindings.length > 0)
	{
		printf("\tAvId: MsvChannelBindings AvLen: %d\n", av_pairs->ChannelBindings.length);
		freerdp_hexdump(av_pairs->ChannelBindings.value, av_pairs->ChannelBindings.length);
	}

	if (av_pairs->TargetName.length > 0)
	{
		printf("\tAvId: MsvAvTargetName AvLen: %d\n", av_pairs->TargetName.length);
		freerdp_hexdump(av_pairs->TargetName.value, av_pairs->TargetName.length);
	}

	printf("}\n");
}

/**
 * Free array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 */

void ntlm_free_av_pairs(NTLM_CONTEXT* context)
{
	AV_PAIRS* av_pairs = context->av_pairs;

	if (av_pairs != NULL)
	{
		if (av_pairs->NbComputerName.value != NULL)
			xfree(av_pairs->NbComputerName.value);
		if (av_pairs->NbDomainName.value != NULL)
			xfree(av_pairs->NbDomainName.value);
		if (av_pairs->DnsComputerName.value != NULL)
			xfree(av_pairs->DnsComputerName.value);
		if (av_pairs->DnsDomainName.value != NULL)
			xfree(av_pairs->DnsDomainName.value);
		if (av_pairs->DnsTreeName.value != NULL)
			xfree(av_pairs->DnsTreeName.value);
		if (av_pairs->Timestamp.value != NULL)
			xfree(av_pairs->Timestamp.value);
		if (av_pairs->Restrictions.value != NULL)
			xfree(av_pairs->Restrictions.value);
		if (av_pairs->TargetName.value != NULL)
			xfree(av_pairs->TargetName.value);
		if (av_pairs->ChannelBindings.value != NULL)
			xfree(av_pairs->ChannelBindings.value);

		xfree(av_pairs);
	}

	context->av_pairs = NULL;
}
