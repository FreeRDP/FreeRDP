/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_WLOG_PACKET_MESSAGE_PRIVATE_H
#define WINPR_WLOG_PACKET_MESSAGE_PRIVATE_H

#include "wlog.h"

#define PCAP_MAGIC_NUMBER	0xA1B2C3D4

struct _wPcapHeader
{
	UINT32 magic_number; /* magic number */
	UINT16 version_major; /* major version number */
	UINT16 version_minor; /* minor version number */
	INT32 thiszone; /* GMT to local correction */
	UINT32 sigfigs; /* accuracy of timestamps */
	UINT32 snaplen; /* max length of captured packets, in octets */
	UINT32 network; /* data link type */
};
typedef struct _wPcapHeader wPcapHeader;

struct _wPcapRecordHeader
{
	UINT32 ts_sec; /* timestamp seconds */
	UINT32 ts_usec; /* timestamp microseconds */
	UINT32 incl_len; /* number of octets of packet saved in file */
	UINT32 orig_len; /* actual length of packet */
};
typedef struct _wPcapRecordHeader wPcapRecordHeader;

typedef struct _wPcapRecord wPcapRecord;

struct _wPcapRecord
{
	wPcapRecordHeader header;
	void* data;
	UINT32 length;
	wPcapRecord* next;
};

struct _wPcap
{
	FILE* fp;
	char* name;
	BOOL write;
	int file_size;
	int record_count;
	wPcapHeader header;
	wPcapRecord* head;
	wPcapRecord* tail;
	wPcapRecord* record;
};
typedef struct _wPcap wPcap;

wPcap* Pcap_Open(char* name, BOOL write);
void Pcap_Close(wPcap* pcap);

void Pcap_Flush(wPcap* pcap);

struct _wEthernetHeader
{
	BYTE Destination[6];
	BYTE Source[6];
	UINT16 Type;
};
typedef struct _wEthernetHeader wEthernetHeader;

struct _wIPv4Header
{
	BYTE Version;
	BYTE InternetHeaderLength;
	BYTE TypeOfService;
	UINT16 TotalLength;
	UINT16 Identification;
	BYTE InternetProtocolFlags;
	UINT16 FragmentOffset;
	BYTE TimeToLive;
	BYTE Protocol;
	UINT16 HeaderChecksum;
	UINT32 SourceAddress;
	UINT32 DestinationAddress;
};
typedef struct _wIPv4Header wIPv4Header;

struct _wTcpHeader
{
	UINT16 SourcePort;
	UINT16 DestinationPort;
	UINT32 SequenceNumber;
	UINT32 AcknowledgementNumber;
	BYTE Offset;
	BYTE Reserved;
	BYTE TcpFlags;
	UINT16 Window;
	UINT16 Checksum;
	UINT16 UrgentPointer;
};
typedef struct _wTcpHeader wTcpHeader;

BOOL WLog_PacketMessage_Write(wPcap* pcap, void* data, DWORD length, DWORD flags);

#endif /* WINPR_WLOG_PACKET_MESSAGE_PRIVATE_H */

