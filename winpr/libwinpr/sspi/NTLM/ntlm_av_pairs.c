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

/* Certificate (TBSCertificate) */

BYTE test_Certificate[] =
	"\x30\x82\x02\xfa\xa0\x03\x02\x01\x02\x02\x10\x33\x1d\x91\x3b\x2b"
	"\x0a\xc8\x90\x4a\x19\x35\xed\x79\x65\xb8\xe3\x30\x09\x06\x05\x2b"
	"\x0e\x03\x02\x1d\x05\x00\x30\x16\x31\x14\x30\x12\x06\x03\x55\x04"
	"\x03\x13\x0b\x4c\x41\x42\x33\x2d\x57\x32\x4b\x38\x52\x32\x30\x1e"
	"\x17\x0d\x31\x32\x31\x32\x31\x37\x31\x37\x33\x33\x31\x32\x5a\x17"
	"\x0d\x33\x39\x31\x32\x33\x31\x32\x33\x35\x39\x35\x39\x5a\x30\x16"
	"\x31\x14\x30\x12\x06\x03\x55\x04\x03\x13\x0b\x4c\x41\x42\x33\x2d"
	"\x57\x32\x4b\x38\x52\x32\x30\x82\x02\x22\x30\x0d\x06\x09\x2a\x86"
	"\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x82\x02\x0f\x00\x30\x82"
	"\x02\x0a\x02\x82\x02\x01\x00\xbf\x99\x68\x59\x41\x06\x5a\xd0\x3a"
	"\x38\x7a\xca\x7a\x1e\x85\xee\x21\x7e\xa3\x01\xca\xf5\xdd\x2f\xbc"
	"\xf7\x05\x87\x12\x89\x22\xd7\x37\xc1\xe1\x5d\x41\x02\xd8\x60\xc5"
	"\x07\xec\x26\x6f\x13\x48\xe1\xf3\xc3\x42\x1e\x71\x5d\x31\x8e\x83"
	"\x74\xac\x23\x84\x88\x02\xf4\xa2\x0a\xa8\x41\xfd\x16\x27\x31\x9c"
	"\xb6\x78\x08\x84\xfe\x5f\xa1\x9f\x9b\x5c\xe4\x41\xf0\x7f\xb2\x41"
	"\xe8\xd7\xf1\xcc\xb5\xd4\x38\x38\x5c\x0b\x64\x4b\x10\x6a\x62\x55"
	"\xc1\x4f\xa3\x22\x58\x01\x92\x1c\x09\x68\xec\xe4\xf1\x01\xda\xe3"
	"\x6d\x4b\xec\xc2\x9e\xc4\x8e\xc0\x00\x0f\x6e\x07\x7b\xec\x87\x0e"
	"\x17\x46\xc7\x84\x78\x78\xd0\x9b\xeb\x86\xca\xd4\x30\x09\xe4\xd5"
	"\xb7\x54\xab\xc0\x86\x2c\x7f\x0e\x8f\x00\xda\xd9\xdf\xb3\x56\xbe"
	"\xce\xd5\x65\x1b\xe0\xe2\x55\x5a\xd1\xef\xfc\xe6\x20\x3f\x22\xc0"
	"\xa6\x88\x63\x00\xa7\x0a\xcb\x73\xf1\x0d\x73\xba\x79\x39\x45\x8d"
	"\xa8\x1f\x32\x58\xb1\xb5\x23\xe7\x68\x7d\xc4\x3a\x8f\x44\xa8\x36"
	"\xc1\x5d\x0d\xcf\x4a\x21\x0f\x33\x8c\xb4\x57\x9a\x9f\xcb\xc6\xcb"
	"\x04\x25\x74\x3a\x91\xaf\xd6\xb5\x35\x94\xa5\xe3\xe1\xa4\x92\x87"
	"\xb5\xc7\xd9\x23\x07\xe2\x37\xf3\xb9\x6d\x01\x55\xa2\x39\x65\x8e"
	"\xee\xcc\x53\x29\xcb\x1f\x1c\x7e\x7d\x4e\xd6\xfc\xe7\x02\x0a\xba"
	"\xdb\xa9\x2d\x74\x9a\xeb\xe0\x95\x50\x23\xcd\xff\xe7\xc4\x47\xc2"
	"\x55\x08\xdb\x1b\x0f\xb8\xd4\x4d\x4f\xd7\xd7\x13\x4c\x66\xed\x0f"
	"\x78\x7d\x9a\xd5\xb5\xc0\x9e\xf3\xff\xe6\xef\xb7\xea\xd1\xff\xd4"
	"\xf7\x09\x92\x15\x3e\x44\xb4\x3e\x01\x29\x06\xf1\x73\xa3\x61\xe8"
	"\x84\x7b\x3e\x0d\x12\x44\x73\x4a\x29\x4b\x82\xe3\xac\x64\x81\xa5"
	"\xa5\x1d\xa6\xb5\xd6\xc2\xe5\x6f\x98\x3b\x7c\x70\x82\xda\xf9\xf1"
	"\xde\x46\x7b\xe8\x9e\x03\xbf\xc7\xb2\xe6\x26\x24\x21\x6c\xb3\x67"
	"\xde\xe0\x8c\x17\x93\xf2\x32\xe5\xae\x5e\xfd\x4f\x77\x40\x3e\x25"
	"\xfe\xfe\xc0\xd5\xc9\xcd\xff\x8b\xa2\x4e\xae\x89\x12\x0f\x05\xdb"
	"\xc5\xba\xc2\x02\x6f\x60\xf8\x91\x23\x99\x29\xb4\x22\x5e\x62\x43"
	"\x5e\xde\xb3\x56\x02\x3c\xa2\xf8\x2a\xbf\x82\xf7\x7a\xcc\x2c\x26"
	"\x96\x88\x0e\x8d\x65\xb6\x7f\xda\x3c\x52\x6a\x25\xc1\xb2\x89\x5e"
	"\x3d\x30\x39\x5f\x11\x74\x3c\x00\xa2\xef\x2c\xff\x3e\xaa\x04\x34"
	"\x93\xbd\x55\xc2\x8b\x98\x5b\x66\x9c\x53\x8b\x27\x7c\x4c\x87\x0b"
	"\x5b\x24\x72\x08\x06\xf9\x11\x02\x03\x01\x00\x01\xa3\x60\x30\x5e"
	"\x30\x13\x06\x03\x55\x1d\x25\x04\x0c\x30\x0a\x06\x08\x2b\x06\x01"
	"\x05\x05\x07\x03\x01\x30\x47\x06\x03\x55\x1d\x01\x04\x40\x30\x3e"
	"\x80\x10\x8c\x90\xe6\x16\x1e\x1e\xa9\xf3\xbf\x03\x93\x22\xe1\x0c"
	"\x3b\xfe\xa1\x18\x30\x16\x31\x14\x30\x12\x06\x03\x55\x04\x03\x13"
	"\x0b\x4c\x41\x42\x33\x2d\x57\x32\x4b\x38\x52\x32\x82\x10\x33\x1d"
	"\x91\x3b\x2b\x0a\xc8\x90\x4a\x19\x35\xed\x79\x65\xb8\xe3";

/* 544D5750776A7533334736516B4D76694762777548516B41 (MD5 hash of SEC_CHANNEL_BINDINGS) */
BYTE test_ChannelBindingsHash[] = "\x54\x4D\x57\x50\x77\x6A\x75\x33\x33\x47\x36\x51\x6B\x4D\x76\x69\x47\x62\x77\x75\x48\x51\x6B\x41";

/* 63d3533695bf1bd0de203fee9691a2515e2e7bfe75bb8ab09da08911e157847c (SHA-256 hash of Certificate, length = 32) */
BYTE test_CertificateHash_SHA256[] = "\x63\xd3\x53\x36\x95\xbf\x1b\xd0\xde\x20\x3f\xee\x96\x91\xa2\x51\x5e\x2e\x7b\xfe\x75\xbb\x8a\xb0\x9d\xa0\x89\x11\xe1\x57\x84\x7c";

/* c0ef35911b5ba641c4b67f4b32c7efc4c1d82e2b (SHA1 hash of Certificate, length = 20) */
BYTE test_CertificateHash_SHA1[] = "\xc0\xef\x35\x91\x1b\x5b\xa6\x41\xc4\xb6\x7f\x4b\x32\xc7\xef\xc4\xc1\xd8\x2e\x2b";

/* d2b4df88a54faf95590cd336987a97a5 (MD5 hash of Certificate, length = 16) */
BYTE test_CertificateHash_MD5[] = "\xd2\xb4\xdf\x88\xa5\x4f\xaf\x95\x59\x0c\xd3\x36\x98\x7a\x97\xa5";

BYTE* test_CertificateHash = test_CertificateHash_SHA256;
int test_CertificateHashLength = 32;

/*
 * Channel Bindings Data:
 *
 * tls-server-end-point:<binary hash>
 */

char TlsServerEndPointPrefix[] = "tls-server-end-point:";

void ntlm_compute_channel_bindings(NTLM_CONTEXT* context)
{
#if 0
	MD5_CTX md5;
	int HashLength;
	int PrefixLength;
	BYTE* pChannelBindingToken;
	int ChannelBindingTokenLength;
	SEC_CHANNEL_BINDINGS* ChannelBindings;

	ZeroMemory(context->ChannelBindingsHash, 16);

	HashLength = test_CertificateHashLength;
	PrefixLength = strlen(TlsServerEndPointPrefix);
	ChannelBindingTokenLength = PrefixLength + HashLength;
	context->ChannelBindingToken = (BYTE*) malloc(ChannelBindingTokenLength + 1);
	strcpy((char*) context->ChannelBindingToken, TlsServerEndPointPrefix);
	CopyMemory(&context->ChannelBindingToken[PrefixLength], test_CertificateHash, HashLength);

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
	MD5_Update(&md5, (void*) context->EndpointBindings.Bindings, context->EndpointBindings.BindingsLength);
	MD5_Final(context->ChannelBindingsHash, &md5);

	printf("ChannelBindingsHash:\n");
	winpr_HexDump(context->ChannelBindingsHash, 16);
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
