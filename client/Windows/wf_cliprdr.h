/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Clipboard Redirection
 *
 * Copyright 2012 Jason Champion
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
#ifndef __WF_CLIPRDR_H
#define __WF_CLIPRDR_H

#define CINTERFACE
#define COBJMACROS

#include <Ole2.h>
#include <ShlObj.h>

typedef struct wf_clipboard wfClipboard;

#include "wf_client.h"

#include <freerdp/log.h>

#ifdef WITH_DEBUG_CLIPRDR
#define DEBUG_CLIPRDR(fmt, ...) WLog_DBG(TAG, fmt, ## __VA_ARGS__)
#else
#define DEBUG_CLIPRDR(fmt, ...) do { } while (0)
#endif

struct _CliprdrStream
{
	IStream iStream;

	LONG m_lRefCount;
	LONG m_lIndex;
	ULARGE_INTEGER m_lSize;
	ULARGE_INTEGER m_lOffset;
	void* m_pData;
};
typedef struct _CliprdrStream CliprdrStream;

CliprdrStream* CliprdrStream_New(LONG index, void* pData);
void CliprdrStream_Delete(CliprdrStream* instance);

struct _CliprdrDataObject
{
	IDataObject iDataObject;

	LONG m_lRefCount;
	FORMATETC* m_pFormatEtc;
	STGMEDIUM* m_pStgMedium;
	LONG m_nNumFormats;
	LONG m_nStreams;
	IStream** m_pStream;
	void* m_pData;
};
typedef struct _CliprdrDataObject CliprdrDataObject;

CliprdrDataObject* CliprdrDataObject_New(FORMATETC* fmtetc, STGMEDIUM* stgmed, int count, void* data);
void CliprdrDataObject_Delete(CliprdrDataObject* instance);

struct _CliprdrEnumFORMATETC
{
	IEnumFORMATETC iEnumFORMATETC;

	LONG m_lRefCount;
	LONG m_nIndex;
	LONG m_nNumFormats;
	FORMATETC* m_pFormatEtc;
};
typedef struct _CliprdrEnumFORMATETC CliprdrEnumFORMATETC;

CliprdrEnumFORMATETC* CliprdrEnumFORMATETC_New(int nFormats, FORMATETC* pFormatEtc);
void CliprdrEnumFORMATETC_Delete(CliprdrEnumFORMATETC* This);

struct format_mapping
{
	UINT32 remote_format_id;
	UINT32 local_format_id;
	void* name; /* Unicode or ASCII characters with NULL terminator */
};
typedef struct format_mapping formatMapping;

struct wf_clipboard
{
	wfContext* wfc;
	rdpChannels* channels;
	CliprdrClientContext* context;

	BOOL sync;
	UINT32 capabilities;

	int map_size;
	int map_capacity;
	formatMapping* format_mappings;

	UINT32 requestedFormatId;

	HWND hwnd;
	HANDLE hmem;
	HANDLE thread;
	HANDLE response_data_event;

	/* file clipping */
	CLIPFORMAT ID_FILEDESCRIPTORW;
	CLIPFORMAT ID_FILECONTENTS;
	CLIPFORMAT ID_PREFERREDDROPEFFECT;

	LPDATAOBJECT data_obj;
	ULONG req_fsize;
	char* req_fdata;
	HANDLE req_fevent;

	int nFiles;
	int file_array_size;
	WCHAR** file_names;
	FILEDESCRIPTORW** fileDescriptor;
};

void wf_cliprdr_init(wfContext* wfc, CliprdrClientContext* cliprdr);
void wf_cliprdr_uninit(wfContext* wfc, CliprdrClientContext* cliprdr);

int cliprdr_send_data_request(wfClipboard* clipboard, UINT32 format);
int cliprdr_send_lock(wfClipboard* clipboard);
int cliprdr_send_unlock(wfClipboard* clipboard);
int cliprdr_send_request_filecontents(wfClipboard* clipboard, void* streamid,
		int index, int flag, DWORD positionhigh, DWORD positionlow, ULONG request);

#endif /* __WF_CLIPRDR_H */
