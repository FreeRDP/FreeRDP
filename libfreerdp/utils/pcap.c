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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/assert.h>
#include <winpr/file.h>
#include <winpr/crt.h>
#include <winpr/sysinfo.h>

#include <freerdp/types.h>
#include <freerdp/utils/pcap.h>

#define PCAP_MAGIC 0xA1B2C3D4

struct rdp_pcap
{
	FILE* fp;
	char* name;
	BOOL write;
	INT64 file_size;
	size_t record_count;
	pcap_header header;
	pcap_record* head;
	pcap_record* tail;
};

WINPR_ATTR_NODISCARD
static BOOL pcap_read_header(rdpPcap* pcap, pcap_header* header)
{
	WINPR_ASSERT(pcap);
	WINPR_ASSERT(header);

	return fread(header, sizeof(pcap_header), 1, pcap->fp) == 1;
}

WINPR_ATTR_NODISCARD
static BOOL pcap_write_header(rdpPcap* pcap, const pcap_header* header)
{
	WINPR_ASSERT(pcap);
	WINPR_ASSERT(header);

	return fwrite(header, sizeof(pcap_header), 1, pcap->fp) == 1;
}

WINPR_ATTR_NODISCARD
static BOOL pcap_read_record_header(rdpPcap* pcap, pcap_record_header* record)
{
	WINPR_ASSERT(pcap);
	WINPR_ASSERT(record);

	return fread(record, sizeof(pcap_record_header), 1, pcap->fp) == 1;
}

WINPR_ATTR_NODISCARD
static BOOL pcap_write_record_header(rdpPcap* pcap, const pcap_record_header* record)
{
	WINPR_ASSERT(pcap);
	WINPR_ASSERT(record);

	return fwrite(record, sizeof(pcap_record_header), 1, pcap->fp) == 1;
}

WINPR_ATTR_NODISCARD
static BOOL pcap_read_record(rdpPcap* pcap, pcap_record* record)
{
	WINPR_ASSERT(pcap);
	WINPR_ASSERT(record);

	if (!pcap_read_record_header(pcap, &record->header))
		return FALSE;

	record->length = record->header.incl_len;
	record->data = malloc(record->length);
	if (!record->data)
		return FALSE;

	if (fread(record->data, record->length, 1, pcap->fp) != 1)
	{
		free(record->data);
		record->data = nullptr;
		return FALSE;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL pcap_write_record(rdpPcap* pcap, const pcap_record* record)
{
	WINPR_ASSERT(pcap);
	WINPR_ASSERT(record);

	return pcap_write_record_header(pcap, &record->header) &&
	       (fwrite(record->cdata, record->length, 1, pcap->fp) == 1);
}

WINPR_ATTR_NODISCARD
static BOOL pcap_add_record_int(rdpPcap* pcap, const void* data, size_t length)
{
	WINPR_ASSERT(pcap);
	WINPR_ASSERT(data || (length == 0));
	WINPR_ASSERT(length <= UINT32_MAX);

	pcap_record* record = (pcap_record*)calloc(1, sizeof(pcap_record));
	if (!record)
		return FALSE;

	record->cdata = data;
	record->length = (UINT32)length;
	record->header.incl_len = (UINT32)length;
	record->header.orig_len = (UINT32)length;

	const UINT64 ns = winpr_GetUnixTimeNS();

	record->header.ts_sec = (UINT32)WINPR_TIME_NS_TO_S(ns);
	record->header.ts_usec = (UINT32)WINPR_TIME_NS_REM_US(ns);

	if (pcap->head == nullptr)
		pcap->head = record;

	if (pcap->tail == nullptr)
		pcap->tail = record;
	else
	{
		record->next = pcap->tail;
		pcap->tail = record;
	}

	return TRUE;
}

BOOL pcap_add_record(rdpPcap* pcap, const void* data, size_t length)
{
	size_t offset = 0;
	const BYTE* bpdata = data;
	while ((length > UINT32_MAX) && (offset < length - UINT32_MAX))
	{
		if (!pcap_add_record_int(pcap, &bpdata[offset], UINT32_MAX))
			return FALSE;
		offset += UINT32_MAX;
	}

	return pcap_add_record_int(pcap, &bpdata[offset], length - offset);
}

BOOL pcap_has_next_record(const rdpPcap* pcap)
{
	WINPR_ASSERT(pcap);

	return (pcap->file_size - (_ftelli64(pcap->fp)) > 16);
}

BOOL pcap_get_next_record_header(rdpPcap* pcap, pcap_record* record)
{
	WINPR_ASSERT(pcap);
	WINPR_ASSERT(record);

	if (pcap_has_next_record(pcap) != TRUE)
		return FALSE;

	if (!pcap_read_record_header(pcap, &record->header))
		return FALSE;

	record->length = record->header.incl_len;

	return TRUE;
}

BOOL pcap_get_next_record_content(rdpPcap* pcap, pcap_record* record)
{
	WINPR_ASSERT(pcap);
	WINPR_ASSERT(record);

	return fread(record->data, record->length, 1, pcap->fp) == 1;
}

BOOL pcap_get_next_record(rdpPcap* pcap, pcap_record* record)
{
	WINPR_ASSERT(pcap);
	WINPR_ASSERT(record);

	return pcap_has_next_record(pcap) && pcap_read_record(pcap, record);
}

rdpPcap* pcap_open(const char* name, BOOL write)
{
	WINPR_ASSERT(name);

	rdpPcap* pcap = (rdpPcap*)calloc(1, sizeof(rdpPcap));
	if (!pcap)
		goto fail;

	pcap->name = _strdup(name);
	if (!pcap->name)
		goto fail;

	pcap->write = write;
	pcap->record_count = 0;
	pcap->fp = winpr_fopen(name, write ? "w+b" : "rb");

	if (pcap->fp == nullptr)
		goto fail;

	if (write)
	{
		pcap->header.magic_number = PCAP_MAGIC;
		pcap->header.version_major = 2;
		pcap->header.version_minor = 4;
		pcap->header.thiszone = 0;
		pcap->header.sigfigs = 0;
		pcap->header.snaplen = UINT32_MAX;
		pcap->header.network = 0;
		if (!pcap_write_header(pcap, &pcap->header))
			goto fail;
	}
	else
	{
		if (_fseeki64(pcap->fp, 0, SEEK_END) != 0)
			goto fail;
		pcap->file_size = _ftelli64(pcap->fp);
		if (_fseeki64(pcap->fp, 0, SEEK_SET) != 0)
			goto fail;
		if (!pcap_read_header(pcap, &pcap->header))
			goto fail;
	}

	return pcap;

fail:
	pcap_close(pcap);
	return nullptr;
}

void pcap_flush(rdpPcap* pcap)
{
	WINPR_ASSERT(pcap);

	while (pcap->head != nullptr)
	{
		(void)pcap_write_record(pcap, pcap->head);
		pcap_record* next = pcap->head->next;
		free(pcap->head);
		pcap->head = next;
	}

	if (pcap->fp != nullptr)
		(void)fflush(pcap->fp);

	pcap->head = nullptr;
	pcap->tail = nullptr;
}

void pcap_close(rdpPcap* pcap)
{
	if (!pcap)
		return;

	pcap_flush(pcap);

	if (pcap->fp != nullptr)
		(void)fclose(pcap->fp);

	free(pcap->name);
	free(pcap);
}
