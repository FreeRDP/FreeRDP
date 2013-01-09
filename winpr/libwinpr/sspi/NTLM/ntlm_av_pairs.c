/**
 * WinPR: Windows Portable Runtime
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ntlm.h"
#include "../sspi.h"

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/sysinfo.h>

#include "ntlm_compute.h"

#include "ntlm_av_pairs.h"

const char* const AV_PAIR_STRINGS[] =
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

void ntlm_av_pair_list_init(NTLM_AV_PAIR* pAvPairList)
{
	NTLM_AV_PAIR* pAvPair = pAvPairList;

	pAvPair->AvId = MsvAvEOL;
	pAvPair->AvLen = 0;
}

ULONG ntlm_av_pair_list_length(NTLM_AV_PAIR* pAvPairList)
{
	ULONG length;
	NTLM_AV_PAIR* pAvPair = pAvPairList;

	if (!pAvPair)
		return 0;

	while (pAvPair->AvId != MsvAvEOL)
	{
		pAvPair = ntlm_av_pair_get_next_pointer(pAvPair);
	}

	length = (pAvPair - pAvPairList) + sizeof(NTLM_AV_PAIR);

	return length;
}

void ntlm_print_av_pair_list(NTLM_AV_PAIR* pAvPairList)
{
	NTLM_AV_PAIR* pAvPair = pAvPairList;

	if (!pAvPair)
		return;

	printf("AV_PAIRs =\n{\n");

	while (pAvPair->AvId != MsvAvEOL)
	{
		printf("\t%s AvId: %d AvLen: %d\n",
				AV_PAIR_STRINGS[pAvPair->AvId],
				pAvPair->AvId, pAvPair->AvLen);

		winpr_HexDump(ntlm_av_pair_get_value_pointer(pAvPair), pAvPair->AvLen);

		pAvPair = ntlm_av_pair_get_next_pointer(pAvPair);
	}

	printf("}\n");
}

ULONG ntlm_av_pair_list_size(ULONG AvPairsCount, ULONG AvPairsValueLength)
{
	/* size of headers + value lengths + terminating MsvAvEOL AV_PAIR */
	return (AvPairsCount + 1) * sizeof(NTLM_AV_PAIR) + AvPairsValueLength;
}

PBYTE ntlm_av_pair_get_value_pointer(NTLM_AV_PAIR* pAvPair)
{
	return &((PBYTE) pAvPair)[sizeof(NTLM_AV_PAIR)];
}

int ntlm_av_pair_get_next_offset(NTLM_AV_PAIR* pAvPair)
{
	return pAvPair->AvLen + sizeof(NTLM_AV_PAIR);
}

NTLM_AV_PAIR* ntlm_av_pair_get_next_pointer(NTLM_AV_PAIR* pAvPair)
{
	return (NTLM_AV_PAIR*) ((PBYTE) pAvPair + ntlm_av_pair_get_next_offset(pAvPair));
}

NTLM_AV_PAIR* ntlm_av_pair_get(NTLM_AV_PAIR* pAvPairList, NTLM_AV_ID AvId)
{
	NTLM_AV_PAIR* pAvPair = pAvPairList;

	if (!pAvPair)
		return NULL;

	while (1)
	{
		if (pAvPair->AvId == AvId)
			return pAvPair;

		if (pAvPair->AvId == MsvAvEOL)
			return NULL;

		pAvPair = ntlm_av_pair_get_next_pointer(pAvPair);
	}

	return NULL;
}

NTLM_AV_PAIR* ntlm_av_pair_add(NTLM_AV_PAIR* pAvPairList, NTLM_AV_ID AvId, PBYTE Value, UINT16 AvLen)
{
	NTLM_AV_PAIR* pAvPair;

	pAvPair = ntlm_av_pair_get(pAvPairList, MsvAvEOL);

	if (!pAvPair)
		return NULL;

	pAvPair->AvId = AvId;
	pAvPair->AvLen = AvLen;

	CopyMemory(ntlm_av_pair_get_value_pointer(pAvPair), Value, AvLen);

	return pAvPair;
}

NTLM_AV_PAIR* ntlm_av_pair_add_copy(NTLM_AV_PAIR* pAvPairList, NTLM_AV_PAIR* pAvPair)
{
	NTLM_AV_PAIR* pAvPairCopy;

	pAvPairCopy = ntlm_av_pair_get(pAvPairList, MsvAvEOL);

	if (!pAvPairCopy)
		return NULL;

	pAvPairCopy->AvId = pAvPair->AvId;
	pAvPairCopy->AvLen = pAvPair->AvLen;

	CopyMemory(ntlm_av_pair_get_value_pointer(pAvPairCopy),
			ntlm_av_pair_get_value_pointer(pAvPair), pAvPair->AvLen);

	return pAvPairCopy;
}

void ntlm_get_target_computer_name(PUNICODE_STRING pName, COMPUTER_NAME_FORMAT type)
{
	char* name;
	int length;
	DWORD nSize = 0;

	GetComputerNameExA(type, NULL, &nSize);
	name = malloc(nSize);
	GetComputerNameExA(type, name, &nSize);

	if (type == ComputerNameNetBIOS)
		CharUpperA(name);

	length = ConvertToUnicode(CP_UTF8, 0, name, -1, &pName->Buffer, 0);

	pName->Length = (length - 1) / 2;
	pName->MaximumLength = pName->Length;

	free(name);
}

void ntlm_free_unicode_string(PUNICODE_STRING string)
{
	if (string != NULL)
	{
		if (string->Length > 0)
		{
			if (string->Buffer != NULL)
				free(string->Buffer);

			string->Buffer = NULL;
			string->Length = 0;
			string->MaximumLength = 0;
		}
	}
}

/**
 * From http://www.ietf.org/proceedings/72/slides/sasl-2.pdf:
 *
 * tls-server-end-point:
 *
 * The hash of the TLS server's end entity certificate as it appears, octet for octet,
 * in the server's Certificate message (note that the Certificate message contains a
 * certificate_list, the first element of which is the server's end entity certificate.)
 * The hash function to be selected is as follows: if the certificate's signature hash
 * algorithm is either MD5 or SHA-1, then use SHA-256, otherwise use the certificate's
 * signature hash algorithm.
 */

/**
 * Channel Bindings sample usage:
 * https://raw.github.com/mozilla/mozilla-central/master/extensions/auth/nsAuthSSPI.cpp
 */

/* Certificate */

BYTE test_Certificate[525] =
	"\x30\x82\x02\x09\x30\x82\x01\x76\xa0\x03\x02\x01\x02\x02\x10\xcb"
	"\x69\x79\xcd\x51\x75\xc5\xb7\x4b\x67\x30\x83\x6c\x78\x44\x27\x30"
	"\x09\x06\x05\x2b\x0e\x03\x02\x1d\x05\x00\x30\x16\x31\x14\x30\x12"
	"\x06\x03\x55\x04\x03\x13\x0b\x44\x43\x2d\x57\x53\x32\x30\x30\x38"
	"\x52\x32\x30\x1e\x17\x0d\x31\x32\x31\x31\x31\x37\x30\x30\x35\x39"
	"\x32\x31\x5a\x17\x0d\x33\x39\x31\x32\x33\x31\x32\x33\x35\x39\x35"
	"\x39\x5a\x30\x16\x31\x14\x30\x12\x06\x03\x55\x04\x03\x13\x0b\x44"
	"\x43\x2d\x57\x53\x32\x30\x30\x38\x52\x32\x30\x81\x9f\x30\x0d\x06"
	"\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x81\x8d\x00"
	"\x30\x81\x89\x02\x81\x81\x00\x9b\x00\xf8\x1a\x2d\x37\xc6\x8d\xa1"
	"\x39\x91\x46\xf3\x6a\x1b\xf9\x60\x6c\xb3\x6c\xa0\xac\xed\x85\xe0"
	"\x3f\xdc\x92\x86\x36\xbd\x64\xbf\x36\x51\xdb\x57\x3a\x8a\x82\x6b"
	"\xd8\x94\x17\x7b\xd3\x91\x11\x98\xef\x19\x06\x52\x30\x03\x73\x67"
	"\xc8\xed\x8e\xfa\x0b\x3d\x4c\xc9\x10\x63\x9f\xcf\xb4\xcf\x39\xd8"
	"\xfe\x99\xeb\x5b\x11\xf2\xfc\xfa\x86\x24\xd9\xff\xd9\x19\xf5\x69"
	"\xb4\xdf\x5a\x5a\xc4\x94\xb4\xb0\x07\x25\x97\x13\xad\x7e\x38\x14"
	"\xfb\xd6\x33\x65\x6f\xe6\xf7\x48\x4b\x2d\xb3\x51\x2e\x6d\xc7\xea"
	"\x11\x76\x9a\x2b\xf0\x00\x4d\x02\x03\x01\x00\x01\xa3\x60\x30\x5e"
	"\x30\x13\x06\x03\x55\x1d\x25\x04\x0c\x30\x0a\x06\x08\x2b\x06\x01"
	"\x05\x05\x07\x03\x01\x30\x47\x06\x03\x55\x1d\x01\x04\x40\x30\x3e"
	"\x80\x10\xeb\x65\x26\x03\x95\x4b\xd6\xc0\x54\x75\x78\x7c\xb6\x2a"
	"\xa1\xbb\xa1\x18\x30\x16\x31\x14\x30\x12\x06\x03\x55\x04\x03\x13"
	"\x0b\x44\x43\x2d\x57\x53\x32\x30\x30\x38\x52\x32\x82\x10\xcb\x69"
	"\x79\xcd\x51\x75\xc5\xb7\x4b\x67\x30\x83\x6c\x78\x44\x27\x30\x09"
	"\x06\x05\x2b\x0e\x03\x02\x1d\x05\x00\x03\x81\x81\x00\x7b\xfa\xfe"
	"\xee\x74\x05\xac\xbb\x79\xe9\xda\xca\x00\x44\x96\x94\x71\x92\xb1"
	"\xdb\xc9\x9b\x71\x29\xc0\xe4\x28\x5e\x6a\x50\x99\xcd\xa8\x17\xe4"
	"\x56\xb9\xef\x7f\x02\x7d\x96\xa3\x48\x14\x72\x75\x2f\xb0\xb5\x87"
	"\xee\x55\xe9\x6a\x6d\x28\x3c\xc1\xfd\x00\xe4\x76\xe3\x80\x88\x78"
	"\x26\x0d\x6c\x8c\xb8\x64\x61\x63\xb7\x13\x3a\xab\xc7\xdd\x1d\x0a"
	"\xd7\x15\x45\xa1\xd6\xd9\x34\xc7\x21\x48\xfb\x43\x87\x38\xda\x1f"
	"\x50\x47\xb1\xa5\x5c\x47\xed\x04\x44\x97\xd3\xac\x74\x2d\xeb\x09"
	"\x77\x59\xbf\xa3\x54\x5b\xde\x42\xd5\x23\x5a\x71\x9f";

BYTE test_CertificateHash_SHA256[] =
	"\xea\x05\xfe\xfe\xcc\x6b\x0b\xd5\x71\xdb\xbc\x5b\xaa\x3e\xd4\x53"
	"\x86\xd0\x44\x68\x35\xf7\xb7\x4c\x85\x62\x1b\x99\x83\x47\x5f\x95";

BYTE test_ChannelBindingsHash[] =
	"\x65\x86\xE9\x9D\x81\xC2\xFC\x98\x4E\x47\x17\x2F\xD4\xDD\x03\x10";

/*
 * Channel Bindings Data:
 *
 * tls-server-end-point:<binary hash>
 */

char TlsServerEndPointPrefix[] = "tls-server-end-point:";

void ntlm_uint32_to_big_endian(UINT32 num, BYTE be32[4])
{
	be32[0] = (num >> 0) & 0xFF;
	be32[1] = (num >> 8) & 0xFF;
	be32[2] = (num >> 16) & 0xFF;
	be32[3] = (num >> 24) & 0xFF;
}

/*
typedef struct gss_channel_bindings_struct {
	OM_uint32 initiator_addrtype;
	gss_buffer_desc initiator_address;
	OM_uint32 acceptor_addrtype;
	gss_buffer_desc acceptor_address;
	gss_buffer_desc application_data;
} *gss_channel_bindings_t;
 */

void ntlm_compute_channel_bindings(NTLM_CONTEXT* context)
{
#if 0
	MD5_CTX md5;
	BYTE be32[4];
	int PrefixLength;
	SHA256_CTX sha256;
	BYTE* CertificateHash;
	int CertificateHashLength;
	BYTE* pChannelBindingToken;
	int ChannelBindingTokenLength;
	SEC_CHANNEL_BINDINGS* ChannelBindings;

	ZeroMemory(context->ChannelBindingsHash, 16);

	CertificateHashLength = 32;
	CertificateHash = (BYTE*) malloc(CertificateHashLength);
	ZeroMemory(CertificateHash, CertificateHashLength);

	SHA256_Init(&sha256);
	SHA256_Update(&sha256, test_Certificate, sizeof(test_Certificate));
	SHA256_Final(CertificateHash, &sha256);

	printf("Certificate SHA256 Hash:\n");
	winpr_HexDump(CertificateHash, CertificateHashLength);

	PrefixLength = strlen(TlsServerEndPointPrefix);
	ChannelBindingTokenLength = PrefixLength + CertificateHashLength;
	context->ChannelBindingToken = (BYTE*) malloc(ChannelBindingTokenLength + 1);
	strcpy((char*) context->ChannelBindingToken, TlsServerEndPointPrefix);
	CopyMemory(&context->ChannelBindingToken[PrefixLength], CertificateHash, CertificateHashLength);

	printf("ChannelBindingToken:\n");
	winpr_HexDump(context->ChannelBindingToken, ChannelBindingTokenLength);

	context->EndpointBindings.BindingsLength = sizeof(SEC_CHANNEL_BINDINGS) + ChannelBindingTokenLength;
	context->EndpointBindings.Bindings = (SEC_CHANNEL_BINDINGS*) malloc(context->EndpointBindings.BindingsLength);
	ZeroMemory(context->EndpointBindings.Bindings, context->EndpointBindings.BindingsLength);

	ChannelBindings = context->EndpointBindings.Bindings;
	ChannelBindings->cbApplicationDataLength = ChannelBindingTokenLength;
	ChannelBindings->dwApplicationDataOffset = sizeof(SEC_CHANNEL_BINDINGS);

	pChannelBindingToken = &((BYTE*) ChannelBindings)[ChannelBindings->dwApplicationDataOffset];
	CopyMemory(pChannelBindingToken, context->ChannelBindingToken, ChannelBindingTokenLength);

	printf("ChannelBindings\n");
	winpr_HexDump((BYTE*) ChannelBindings, context->EndpointBindings.BindingsLength);

	MD5_Init(&md5);

	ntlm_uint32_to_big_endian(ChannelBindings->dwInitiatorAddrType, be32);
	MD5_Update(&md5, be32, 4);

	ntlm_uint32_to_big_endian(ChannelBindings->cbInitiatorLength, be32);
	MD5_Update(&md5, be32, 4);

	ntlm_uint32_to_big_endian(ChannelBindings->dwAcceptorAddrType, be32);
	MD5_Update(&md5, be32, 4);

	ntlm_uint32_to_big_endian(ChannelBindings->cbAcceptorLength, be32);
	MD5_Update(&md5, be32, 4);

	ntlm_uint32_to_big_endian(ChannelBindings->cbApplicationDataLength, be32);
	MD5_Update(&md5, be32, 4);

	MD5_Update(&md5, (void*) pChannelBindingToken, ChannelBindingTokenLength);

	MD5_Final(context->ChannelBindingsHash, &md5);

	printf("ChannelBindingsHash:\n");
	winpr_HexDump(context->ChannelBindingsHash, 16);

	free(CertificateHash);

	printf("\n\n");
#endif
}

void ntlm_construct_challenge_target_info(NTLM_CONTEXT* context)
{
	int length;
	ULONG AvPairsCount;
	ULONG AvPairsLength;
	LONG AvPairListSize;
	NTLM_AV_PAIR* pAvPairList;
	UNICODE_STRING NbDomainName;
	UNICODE_STRING NbComputerName;
	UNICODE_STRING DnsDomainName;
	UNICODE_STRING DnsComputerName;

	ntlm_get_target_computer_name(&NbDomainName, ComputerNameNetBIOS);
	ntlm_get_target_computer_name(&NbComputerName, ComputerNameNetBIOS);
	ntlm_get_target_computer_name(&DnsDomainName, ComputerNameDnsDomain);
	ntlm_get_target_computer_name(&DnsComputerName, ComputerNameDnsHostname);

	AvPairsCount = 5;
	AvPairsLength = NbDomainName.Length + NbComputerName.Length +
			DnsDomainName.Length + DnsComputerName.Length + 8;

	length = ntlm_av_pair_list_size(AvPairsCount, AvPairsLength);
	sspi_SecBufferAlloc(&context->ChallengeTargetInfo, length);

	pAvPairList = (NTLM_AV_PAIR*) context->ChallengeTargetInfo.pvBuffer;
	AvPairListSize = (ULONG) context->ChallengeTargetInfo.cbBuffer;

	ntlm_av_pair_list_init(pAvPairList);
	ntlm_av_pair_add(pAvPairList, MsvAvNbDomainName, (PBYTE) NbDomainName.Buffer, NbDomainName.Length);
	ntlm_av_pair_add(pAvPairList, MsvAvNbComputerName, (PBYTE) NbComputerName.Buffer, NbComputerName.Length);
	ntlm_av_pair_add(pAvPairList, MsvAvDnsDomainName, (PBYTE) DnsDomainName.Buffer, DnsDomainName.Length);
	ntlm_av_pair_add(pAvPairList, MsvAvDnsComputerName, (PBYTE) DnsComputerName.Buffer, DnsComputerName.Length);
	ntlm_av_pair_add(pAvPairList, MsvAvTimestamp, context->Timestamp, sizeof(context->Timestamp));

	ntlm_free_unicode_string(&NbDomainName);
	ntlm_free_unicode_string(&NbComputerName);
	ntlm_free_unicode_string(&DnsDomainName);
	ntlm_free_unicode_string(&DnsComputerName);
}

void ntlm_construct_authenticate_target_info(NTLM_CONTEXT* context)
{
	ULONG size;
	ULONG AvPairsCount;
	ULONG AvPairsValueLength;
	NTLM_AV_PAIR* AvTimestamp;
	NTLM_AV_PAIR* AvNbDomainName;
	NTLM_AV_PAIR* AvNbComputerName;
	NTLM_AV_PAIR* AvDnsDomainName;
	NTLM_AV_PAIR* AvDnsComputerName;
	NTLM_AV_PAIR* AvDnsTreeName;
	NTLM_AV_PAIR* ChallengeTargetInfo;
	NTLM_AV_PAIR* AuthenticateTargetInfo;

	AvPairsCount = 1;
	AvPairsValueLength = 0;
	ChallengeTargetInfo = (NTLM_AV_PAIR*) context->ChallengeTargetInfo.pvBuffer;

	AvNbDomainName = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvNbDomainName);
	AvNbComputerName = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvNbComputerName);
	AvDnsDomainName = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvDnsDomainName);
	AvDnsComputerName = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvDnsComputerName);
	AvDnsTreeName = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvDnsTreeName);
	AvTimestamp = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvTimestamp);

	if (AvNbDomainName != NULL)
	{
		AvPairsCount++; /* MsvAvNbDomainName */
		AvPairsValueLength += AvNbDomainName->AvLen;
	}

	if (AvNbComputerName != NULL)
	{
		AvPairsCount++; /* MsvAvNbComputerName */
		AvPairsValueLength += AvNbComputerName->AvLen;
	}

	if (AvDnsDomainName != NULL)
	{
		AvPairsCount++; /* MsvAvDnsDomainName */
		AvPairsValueLength += AvDnsDomainName->AvLen;
	}

	if (AvDnsComputerName != NULL)
	{
		AvPairsCount++; /* MsvAvDnsComputerName */
		AvPairsValueLength += AvDnsComputerName->AvLen;
	}

	if (AvDnsTreeName != NULL)
	{
		AvPairsCount++; /* MsvAvDnsTreeName */
		AvPairsValueLength += AvDnsTreeName->AvLen;
	}

	AvPairsCount++; /* MsvAvTimestamp */
	AvPairsValueLength += 8;

	if (context->UseMIC)
	{
		AvPairsCount++; /* MsvAvFlags */
		AvPairsValueLength += 4;
	}

	//AvPairsCount++; /* MsvAvRestrictions */
	//AvPairsValueLength += 48;

	/**
	 * Extended Protection for Authentication:
	 * http://blogs.technet.com/b/srd/archive/2009/12/08/extended-protection-for-authentication.aspx
	 */

	if (!context->SuppressExtendedProtection)
	{
		/**
		 * SEC_CHANNEL_BINDINGS structure
		 * http://msdn.microsoft.com/en-us/library/windows/desktop/dd919963/
		 */

		AvPairsCount++; /* MsvChannelBindings */
		AvPairsValueLength += 16;
		ntlm_compute_channel_bindings(context);

		if (context->ServicePrincipalName.Length > 0)
		{
			AvPairsCount++; /* MsvAvTargetName */
			AvPairsValueLength += context->ServicePrincipalName.Length;
		}
	}

	size = ntlm_av_pair_list_size(AvPairsCount, AvPairsValueLength);

	if (context->NTLMv2)
		size += 8; /* unknown 8-byte padding */

	sspi_SecBufferAlloc(&context->AuthenticateTargetInfo, size);
	AuthenticateTargetInfo = (NTLM_AV_PAIR*) context->AuthenticateTargetInfo.pvBuffer;

	ntlm_av_pair_list_init(AuthenticateTargetInfo);

	if (AvNbDomainName != NULL)
		ntlm_av_pair_add_copy(AuthenticateTargetInfo, AvNbDomainName);

	if (AvNbComputerName != NULL)
		ntlm_av_pair_add_copy(AuthenticateTargetInfo, AvNbComputerName);

	if (AvDnsDomainName != NULL)
		ntlm_av_pair_add_copy(AuthenticateTargetInfo, AvDnsDomainName);

	if (AvDnsComputerName != NULL)
		ntlm_av_pair_add_copy(AuthenticateTargetInfo, AvDnsComputerName);

	if (AvDnsTreeName != NULL)
		ntlm_av_pair_add_copy(AuthenticateTargetInfo, AvDnsTreeName);

	if (AvTimestamp != NULL)
		ntlm_av_pair_add_copy(AuthenticateTargetInfo, AvTimestamp);

	if (context->UseMIC)
	{
		UINT32 flags = MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK;
		ntlm_av_pair_add(AuthenticateTargetInfo, MsvAvFlags, (PBYTE) &flags, 4);
	}

	if (!context->SuppressExtendedProtection)
	{
		ntlm_av_pair_add(AuthenticateTargetInfo, MsvChannelBindings, context->ChannelBindingsHash, 16);

		if (context->ServicePrincipalName.Length > 0)
		{
			ntlm_av_pair_add(AuthenticateTargetInfo, MsvAvTargetName,
					(PBYTE) context->ServicePrincipalName.Buffer,
					context->ServicePrincipalName.Length);
		}
	}

	if (context->NTLMv2)
	{
		NTLM_AV_PAIR* AvEOL;

		AvEOL = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvEOL);
		ZeroMemory((void*) AvEOL, 4);
	}
}
