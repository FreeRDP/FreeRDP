/**
 * FreeRDP: A Remote Desktop Protocol Client
 * pcap File Format Utils
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

#ifndef __UTILS_PCAP_H
#define __UTILS_PCAP_H

#include <freerdp/api.h>
#include <freerdp/types.h>

struct _pcap_header
{
	uint32 magic_number;   /* magic number */
	uint16 version_major;  /* major version number */
	uint16 version_minor;  /* minor version number */
	sint32 thiszone;       /* GMT to local correction */
	uint32 sigfigs;        /* accuracy of timestamps */
	uint32 snaplen;        /* max length of captured packets, in octets */
	uint32 network;        /* data link type */
};
typedef struct _pcap_header pcap_header;

struct _pcap_record_header
{
	uint32 ts_sec;         /* timestamp seconds */
	uint32 ts_usec;        /* timestamp microseconds */
	uint32 incl_len;       /* number of octets of packet saved in file */
	uint32 orig_len;       /* actual length of packet */
};
typedef struct _pcap_record_header pcap_record_header;

typedef struct _pcap_record pcap_record;

struct _pcap_record
{
	pcap_record_header header;
	void* data;
	uint32 length;
	pcap_record* next;
};

struct rdp_pcap
{
	FILE* fp;
	char* name;
	boolean write;
	int file_size;
	int record_count;
	pcap_header header;
	pcap_record* head;
	pcap_record* tail;
	pcap_record* record;
};
typedef struct rdp_pcap rdpPcap;

FREERDP_API rdpPcap* pcap_open(char* name, boolean write);
FREERDP_API void pcap_close(rdpPcap* pcap);

FREERDP_API void pcap_add_record(rdpPcap* pcap, void* data, uint32 length);
FREERDP_API boolean pcap_has_next_record(rdpPcap* pcap);
FREERDP_API boolean pcap_get_next_record(rdpPcap* pcap, pcap_record* record);
FREERDP_API boolean pcap_get_next_record_header(rdpPcap* pcap, pcap_record* record);
FREERDP_API boolean pcap_get_next_record_content(rdpPcap* pcap, pcap_record* record);
FREERDP_API void pcap_flush(rdpPcap* pcap);

#endif /* __UTILS_PCAP_H */
