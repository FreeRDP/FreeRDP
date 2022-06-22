/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_UTILS_PCAP_H
#define FREERDP_UTILS_PCAP_H

#include <freerdp/api.h>
#include <freerdp/types.h>

typedef struct
{
	UINT32 magic_number;  /* magic number */
	UINT16 version_major; /* major version number */
	UINT16 version_minor; /* minor version number */
	INT32 thiszone;       /* GMT to local correction */
	UINT32 sigfigs;       /* accuracy of timestamps */
	UINT32 snaplen;       /* max length of captured packets, in octets */
	UINT32 network;       /* data link type */
} pcap_header;

typedef struct
{
	UINT32 ts_sec;   /* timestamp seconds */
	UINT32 ts_usec;  /* timestamp microseconds */
	UINT32 incl_len; /* number of octets of packet saved in file */
	UINT32 orig_len; /* actual length of packet */
} pcap_record_header;

typedef struct s_pcap_record pcap_record;

struct s_pcap_record
{
	pcap_record_header header;
	union
	{
		void* data;
		const void* cdata;
	};
	UINT32 length;
	pcap_record* next;
};

typedef struct rdp_pcap rdpPcap;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API rdpPcap* pcap_open(const char* name, BOOL write);
	FREERDP_API void pcap_close(rdpPcap* pcap);

	FREERDP_API BOOL pcap_add_record(rdpPcap* pcap, const void* data, size_t length);
	FREERDP_API BOOL pcap_has_next_record(const rdpPcap* pcap);
	FREERDP_API BOOL pcap_get_next_record(rdpPcap* pcap, pcap_record* record);
	FREERDP_API BOOL pcap_get_next_record_header(rdpPcap* pcap, pcap_record* record);
	FREERDP_API BOOL pcap_get_next_record_content(rdpPcap* pcap, pcap_record* record);
	FREERDP_API void pcap_flush(rdpPcap* pcap);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_PCAP_H */
