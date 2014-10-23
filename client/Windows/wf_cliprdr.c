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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/utils/event.h>
#include <freerdp/client/cliprdr.h>

#include <Strsafe.h>

#include "wf_cliprdr.h"

#define TAG CLIENT_TAG("windows")

extern BOOL WINAPI AddClipboardFormatListener(_In_ HWND hwnd);
extern BOOL WINAPI RemoveClipboardFormatListener(_In_  HWND hwnd);

#define WM_CLIPRDR_MESSAGE  (WM_USER + 156)
#define OLE_SETCLIPBOARD    1

/* this macro will update _p pointer */
#define Read_UINT32(_p, _v) do { _v = \
	(UINT32)(*_p) + \
	(((UINT32)(*(_p + 1))) << 8) + \
	(((UINT32)(*(_p + 2))) << 16) + \
	(((UINT32)(*(_p + 3))) << 24); \
	_p += 4; } while (0)

/* this macro will NOT update _p pointer */
#define Write_UINT32(_p, _v) do { \
	*(_p) = (_v) & 0xFF; \
	*(_p + 1) = ((_v) >> 8) & 0xFF; \
	*(_p + 2) = ((_v) >> 16) & 0xFF; \
	*(_p + 3) = ((_v) >> 24) & 0xFF; } while (0)

BOOL wf_create_file_obj(cliprdrContext* cliprdr, IDataObject** ppDataObject);
void wf_destroy_file_obj(IDataObject* instance);

/**
 * IStream
 */

HRESULT STDMETHODCALLTYPE CliprdrStream_QueryInterface(IStream* This, REFIID riid, void** ppvObject)
{
	CliprdrStream* instance = (CliprdrStream*) This;

	if (IsEqualIID(riid, &IID_IStream) || IsEqualIID(riid, &IID_IUnknown))
	{
		IStream_AddRef(This);
		*ppvObject = This;
		return S_OK;
	}
	else
	{
		*ppvObject = 0;
		return E_NOINTERFACE;
	}
}

ULONG STDMETHODCALLTYPE CliprdrStream_AddRef(IStream* This)
{
	CliprdrStream* instance = (CliprdrStream*) This;

	return InterlockedIncrement(&instance->m_lRefCount);
}

ULONG STDMETHODCALLTYPE CliprdrStream_Release(IStream* This)
{
	LONG count;
	CliprdrStream* instance = (CliprdrStream*) This;

	count = InterlockedDecrement(&instance->m_lRefCount);

	if (count == 0)
	{
		CliprdrStream_Delete(instance);
		return 0;
	}
	else
	{
		return count;
	}
}

#define FILECONTENTS_SIZE     0x00000001
#define FILECONTENTS_RANGE    0x00000002

HRESULT STDMETHODCALLTYPE CliprdrStream_Read(IStream* This, void *pv, ULONG cb, ULONG *pcbRead)
{
	int ret;
	CliprdrStream* instance = (CliprdrStream*) This;
	cliprdrContext* cliprdr = (cliprdrContext*) instance->m_pData;

	if (pv == NULL || pcbRead == NULL)
		return E_INVALIDARG;

	*pcbRead = 0;

	if (instance->m_lOffset.QuadPart >= instance->m_lSize.QuadPart)
		return S_FALSE;

	ret = cliprdr_send_request_filecontents(cliprdr, (void*) This,
			instance->m_lIndex, FILECONTENTS_RANGE,
			instance->m_lOffset.HighPart, instance->m_lOffset.LowPart, cb);

	if (ret < 0)
		return S_FALSE;

	if (cliprdr->req_fdata)
	{
		memcpy(pv, cliprdr->req_fdata, cliprdr->req_fsize);
		free(cliprdr->req_fdata);
	}

	*pcbRead = cliprdr->req_fsize;
	instance->m_lOffset.QuadPart += cliprdr->req_fsize;

	if (cliprdr->req_fsize < cb)
		return S_FALSE;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Write(IStream* This, const void *pv, ULONG cb, ULONG *pcbWritten)
{
	CliprdrStream* instance = (CliprdrStream*) This;

	return STG_E_ACCESSDENIED;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Seek(IStream* This, LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
{
	ULONGLONG newoffset;
	CliprdrStream* instance = (CliprdrStream*) This;

	newoffset = instance->m_lOffset.QuadPart;

	switch (dwOrigin)
	{
		case STREAM_SEEK_SET:
			newoffset = dlibMove.QuadPart;
			break;
		case STREAM_SEEK_CUR:
			newoffset += dlibMove.QuadPart;
			break;
		case STREAM_SEEK_END:
			newoffset = instance->m_lSize.QuadPart + dlibMove.QuadPart;
			break;
		default:
			return S_FALSE;
	}

	if (newoffset < 0 || newoffset >= instance->m_lSize.QuadPart)
		return FALSE;

	instance->m_lOffset.QuadPart = newoffset;
	if (plibNewPosition)
		plibNewPosition->QuadPart = instance->m_lOffset.QuadPart;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_SetSize(IStream* This, ULARGE_INTEGER libNewSize)
{
	CliprdrStream* instance = (CliprdrStream*) This;

	return STG_E_INSUFFICIENTMEMORY;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_CopyTo(IStream* This, IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER *pcbWritten)
{
	CliprdrStream* instance = (CliprdrStream*) This;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Commit(IStream* This, DWORD grfCommitFlags)
{
	CliprdrStream* instance = (CliprdrStream*) This;

	return STG_E_MEDIUMFULL;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Revert(IStream* This)
{
	CliprdrStream* instance = (CliprdrStream*) This;

	return STG_E_INSUFFICIENTMEMORY;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_LockRegion(IStream* This, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	CliprdrStream* instance = (CliprdrStream*) This;

	return STG_E_INSUFFICIENTMEMORY;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_UnlockRegion(IStream* This, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	CliprdrStream* instance = (CliprdrStream*) This;

	return STG_E_INSUFFICIENTMEMORY;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Stat(IStream* This, STATSTG* pstatstg, DWORD grfStatFlag)
{
	CliprdrStream* instance = (CliprdrStream*) This;

	if (pstatstg == NULL)
		return STG_E_INVALIDPOINTER;

	ZeroMemory(pstatstg, sizeof(STATSTG));

	switch (grfStatFlag)
	{
		case STATFLAG_DEFAULT:
			return STG_E_INSUFFICIENTMEMORY;

		case STATFLAG_NONAME:
			pstatstg->cbSize.QuadPart = instance->m_lSize.QuadPart;
			pstatstg->grfLocksSupported = LOCK_EXCLUSIVE;
			pstatstg->grfMode = GENERIC_READ;
			pstatstg->grfStateBits = 0;
			pstatstg->type = STGTY_STREAM;
			break;

		case STATFLAG_NOOPEN:
			return STG_E_INVALIDFLAG;

		default:
			return STG_E_INVALIDFLAG;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Clone(IStream* This, IStream** ppstm)
{
	CliprdrStream* instance = (CliprdrStream*) This;

	return STG_E_INSUFFICIENTMEMORY;
}

CliprdrStream *CliprdrStream_New(LONG index, void* pData)
{
	IStream* iStream;
	CliprdrStream* instance;
	cliprdrContext* cliprdr = (cliprdrContext*) pData;

	instance = (CliprdrStream*) calloc(1, sizeof(CliprdrStream));

	if (instance)
	{
		iStream = &instance->iStream;

		iStream->lpVtbl = (IStreamVtbl*) calloc(1, sizeof(IStreamVtbl));

		if (iStream->lpVtbl)
		{
			iStream->lpVtbl->QueryInterface = CliprdrStream_QueryInterface;
			iStream->lpVtbl->AddRef = CliprdrStream_AddRef;
			iStream->lpVtbl->Release = CliprdrStream_Release;
			iStream->lpVtbl->Read = CliprdrStream_Read;
			iStream->lpVtbl->Write = CliprdrStream_Write;
			iStream->lpVtbl->Seek = CliprdrStream_Seek;
			iStream->lpVtbl->SetSize = CliprdrStream_SetSize;
			iStream->lpVtbl->CopyTo = CliprdrStream_CopyTo;
			iStream->lpVtbl->Commit = CliprdrStream_Commit;
			iStream->lpVtbl->Revert = CliprdrStream_Revert;
			iStream->lpVtbl->LockRegion = CliprdrStream_LockRegion;
			iStream->lpVtbl->UnlockRegion = CliprdrStream_UnlockRegion;
			iStream->lpVtbl->Stat = CliprdrStream_Stat;
			iStream->lpVtbl->Clone = CliprdrStream_Clone;

			instance->m_lRefCount = 1;
			instance->m_lIndex = index;
			instance->m_pData = pData;
			instance->m_lOffset.QuadPart = 0;

			/* get content size of this stream */
			cliprdr_send_request_filecontents(cliprdr, (void*) instance,
				instance->m_lIndex, FILECONTENTS_SIZE, 0, 0, 8);
			instance->m_lSize.QuadPart = *(LONGLONG*)cliprdr->req_fdata;
			free(cliprdr->req_fdata);
		}
		else
		{
			free(instance);
			instance = NULL;
		}
	}

	return instance;
}

void CliprdrStream_Delete(CliprdrStream* instance)
{
	if (instance)
	{
		if (instance->iStream.lpVtbl)
			free(instance->iStream.lpVtbl);

		free(instance);
	}
}

/**
 * IDataObject
 */

static int cliprdr_lookup_format(CliprdrDataObject* instance, FORMATETC* pFormatEtc)
{
	int i;

	for (i = 0; i < instance->m_nNumFormats; i++)
	{
		if ((pFormatEtc->tymed & instance->m_pFormatEtc[i].tymed) &&
			pFormatEtc->cfFormat == instance->m_pFormatEtc[i].cfFormat &&
			pFormatEtc->dwAspect == instance->m_pFormatEtc[i].dwAspect)
		{
			return i;
		}
	}

	return -1;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_QueryInterface(IDataObject* This, REFIID riid, void** ppvObject)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	if (IsEqualIID(riid, &IID_IDataObject) || IsEqualIID(riid, &IID_IUnknown))
	{
		IDataObject_AddRef(This);
		*ppvObject = This;
		return S_OK;
	}
	else
	{
		*ppvObject = 0;
		return E_NOINTERFACE;
	}
}

ULONG STDMETHODCALLTYPE CliprdrDataObject_AddRef(IDataObject* This)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	return InterlockedIncrement(&instance->m_lRefCount);
}

ULONG STDMETHODCALLTYPE CliprdrDataObject_Release(IDataObject* This)
{
	LONG count;
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	count = InterlockedDecrement(&instance->m_lRefCount);

	if (count == 0)
	{
		CliprdrDataObject_Delete(instance);
		return 0;
	}
	else
	{
		return count;
	}
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetData(IDataObject* This, FORMATETC* pFormatEtc, STGMEDIUM* pMedium)
{
	int i, idx;
	CliprdrDataObject* instance = (CliprdrDataObject*) This;
	cliprdrContext* cliprdr = (cliprdrContext*) instance->m_pData;

	if (!pFormatEtc || !pMedium)
	{
		return E_INVALIDARG;
	}

	if ((idx = cliprdr_lookup_format(instance, pFormatEtc)) == -1)
	{
		return DV_E_FORMATETC;
	}

	pMedium->tymed = instance->m_pFormatEtc[idx].tymed;
	pMedium->pUnkForRelease = 0;

	if (instance->m_pFormatEtc[idx].cfFormat == cliprdr->ID_FILEDESCRIPTORW)
	{
		if (cliprdr_send_data_request(cliprdr, instance->m_pFormatEtc[idx].cfFormat) != 0)
			return E_UNEXPECTED;

		pMedium->hGlobal = cliprdr->hmem;   /* points to a FILEGROUPDESCRIPTOR structure */

		/* GlobalLock returns a pointer to the first byte of the memory block,
		* in which is a FILEGROUPDESCRIPTOR structure, whose first UINT member
		* is the number of FILEDESCRIPTOR's */
		instance->m_nStreams = *(PUINT)GlobalLock(cliprdr->hmem);
		GlobalUnlock(cliprdr->hmem);

		if (instance->m_nStreams > 0)
		{
			if (!instance->m_pStream)
			{
				instance->m_pStream = (LPSTREAM*) calloc(instance->m_nStreams, sizeof(LPSTREAM));

				if (instance->m_pStream)
				{
					for (i = 0; i < instance->m_nStreams; i++)
					{
						instance->m_pStream[i] = (IStream*) CliprdrStream_New(i, cliprdr);
					}
				}
			}
		}

		if (!instance->m_pStream)
		{
			cliprdr->hmem = GlobalFree(cliprdr->hmem);
			pMedium->hGlobal = cliprdr->hmem;
			return E_OUTOFMEMORY;
		}
	}
	else if (instance->m_pFormatEtc[idx].cfFormat == cliprdr->ID_FILECONTENTS)
	{
		if (pFormatEtc->lindex < instance->m_nStreams)
		{
			pMedium->pstm = instance->m_pStream[pFormatEtc->lindex];
			IDataObject_AddRef(instance->m_pStream[pFormatEtc->lindex]);
		}
		else
		{
			return E_INVALIDARG;
		}
	}
	else if (instance->m_pFormatEtc[idx].cfFormat == cliprdr->ID_PREFERREDDROPEFFECT)
	{
		if (cliprdr_send_data_request(cliprdr, instance->m_pFormatEtc[idx].cfFormat) != 0)
			return E_UNEXPECTED;

		pMedium->hGlobal = cliprdr->hmem;
	}
	else
	{
		return E_UNEXPECTED;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetDataHere(IDataObject* This, FORMATETC *pformatetc, STGMEDIUM *pmedium)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	return DATA_E_FORMATETC;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_QueryGetData(IDataObject* This, FORMATETC *pformatetc)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	if (!pformatetc)
		return E_INVALIDARG;

	return (cliprdr_lookup_format(instance, pformatetc) == -1) ? DV_E_FORMATETC : S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetCanonicalFormatEtc(IDataObject* This, FORMATETC *pformatectIn, FORMATETC *pformatetcOut)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	if (!pformatetcOut)
		return E_INVALIDARG;

	pformatetcOut->ptd = NULL;

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_SetData(IDataObject* This, FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_EnumFormatEtc(IDataObject* This, DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	if (!ppenumFormatEtc)
		return E_INVALIDARG;

	if (dwDirection == DATADIR_GET)
	{
		*ppenumFormatEtc = (IEnumFORMATETC*) CliprdrEnumFORMATETC_New(instance->m_nNumFormats, instance->m_pFormatEtc);
		return (*ppenumFormatEtc) ? S_OK : E_OUTOFMEMORY;
	}
	else
	{
		return E_NOTIMPL;
	}
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_DAdvise(IDataObject* This, FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink, DWORD *pdwConnection)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	return OLE_E_ADVISENOTSUPPORTED;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_DUnadvise(IDataObject* This, DWORD dwConnection)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	return OLE_E_ADVISENOTSUPPORTED;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_EnumDAdvise(IDataObject* This, IEnumSTATDATA** ppenumAdvise)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	return OLE_E_ADVISENOTSUPPORTED;
}

CliprdrDataObject* CliprdrDataObject_New(FORMATETC* fmtetc, STGMEDIUM* stgmed, int count, void* data)
{
	int i;
	CliprdrDataObject* instance;
	IDataObject* iDataObject;

	instance = (CliprdrDataObject*) calloc(1, sizeof(CliprdrDataObject));

	if (instance)
	{
		iDataObject = &instance->iDataObject;

		iDataObject->lpVtbl = (IDataObjectVtbl*) calloc(1, sizeof(IDataObjectVtbl));

		if (iDataObject->lpVtbl)
		{
			iDataObject->lpVtbl->QueryInterface = CliprdrDataObject_QueryInterface;
			iDataObject->lpVtbl->AddRef = CliprdrDataObject_AddRef;
			iDataObject->lpVtbl->Release = CliprdrDataObject_Release;
			iDataObject->lpVtbl->GetData = CliprdrDataObject_GetData;
			iDataObject->lpVtbl->GetDataHere = CliprdrDataObject_GetDataHere;
			iDataObject->lpVtbl->QueryGetData = CliprdrDataObject_QueryGetData;
			iDataObject->lpVtbl->GetCanonicalFormatEtc = CliprdrDataObject_GetCanonicalFormatEtc;
			iDataObject->lpVtbl->SetData = CliprdrDataObject_SetData;
			iDataObject->lpVtbl->EnumFormatEtc = CliprdrDataObject_EnumFormatEtc;
			iDataObject->lpVtbl->DAdvise = CliprdrDataObject_DAdvise;
			iDataObject->lpVtbl->DUnadvise = CliprdrDataObject_DUnadvise;
			iDataObject->lpVtbl->EnumDAdvise = CliprdrDataObject_EnumDAdvise;

			instance->m_lRefCount = 1;
			instance->m_nNumFormats = count;
			instance->m_pData = data;
			instance->m_nStreams = 0;
			instance->m_pStream = NULL;

			instance->m_pFormatEtc = (FORMATETC*) calloc(count, sizeof(FORMATETC));
			instance->m_pStgMedium = (STGMEDIUM*) calloc(count, sizeof(STGMEDIUM));

			for (i = 0; i < count; i++)
			{
				instance->m_pFormatEtc[i] = fmtetc[i];
				instance->m_pStgMedium[i] = stgmed[i];
			}
		}
		else
		{
			free(instance);
			instance = NULL;
		}
	}

	return instance;
}

void CliprdrDataObject_Delete(CliprdrDataObject* instance)
{
	if (instance)
	{
		if (instance->iDataObject.lpVtbl)
			free(instance->iDataObject.lpVtbl);

		if (instance->m_pFormatEtc)
			free(instance->m_pFormatEtc);

		if (instance->m_pStgMedium)
			free(instance->m_pStgMedium);

		if (instance->m_pStream)
		{
			int i;

			for (i = 0; i < instance->m_nStreams; i++)
			{
				CliprdrStream_Release(instance->m_pStream[i]);
			}

			free(instance->m_pStream);
		}

		free(instance);
	}
}

BOOL wf_create_file_obj(cliprdrContext* cliprdr, IDataObject** ppDataObject)
{
	FORMATETC fmtetc[3];
	STGMEDIUM stgmeds[3];

	if (!ppDataObject)
		return FALSE;

	fmtetc[0].cfFormat = RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW);
	fmtetc[0].dwAspect = DVASPECT_CONTENT;
	fmtetc[0].lindex = 0;
	fmtetc[0].ptd = NULL;
	fmtetc[0].tymed = TYMED_HGLOBAL;

	stgmeds[0].tymed = TYMED_HGLOBAL;
	stgmeds[0].hGlobal = NULL;
	stgmeds[0].pUnkForRelease = NULL;

	fmtetc[1].cfFormat = RegisterClipboardFormatW(CFSTR_FILECONTENTS);
	fmtetc[1].dwAspect = DVASPECT_CONTENT;
	fmtetc[1].lindex = 0;
	fmtetc[1].ptd = NULL;
	fmtetc[1].tymed = TYMED_ISTREAM;

	stgmeds[1].tymed = TYMED_ISTREAM;
	stgmeds[1].pstm = NULL;
	stgmeds[1].pUnkForRelease = NULL;

	fmtetc[2].cfFormat = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
	fmtetc[2].dwAspect = DVASPECT_CONTENT;
	fmtetc[2].lindex = 0;
	fmtetc[2].ptd = NULL;
	fmtetc[2].tymed = TYMED_HGLOBAL;

	stgmeds[2].tymed = TYMED_HGLOBAL;
	stgmeds[2].hGlobal = NULL;
	stgmeds[2].pUnkForRelease = NULL;

	*ppDataObject = (IDataObject*) CliprdrDataObject_New(fmtetc, stgmeds, 3, cliprdr);

	return (*ppDataObject) ? TRUE : FALSE;
}

void wf_destroy_file_obj(IDataObject* instance)
{
	if (instance)
		IDataObject_Release(instance);
}

/**
 * IEnumFORMATETC
 */

static void cliprdr_format_deep_copy(FORMATETC* dest, FORMATETC* source)
{
	*dest = *source;

	if (source->ptd)
	{
		dest->ptd = (DVTARGETDEVICE*) CoTaskMemAlloc(sizeof(DVTARGETDEVICE));
		*(dest->ptd) = *(source->ptd);
	}
}

HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_QueryInterface(IEnumFORMATETC* This, REFIID riid, void** ppvObject)
{
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*) This;

	if (IsEqualIID(riid, &IID_IEnumFORMATETC) || IsEqualIID(riid, &IID_IUnknown))
	{
		IEnumFORMATETC_AddRef(This);
		*ppvObject = This;
		return S_OK;
	}
	else
	{
		*ppvObject = 0;
		return E_NOINTERFACE;
	}
}

ULONG STDMETHODCALLTYPE CliprdrEnumFORMATETC_AddRef(IEnumFORMATETC* This)
{
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*) This;

	return InterlockedIncrement(&instance->m_lRefCount);
}

ULONG STDMETHODCALLTYPE CliprdrEnumFORMATETC_Release(IEnumFORMATETC* This)
{
	LONG count;
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*) This;

	count = InterlockedDecrement(&instance->m_lRefCount);

	if (count == 0)
	{
		CliprdrEnumFORMATETC_Delete(instance);
		return 0;
	}
	else
	{
		return count;
	}
}

HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Next(IEnumFORMATETC* This, ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched)
{
	ULONG copied  = 0;
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*) This;

	if (celt == 0 || !rgelt)
		return E_INVALIDARG;

	while (instance->m_nIndex < instance->m_nNumFormats && copied < celt)
	{
		cliprdr_format_deep_copy(&rgelt[copied++], &instance->m_pFormatEtc[instance->m_nIndex++]);
	}

	if (pceltFetched != 0)
		*pceltFetched = copied;

	return (copied == celt) ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Skip(IEnumFORMATETC* This, ULONG celt)
{
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*) This;

	if (instance->m_nIndex + (LONG) celt > instance->m_nNumFormats)
		return S_FALSE;

	instance->m_nIndex += celt;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Reset(IEnumFORMATETC* This)
{
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*) This;

	instance->m_nIndex = 0;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Clone(IEnumFORMATETC* This, IEnumFORMATETC **ppEnum)
{
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*) This;

	if (!ppEnum)
		return E_INVALIDARG;

	*ppEnum = (IEnumFORMATETC*) CliprdrEnumFORMATETC_New(instance->m_nNumFormats, instance->m_pFormatEtc);

	if (!*ppEnum)
		return E_OUTOFMEMORY;

	((CliprdrEnumFORMATETC*) *ppEnum)->m_nIndex = instance->m_nIndex;

	return S_OK;
}

CliprdrEnumFORMATETC* CliprdrEnumFORMATETC_New(int nFormats, FORMATETC* pFormatEtc)
{
	int i;
	CliprdrEnumFORMATETC* instance;
	IEnumFORMATETC* iEnumFORMATETC;

	if (!pFormatEtc)
		return NULL;

	instance = (CliprdrEnumFORMATETC*) calloc(1, sizeof(CliprdrEnumFORMATETC));

	if (instance)
	{
		iEnumFORMATETC = &instance->iEnumFORMATETC;

		iEnumFORMATETC->lpVtbl = (IEnumFORMATETCVtbl*) calloc(1, sizeof(IEnumFORMATETCVtbl));

		if (iEnumFORMATETC->lpVtbl)
		{
			iEnumFORMATETC->lpVtbl->QueryInterface = CliprdrEnumFORMATETC_QueryInterface;
			iEnumFORMATETC->lpVtbl->AddRef = CliprdrEnumFORMATETC_AddRef;
			iEnumFORMATETC->lpVtbl->Release = CliprdrEnumFORMATETC_Release;
			iEnumFORMATETC->lpVtbl->Next = CliprdrEnumFORMATETC_Next;
			iEnumFORMATETC->lpVtbl->Skip = CliprdrEnumFORMATETC_Skip;
			iEnumFORMATETC->lpVtbl->Reset = CliprdrEnumFORMATETC_Reset;
			iEnumFORMATETC->lpVtbl->Clone = CliprdrEnumFORMATETC_Clone;

			instance->m_lRefCount = 0;
			instance->m_nIndex = 0;
			instance->m_nNumFormats = nFormats;
			instance->m_pFormatEtc = (FORMATETC*) calloc(nFormats, sizeof(FORMATETC));

			for (i = 0; i < nFormats; i++)
			{
				cliprdr_format_deep_copy(&instance->m_pFormatEtc[i], &pFormatEtc[i]);
			}
		}
		else
		{
			free(instance);
			instance = NULL;
		}
	}

	return instance;
}

void CliprdrEnumFORMATETC_Delete(CliprdrEnumFORMATETC* instance)
{
	int i;

	if (instance)
	{
		if (instance->iEnumFORMATETC.lpVtbl)
			free(instance->iEnumFORMATETC.lpVtbl);

		if (instance->m_pFormatEtc)
		{
			for (i = 0; i < instance->m_nNumFormats; i++)
			{
				if (instance->m_pFormatEtc[i].ptd)
					CoTaskMemFree(instance->m_pFormatEtc[i].ptd);
			}

			free(instance->m_pFormatEtc);
		}

		free(instance);
	}
}

/***********************************************************************************/

static UINT32 get_local_format_id_by_name(cliprdrContext* cliprdr, void* format_name)
{
	int i;
	formatMapping* map;

	for (i = 0; i < cliprdr->map_size; i++)
	{
		map = &cliprdr->format_mappings[i];

		if ((cliprdr->capabilities & CB_USE_LONG_FORMAT_NAMES) != 0)
		{
			if (map->name)
			{
				if (memcmp(map->name, format_name, wcslen((LPCWSTR)format_name)) == 0)
					return map->local_format_id;
			}
		}
	}

	return 0;
}

static INLINE BOOL file_transferring(cliprdrContext* cliprdr)
{
	return !!get_local_format_id_by_name(cliprdr, L"FileGroupDescriptorW");
}

static UINT32 get_remote_format_id(cliprdrContext* cliprdr, UINT32 local_format)
{
	int i;
	formatMapping* map;

	for (i = 0; i < cliprdr->map_size; i++)
	{
		map = &cliprdr->format_mappings[i];

		if (map->local_format_id == local_format)
		{
			return map->remote_format_id;
		}
	}

	return local_format;
}

static void map_ensure_capacity(cliprdrContext* cliprdr)
{
	if (cliprdr->map_size >= cliprdr->map_capacity)
	{
		cliprdr->map_capacity *= 2;
		cliprdr->format_mappings = (formatMapping*) realloc(cliprdr->format_mappings,
			sizeof(formatMapping) * cliprdr->map_capacity);
	}
}

static void clear_format_map(cliprdrContext* cliprdr)
{
	int i;
	formatMapping* map;

	if (cliprdr->format_mappings)
	{
		for (i = 0; i < cliprdr->map_capacity; i++)
		{
			map = &cliprdr->format_mappings[i];
			map->remote_format_id = 0;
			map->local_format_id = 0;

			if (map->name)
			{
				free(map->name);
				map->name = NULL;
			}
		}
	}

	cliprdr->map_size = 0;
}
/*
2.2.2.3   Client Temporary Directory PDU (CLIPRDR_TEMP_DIRECTORY)
  The Temporary Directory PDU is an optional PDU sent from the client to the server.
	This PDU informs the server of a location on the client file system that MUST be
	used to deposit files being copied to the client. The location MUST be accessible
	by the server to be useful. Section 3.1.1.3 specifies how direct file access
	impacts file copy and paste.
*/
int cliprdr_send_tempdir(cliprdrContext* cliprdr)
{
	RDP_CB_TEMPDIR_EVENT* cliprdr_event;

	cliprdr_event = (RDP_CB_TEMPDIR_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_TemporaryDirectory, NULL, NULL);

	if (!cliprdr_event)
		return -1;

	/* Sending the TEMP path would only be valid iff the path is accessible from the server.
		 This should perhaps to change to a command line parameter value
	*/
	GetEnvironmentVariableW(L"TEMP", (LPWSTR) cliprdr_event->dirname, MAX_PATH);

	return freerdp_channels_send_event(cliprdr->channels, (wMessage*) cliprdr_event);
}

static void cliprdr_send_format_list(cliprdrContext* cliprdr)
{
	BYTE* format_data;
	int format = 0;
	int data_size;
	int format_count;
	int len = 0;
	int namelen;
	int stream_file_transferring = FALSE;
	RDP_CB_FORMAT_LIST_EVENT* cliprdr_event;

	if (!OpenClipboard(cliprdr->hwndClipboard))
	{
		DEBUG_CLIPRDR("OpenClipboard failed with 0x%x", GetLastError());
		return;
	}

	format_count = CountClipboardFormats();
	data_size = format_count * (4 + MAX_PATH * 2);

	format_data = (BYTE*) calloc(1, data_size);
	assert(format_data != NULL);

	while (format = EnumClipboardFormats(format))
	{
		Write_UINT32(format_data + len, format);
		len += 4;

		if ((cliprdr->capabilities & CB_USE_LONG_FORMAT_NAMES) != 0)
		{
			if (format >= CF_MAX)
			{
				namelen = GetClipboardFormatNameW(format, (LPWSTR)(format_data + len), MAX_PATH);

				if ((wcscmp((LPWSTR)(format_data + len), L"FileNameW") == 0) ||
					(wcscmp((LPWSTR)(format_data + len), L"FileName") == 0) || (wcscmp((LPWSTR)(format_data + len), CFSTR_FILEDESCRIPTORW) == 0)) {
					stream_file_transferring = TRUE;
				}

				len += namelen * sizeof(WCHAR);
			}
			len += 2;							/* end of Unicode string */
		}
		else
		{
			if (format >= CF_MAX)
			{
				int wLen;
				static WCHAR wName[MAX_PATH] = { 0 };

				ZeroMemory(wName, MAX_PATH * 2);

				wLen = GetClipboardFormatNameW(format, wName, MAX_PATH);

				if (wLen < 16)
				{
					memcpy(format_data + len, wName, wLen * sizeof(WCHAR));
				}
				else
				{
					memcpy(format_data + len, wName, 32);	/* truncate the long name to 32 bytes */
				}
			}
			len += 32;
		}
	}

	CloseClipboard();

	cliprdr_event = (RDP_CB_FORMAT_LIST_EVENT *) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FormatList, NULL, NULL);

	if (stream_file_transferring)
	{
		cliprdr_event->raw_format_data_size = 4 + 42;
		cliprdr_event->raw_format_data = (BYTE*) calloc(1, cliprdr_event->raw_format_data_size);
		format = RegisterClipboardFormatW(L"FileGroupDescriptorW");
		Write_UINT32(cliprdr_event->raw_format_data, format);
		wcscpy_s((WCHAR*)(cliprdr_event->raw_format_data + 4),
			(cliprdr_event->raw_format_data_size - 4) / 2, L"FileGroupDescriptorW");
	}
	else
	{
		cliprdr_event->raw_format_data = (BYTE*) calloc(1, len);
		assert(cliprdr_event->raw_format_data != NULL);

		CopyMemory(cliprdr_event->raw_format_data, format_data, len);
		cliprdr_event->raw_format_data_size = len;
	}
	free(format_data);

	freerdp_channels_send_event(cliprdr->channels, (wMessage*) cliprdr_event);
}

int cliprdr_send_data_request(cliprdrContext* cliprdr, UINT32 format)
{
	int ret;
	RDP_CB_DATA_REQUEST_EVENT* cliprdr_event;

	cliprdr_event = (RDP_CB_DATA_REQUEST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_DataRequest, NULL, NULL);

	if (!cliprdr_event)
		return -1;

	cliprdr_event->format = get_remote_format_id(cliprdr, format);

	ret = freerdp_channels_send_event(cliprdr->channels, (wMessage*) cliprdr_event);

	if (ret != 0)
		return -1;

	WaitForSingleObject(cliprdr->response_data_event, INFINITE);
	ResetEvent(cliprdr->response_data_event);

	return 0;
}

int cliprdr_send_lock(cliprdrContext* cliprdr)
{
	int ret;
	RDP_CB_LOCK_CLIPDATA_EVENT* cliprdr_event;

	if ((cliprdr->capabilities & CB_CAN_LOCK_CLIPDATA) == 1)
	{
		cliprdr_event = (RDP_CB_LOCK_CLIPDATA_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_LockClipdata, NULL, NULL);

		if (!cliprdr_event)
			return -1;

		cliprdr_event->clipDataId = 0;

		ret = freerdp_channels_send_event(cliprdr->channels, (wMessage*) cliprdr_event);

		if (ret != 0)
			return -1;
	}

	return 0;
}

int cliprdr_send_unlock(cliprdrContext* cliprdr)
{
	int ret;
	RDP_CB_UNLOCK_CLIPDATA_EVENT* cliprdr_event;

	if ((cliprdr->capabilities & CB_CAN_LOCK_CLIPDATA) == 1)
	{
		cliprdr_event = (RDP_CB_UNLOCK_CLIPDATA_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_UnLockClipdata, NULL, NULL);

		if (!cliprdr_event)
			return -1;

		cliprdr_event->clipDataId = 0;

		ret = freerdp_channels_send_event(cliprdr->channels, (wMessage*) cliprdr_event);

		if (ret != 0)
			return -1;
	}

	return 0;
}

int cliprdr_send_request_filecontents(cliprdrContext* cliprdr, void* streamid,
		int index, int flag, DWORD positionhigh, DWORD positionlow, ULONG nreq)
{
	int ret;
	RDP_CB_FILECONTENTS_REQUEST_EVENT* cliprdr_event;

	cliprdr_event = (RDP_CB_FILECONTENTS_REQUEST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FilecontentsRequest, NULL, NULL);

	if (!cliprdr_event)
		return -1;

	cliprdr_event->streamId = (UINT32)streamid;
	cliprdr_event->lindex = index;
	cliprdr_event->dwFlags = flag;
	cliprdr_event->nPositionLow = positionlow;
	cliprdr_event->nPositionHigh = positionhigh;
	cliprdr_event->cbRequested = nreq;
	cliprdr_event->clipDataId = 0;

	ret = freerdp_channels_send_event(cliprdr->channels, (wMessage*) cliprdr_event);

	if (ret != 0)
		return -1;

	WaitForSingleObject(cliprdr->req_fevent, INFINITE);
	ResetEvent(cliprdr->req_fevent);

	return 0;
}

int cliprdr_send_response_filecontents(cliprdrContext* cliprdr, UINT32 streamid, UINT32 size, BYTE* data)
{
	int ret;
	RDP_CB_FILECONTENTS_RESPONSE_EVENT* cliprdr_event;

	cliprdr_event = (RDP_CB_FILECONTENTS_RESPONSE_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FilecontentsResponse, NULL, NULL);

	if (!cliprdr_event)
		return -1;

	cliprdr_event->streamId = streamid;
	cliprdr_event->size = size;
	cliprdr_event->data = data;

	ret = freerdp_channels_send_event(cliprdr->channels, (wMessage *)cliprdr_event);

	if (ret != 0)
		return -1;

	return 0;
}

static LRESULT CALLBACK cliprdr_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static cliprdrContext* cliprdr = NULL;

	switch (Msg)
	{
		case WM_CREATE:
			DEBUG_CLIPRDR("info: %s - WM_CREATE", __FUNCTION__);
			cliprdr = (cliprdrContext*)((CREATESTRUCT*) lParam)->lpCreateParams;
			if (!AddClipboardFormatListener(hWnd)) {
				DEBUG_CLIPRDR("error: AddClipboardFormatListener failed with %#x.", GetLastError());
			}
			cliprdr->hwndClipboard = hWnd;
			break;

		case WM_CLOSE:
			DEBUG_CLIPRDR("info: %s - WM_CLOSE", __FUNCTION__);
			RemoveClipboardFormatListener(hWnd);
			break;

		case WM_CLIPBOARDUPDATE:
			DEBUG_CLIPRDR("info: %s - WM_CLIPBOARDUPDATE", __FUNCTION__);
			if (cliprdr->channel_initialized)
			{
				if ((GetClipboardOwner() != cliprdr->hwndClipboard) && (S_FALSE == OleIsCurrentClipboard(cliprdr->data_obj)))
				{
						if (!cliprdr->hmem)
						{
							cliprdr->hmem = GlobalFree(cliprdr->hmem);
						}
						cliprdr_send_format_list(cliprdr);
				}
			}
			break;

		case WM_RENDERALLFORMATS:
			DEBUG_CLIPRDR("info: %s - WM_RENDERALLFORMATS", __FUNCTION__);
			/* discard all contexts in clipboard */
			if (!OpenClipboard(cliprdr->hwndClipboard))
			{
				DEBUG_CLIPRDR("OpenClipboard failed with 0x%x", GetLastError());
				break;
			}
			EmptyClipboard();
			CloseClipboard();
			break;

		case WM_RENDERFORMAT:
			DEBUG_CLIPRDR("info: %s - WM_RENDERFORMAT", __FUNCTION__);
			if (cliprdr_send_data_request(cliprdr, (UINT32)wParam) != 0)
			{
				DEBUG_CLIPRDR("error: cliprdr_send_data_request failed.");
				break;
			}

			if (SetClipboardData((UINT) wParam, cliprdr->hmem) == NULL)
			{
				DEBUG_CLIPRDR("SetClipboardData failed with 0x%x", GetLastError());
				cliprdr->hmem = GlobalFree(cliprdr->hmem);
			}
			/* Note: GlobalFree() is not needed when success */
			break;

		case WM_CLIPRDR_MESSAGE:
			DEBUG_CLIPRDR("info: %s - WM_CLIPRDR_MESSAGE", __FUNCTION__);
			switch (wParam)
			{
				case OLE_SETCLIPBOARD:
					DEBUG_CLIPRDR("info: %s - OLE_SETCLIPBOARD", __FUNCTION__);
					if (wf_create_file_obj(cliprdr, &cliprdr->data_obj))
					{
						if (OleSetClipboard(cliprdr->data_obj) != S_OK)
						{
							wf_destroy_file_obj(cliprdr->data_obj);
							cliprdr->data_obj = NULL;
						}
					}
					break;

				default:
					break;
			}
			break;

		case WM_DESTROYCLIPBOARD:
		case WM_ASKCBFORMATNAME:
		case WM_HSCROLLCLIPBOARD:
		case WM_PAINTCLIPBOARD:
		case WM_SIZECLIPBOARD:
		case WM_VSCROLLCLIPBOARD:
		default:
			return DefWindowProc(hWnd, Msg, wParam, lParam);
	}

	return 0;
}

static int create_cliprdr_window(cliprdrContext* cliprdr)
{
	WNDCLASSEX wnd_cls;

	ZeroMemory(&wnd_cls, sizeof(WNDCLASSEX));
	wnd_cls.cbSize = sizeof(WNDCLASSEX);
	wnd_cls.style = CS_OWNDC;
	wnd_cls.lpfnWndProc = cliprdr_proc;
	wnd_cls.cbClsExtra = 0;
	wnd_cls.cbWndExtra = 0;
	wnd_cls.hIcon = NULL;
	wnd_cls.hCursor = NULL;
	wnd_cls.hbrBackground = NULL;
	wnd_cls.lpszMenuName = NULL;
	wnd_cls.lpszClassName = L"ClipboardHiddenMessageProcessor";
	wnd_cls.hInstance = GetModuleHandle(NULL);
	wnd_cls.hIconSm = NULL;

	RegisterClassEx(&wnd_cls);

	cliprdr->hwndClipboard = CreateWindowEx(WS_EX_LEFT,
		L"ClipboardHiddenMessageProcessor",
		L"rdpclip",
		0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), cliprdr);

	if (cliprdr->hwndClipboard == NULL)
	{
		DEBUG_CLIPRDR("error: CreateWindowEx failed with %x.", GetLastError());
		return -1;
	}

	return 0;
}

static void* cliprdr_thread_func(void* arg)
{
	int ret;
	MSG msg;
	BOOL mcode;
	cliprdrContext* cliprdr = (cliprdrContext*) arg;

	OleInitialize(0);

	if ((ret = create_cliprdr_window(cliprdr)) != 0)
	{
		DEBUG_CLIPRDR("error: create clipboard window failed.");
		return NULL;
	}

	while ((mcode = GetMessage(&msg, 0, 0, 0)) != 0)
	{
		if (mcode == -1)
		{
			DEBUG_CLIPRDR("error: clipboard thread GetMessage failed.");
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	OleUninitialize();

	return NULL;
}

static void clear_file_array(cliprdrContext* cliprdr)
{
	int i;

	/* clear file_names array */
	if (cliprdr->file_names)
	{
		for (i = 0; i < cliprdr->nFiles; i++)
		{
			if (cliprdr->file_names[i])
			{
				free(cliprdr->file_names[i]);
				cliprdr->file_names[i] = NULL;
			}
		}
	}

	/* clear fileDescriptor array */
	if (cliprdr->fileDescriptor)
	{
		for (i = 0; i < cliprdr->nFiles; i++)
		{
			if (cliprdr->fileDescriptor[i])
			{
				free(cliprdr->fileDescriptor[i]);
				cliprdr->fileDescriptor[i] = NULL;
			}
		}
	}

	cliprdr->nFiles = 0;
}

void wf_cliprdr_init(wfContext* wfc, rdpChannels* channels)
{
	cliprdrContext* cliprdr;

	if (!wfc->instance->settings->RedirectClipboard)
	{
		wfc->cliprdr_context = NULL;
		WLog_ERR(TAG,  "clipboard is not redirected.");
		return;
	}

	wfc->cliprdr_context = (cliprdrContext*) calloc(1, sizeof(cliprdrContext));
	cliprdr = (cliprdrContext*) wfc->cliprdr_context;
	assert(cliprdr != NULL);

	cliprdr->channels = channels;
	cliprdr->channel_initialized = FALSE;

	cliprdr->map_capacity = 32;
	cliprdr->map_size = 0;

	cliprdr->format_mappings = (formatMapping*) calloc(1, sizeof(formatMapping) * cliprdr->map_capacity);
	assert(cliprdr->format_mappings != NULL);

	cliprdr->file_array_size = 32;
	cliprdr->file_names = (WCHAR**) calloc(1, cliprdr->file_array_size * sizeof(WCHAR*));
	cliprdr->fileDescriptor = (FILEDESCRIPTORW**) calloc(1, cliprdr->file_array_size * sizeof(FILEDESCRIPTORW*));

	cliprdr->response_data_event = CreateEvent(NULL, TRUE, FALSE, L"response_data_event");
	assert(cliprdr->response_data_event != NULL);

	cliprdr->req_fevent = CreateEvent(NULL, TRUE, FALSE, L"request_filecontents_event");
	cliprdr->ID_FILEDESCRIPTORW = RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW);
	cliprdr->ID_FILECONTENTS = RegisterClipboardFormatW(CFSTR_FILECONTENTS);
	cliprdr->ID_PREFERREDDROPEFFECT = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);

	cliprdr->cliprdr_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) cliprdr_thread_func, cliprdr, 0, NULL);
	assert(cliprdr->cliprdr_thread != NULL);
}

void wf_cliprdr_uninit(wfContext* wfc)
{
	cliprdrContext* cliprdr = (cliprdrContext*) wfc->cliprdr_context;

	if (!cliprdr)
		return;

	if (cliprdr->hwndClipboard)
		PostMessage(cliprdr->hwndClipboard, WM_QUIT, 0, 0);

	if (cliprdr->cliprdr_thread)
	{
		WaitForSingleObject(cliprdr->cliprdr_thread, INFINITE);
		CloseHandle(cliprdr->cliprdr_thread);
	}

	if (cliprdr->response_data_event)
		CloseHandle(cliprdr->response_data_event);

	clear_file_array(cliprdr);
	clear_format_map(cliprdr);

	if (cliprdr->file_names)
		free(cliprdr->file_names);
	if (cliprdr->fileDescriptor)
		free(cliprdr->fileDescriptor);
	if (cliprdr->format_mappings)
		free(cliprdr->format_mappings);

	free(cliprdr);
}

static void wf_cliprdr_process_cb_clip_caps_event(wfContext* wfc, RDP_CB_CLIP_CAPS* caps_event)
{
	cliprdrContext* cliprdr = (cliprdrContext*) wfc->cliprdr_context;

	cliprdr->capabilities = caps_event->capabilities;
}

static void wf_cliprdr_process_cb_monitor_ready_event(wfContext* wfc, RDP_CB_MONITOR_READY_EVENT* ready_event)
{
	cliprdrContext* cliprdr = (cliprdrContext*) wfc->cliprdr_context;
#if 0
	/*Disabled since the current function only sends the temp directory which is not 
	  guaranteed to be accessible to the server
	*/
	cliprdr_send_tempdir(cliprdr);
#endif	
	cliprdr->channel_initialized = TRUE;

	cliprdr_send_format_list(wfc->cliprdr_context);
}

static BOOL wf_cliprdr_get_file_contents(WCHAR* file_name, BYTE* buffer,
	int positionLow, int positionHigh, int nRequested, unsigned int* puSize)
{
	HANDLE hFile;
	DWORD nGet;

	if (!file_name || !buffer || !puSize)
	{
		WLog_ERR(TAG,  "get file contents Invalid Arguments.");
		return FALSE;
	}
	
	hFile = CreateFileW(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	SetFilePointer(hFile, positionLow, (PLONG) &positionHigh, FILE_BEGIN);

	if (!ReadFile(hFile, buffer, nRequested, &nGet, NULL))
	{
		DWORD err = GetLastError();
		DEBUG_CLIPRDR("ReadFile failed with 0x%x.", err);
	}

	CloseHandle(hFile);

	*puSize = nGet;

	return TRUE;
}

/* path_name has a '\' at the end. e.g. c:\newfolder\, file_name is c:\newfolder\new.txt */
static FILEDESCRIPTORW* wf_cliprdr_get_file_descriptor(WCHAR* file_name, int pathLen)
{
	HANDLE hFile;
	FILEDESCRIPTORW* fd;

	fd = (FILEDESCRIPTORW*) calloc(1, sizeof(FILEDESCRIPTORW));

	if (!fd)
		return NULL;

	hFile = CreateFileW(file_name, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		free(fd);
		return NULL;
	}

	fd->dwFlags = FD_ATTRIBUTES | FD_FILESIZE | FD_WRITESTIME | FD_PROGRESSUI;

	fd->dwFileAttributes = GetFileAttributes(file_name);

	if (!GetFileTime(hFile, NULL, NULL, &fd->ftLastWriteTime))
	{
		fd->dwFlags &= ~FD_WRITESTIME;
	}

	fd->nFileSizeLow = GetFileSize(hFile, &fd->nFileSizeHigh);

	wcscpy_s(fd->cFileName, sizeof(fd->cFileName) / 2, file_name + pathLen);
	CloseHandle(hFile);

	return fd;
}

static void wf_cliprdr_array_ensure_capacity(cliprdrContext* cliprdr)
{
	if (cliprdr->nFiles == cliprdr->file_array_size)
	{
		cliprdr->file_array_size *= 2;
		cliprdr->fileDescriptor = (FILEDESCRIPTORW**) realloc(cliprdr->fileDescriptor, cliprdr->file_array_size * sizeof(FILEDESCRIPTORW*));
		cliprdr->file_names = (WCHAR**) realloc(cliprdr->file_names, cliprdr->file_array_size * sizeof(WCHAR*));
	}
}

static void wf_cliprdr_add_to_file_arrays(cliprdrContext* cliprdr, WCHAR* full_file_name, int pathLen)
{
	/* add to name array */
	cliprdr->file_names[cliprdr->nFiles] = (LPWSTR) malloc(MAX_PATH * 2);
	wcscpy_s(cliprdr->file_names[cliprdr->nFiles], MAX_PATH, full_file_name);

	/* add to descriptor array */
	cliprdr->fileDescriptor[cliprdr->nFiles] = wf_cliprdr_get_file_descriptor(full_file_name, pathLen);

	cliprdr->nFiles++;

	wf_cliprdr_array_ensure_capacity(cliprdr);
}

static void wf_cliprdr_traverse_directory(cliprdrContext* cliprdr, WCHAR* Dir, int pathLen)
{
	HANDLE hFind;
	WCHAR DirSpec[MAX_PATH];
	WIN32_FIND_DATA FindFileData;

	StringCchCopy(DirSpec, MAX_PATH, Dir);
	StringCchCat(DirSpec, MAX_PATH, TEXT("\\*"));

	hFind = FindFirstFile(DirSpec, &FindFileData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		DEBUG_CLIPRDR("FindFirstFile failed with 0x%x.", GetLastError());
		return;
	}

	while (FindNextFile(hFind, &FindFileData))
	{
		if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
			&& wcscmp(FindFileData.cFileName,L".") == 0
			|| wcscmp(FindFileData.cFileName,L"..") == 0)
		{
			continue;
		}

		if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) !=0)
		{
			WCHAR DirAdd[MAX_PATH];

			StringCchCopy(DirAdd, MAX_PATH, Dir);
			StringCchCat(DirAdd, MAX_PATH, TEXT("\\"));
			StringCchCat(DirAdd, MAX_PATH, FindFileData.cFileName);
			wf_cliprdr_add_to_file_arrays(cliprdr, DirAdd, pathLen);
			wf_cliprdr_traverse_directory(cliprdr, DirAdd, pathLen);
		}
		else
		{
			WCHAR fileName[MAX_PATH];

			StringCchCopy(fileName, MAX_PATH, Dir);
			StringCchCat(fileName, MAX_PATH, TEXT("\\"));
			StringCchCat(fileName, MAX_PATH, FindFileData.cFileName);

			wf_cliprdr_add_to_file_arrays(cliprdr, fileName, pathLen);
		}
	}

	FindClose(hFind);
}

static void wf_cliprdr_process_cb_data_request_event(wfContext* wfc, RDP_CB_DATA_REQUEST_EVENT* event)
{
	HANDLE hClipdata;
	int size = 0;
	char* buff = NULL;
	char* globlemem = NULL;
	UINT32 local_format;
	cliprdrContext* cliprdr = (cliprdrContext*) wfc->cliprdr_context;
	RDP_CB_DATA_RESPONSE_EVENT* response_event;

	local_format = event->format;

	if (local_format == FORMAT_ID_PALETTE)
	{
		/* TODO: implement this */
		DEBUG_CLIPRDR("FORMAT_ID_PALETTE is not supported yet.");
	}
	else if (local_format == FORMAT_ID_METAFILE)
	{
		/* TODO: implement this */
		DEBUG_CLIPRDR("FORMAT_ID_MATEFILE is not supported yet.");
	}
	else if (local_format == RegisterClipboardFormatW(L"FileGroupDescriptorW"))
	{
		int len;
		int i;
		WCHAR* wFileName;
		unsigned int uSize;
		HRESULT result;
		LPDATAOBJECT dataObj;
		FORMATETC format_etc;
		STGMEDIUM stg_medium;
		DROPFILES* dropFiles;

		DEBUG_CLIPRDR("file descriptors request.");

		result = OleGetClipboard(&dataObj);

		if (!SUCCEEDED(result))
		{
			DEBUG_CLIPRDR("OleGetClipboard failed.");
		}

		ZeroMemory(&format_etc, sizeof(FORMATETC));
		ZeroMemory(&stg_medium, sizeof(STGMEDIUM));

		/* try to get FileGroupDescriptorW struct from OLE */
		format_etc.cfFormat = local_format;
		format_etc.tymed = TYMED_HGLOBAL;
		format_etc.dwAspect = 1;
		format_etc.lindex = -1;
		format_etc.ptd = 0;

		result = IDataObject_GetData(dataObj, &format_etc, &stg_medium);

		if (SUCCEEDED(result))
		{
			DEBUG_CLIPRDR("Got FileGroupDescriptorW.");
			globlemem = (char*) GlobalLock(stg_medium.hGlobal);
			uSize = GlobalSize(stg_medium.hGlobal);
			size = uSize;
			buff = (char*) malloc(uSize);
			CopyMemory(buff, globlemem, uSize);
			GlobalUnlock(stg_medium.hGlobal);

			ReleaseStgMedium(&stg_medium);

			clear_file_array(cliprdr);
		}
		else
		{
			/* get DROPFILES struct from OLE */
			format_etc.cfFormat = CF_HDROP;
			format_etc.tymed = TYMED_HGLOBAL;
			format_etc.dwAspect = 1;
			format_etc.lindex = -1;

			result = IDataObject_GetData(dataObj, &format_etc, &stg_medium);

			if (!SUCCEEDED(result)) {
				DEBUG_CLIPRDR("dataObj->GetData failed.");
			}

			globlemem = (char*) GlobalLock(stg_medium.hGlobal);

			if (!globlemem)
			{
				GlobalUnlock(stg_medium.hGlobal);

				ReleaseStgMedium(&stg_medium);
				cliprdr->nFiles = 0;

				goto exit;
			}
			uSize = GlobalSize(stg_medium.hGlobal);

			dropFiles = (DROPFILES*) malloc(uSize);
			memcpy(dropFiles, globlemem, uSize);

			GlobalUnlock(stg_medium.hGlobal);

			ReleaseStgMedium(&stg_medium);

			clear_file_array(cliprdr);

			if (dropFiles->fWide)
			{
				WCHAR* p;
				int str_len;
				int offset;
				int pathLen;

				/* dropFiles contains file names */
				for (wFileName = (WCHAR*)((char*)dropFiles + dropFiles->pFiles); (len = wcslen(wFileName)) > 0; wFileName += len + 1)
				{
					/* get path name */
					str_len = wcslen(wFileName);
					offset = str_len;
					/* find the last '\' in full file name */
					for (p = wFileName + offset; *p != L'\\'; p--)
					{
						;
					}
					p += 1;
					pathLen = wcslen(wFileName) - wcslen(p);

					wf_cliprdr_add_to_file_arrays(cliprdr, wFileName, pathLen);

					if ((cliprdr->fileDescriptor[cliprdr->nFiles - 1]->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
					{
						/* this is a directory */
						wf_cliprdr_traverse_directory(cliprdr, wFileName, pathLen);
					}
				}
			}
			else
			{
				char *p;

				for (p = (char*)((char*)dropFiles + dropFiles->pFiles); (len = strlen(p)) > 0; p += len + 1, cliprdr->nFiles++)
				{
					int cchWideChar;

					cchWideChar = MultiByteToWideChar(CP_ACP, MB_COMPOSITE, p, len, NULL, 0);
					cliprdr->file_names[cliprdr->nFiles] = (LPWSTR) malloc(cchWideChar * 2);
					MultiByteToWideChar(CP_ACP, MB_COMPOSITE, p, len, cliprdr->file_names[cliprdr->nFiles], cchWideChar);

					if (cliprdr->nFiles == cliprdr->file_array_size)
					{
						cliprdr->file_array_size *= 2;
						cliprdr->fileDescriptor = (FILEDESCRIPTORW**) realloc(cliprdr->fileDescriptor, cliprdr->file_array_size * sizeof(FILEDESCRIPTORW*));
						cliprdr->file_names = (WCHAR**) realloc(cliprdr->file_names, cliprdr->file_array_size * sizeof(WCHAR*));
					}
				}
			}

exit:
			size = 4 + cliprdr->nFiles * sizeof(FILEDESCRIPTORW);
			buff = (char*) malloc(size);

			Write_UINT32(buff, cliprdr->nFiles);

			for (i = 0; i < cliprdr->nFiles; i++)
			{
				if (cliprdr->fileDescriptor[i])
				{
					memcpy(buff + 4 + i * sizeof(FILEDESCRIPTORW), cliprdr->fileDescriptor[i], sizeof(FILEDESCRIPTORW));
				}
			}
		}

		IDataObject_Release(dataObj);
	}
	else
	{
		if (!OpenClipboard(cliprdr->hwndClipboard))
		{
			DEBUG_CLIPRDR("OpenClipboard failed with 0x%x", GetLastError());
			return;
		}

		hClipdata = GetClipboardData(event->format);

		if (!hClipdata)
		{
			DEBUG_CLIPRDR("GetClipboardData failed.");
			CloseClipboard();
			return;
		}

		globlemem = (char*) GlobalLock(hClipdata);
		size = (int) GlobalSize(hClipdata);

		buff = (char*) malloc(size);
		memcpy(buff, globlemem, size);

		GlobalUnlock(hClipdata);

		CloseClipboard();
	}

	response_event = (RDP_CB_DATA_RESPONSE_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_DataResponse, NULL, NULL);

	response_event->data = (BYTE*) buff;
	response_event->size = size;

	freerdp_channels_send_event(cliprdr->channels, (wMessage*) response_event);

	/* Note: don't free buffer here. */
}

static void wf_cliprdr_process_cb_format_list_event(wfContext* wfc, RDP_CB_FORMAT_LIST_EVENT* event)
{
	int i = 0;
	BYTE* p;
	BYTE* end_mark;
	BOOL format_forbidden = FALSE;
	cliprdrContext* cliprdr = (cliprdrContext*) wfc->cliprdr_context;
	UINT32 left_size = event->raw_format_data_size;

	/* ignore the formats member in event struct, only parsing raw_format_data */

	p = event->raw_format_data;
	end_mark = p + event->raw_format_data_size;

	clear_format_map(cliprdr);

	if ((cliprdr->capabilities & CB_USE_LONG_FORMAT_NAMES) != 0)
	{
		while (left_size >= 6)
		{
			BYTE* tmp;
			int name_len;
			formatMapping* map;

			map = &cliprdr->format_mappings[i++];

			Read_UINT32(p, map->remote_format_id);
			map->name = NULL;

			/* get name_len */
			for (tmp = p, name_len = 0; tmp + 1 < end_mark; tmp += 2, name_len += 2)
			{
				if (*((unsigned short*) tmp) == 0)
					break;
			}

			if (name_len > 0)
			{
				map->name = malloc(name_len + 2);
				memcpy(map->name, p, name_len + 2);

				map->local_format_id = RegisterClipboardFormatW((LPCWSTR) map->name);
			}
			else
			{
				map->local_format_id = map->remote_format_id;
			}

			left_size -= name_len + 4 + 2;
			p += name_len + 2;          /* p's already +4 when Read_UINT32() is called. */

			cliprdr->map_size++;

			map_ensure_capacity(cliprdr);
		}
	}
	else
	{
		UINT32 k;

		for (k = 0; k < event->raw_format_data_size / 36; k++)
		{
			int name_len;
			formatMapping* map;

			map = &cliprdr->format_mappings[i++];

			Read_UINT32(p, map->remote_format_id);
			map->name = NULL;

			if (event->raw_format_unicode)
			{
				/* get name_len, in bytes, if the file name is truncated, no terminated null will be included. */
				for (name_len = 0; name_len < 32; name_len += 2)
				{
					if (*((unsigned short*) (p + name_len)) == 0)
						break;
				}

				if (name_len > 0)
				{
					map->name = calloc(1, name_len + 2);
					memcpy(map->name, p, name_len);
					map->local_format_id = RegisterClipboardFormatW((LPCWSTR)map->name);
				}
				else
				{
					map->local_format_id = map->remote_format_id;
				}
			}
			else
			{
				/* get name_len, in bytes, if the file name is truncated, no terminated null will be included. */
				for (name_len = 0; name_len < 32; name_len += 1)
				{
					if (*((unsigned char*) (p + name_len)) == 0)
						break;
				}

				if (name_len > 0)
				{
					map->name = calloc(1, name_len + 1);
					memcpy(map->name, p, name_len);
					map->local_format_id = RegisterClipboardFormatA((LPCSTR)map->name);
				}
				else
				{
					map->local_format_id = map->remote_format_id;
				}
			}

			p += 32;          /* p's already +4 when Read_UINT32() is called. */
			cliprdr->map_size++;
			map_ensure_capacity(cliprdr);
		}
	}

	if (file_transferring(cliprdr))
	{
		PostMessage(cliprdr->hwndClipboard, WM_CLIPRDR_MESSAGE, OLE_SETCLIPBOARD, 0);
	}
	else
	{
		if (!OpenClipboard(cliprdr->hwndClipboard))
			return;

		if (EmptyClipboard())
		{
			for (i = 0; i < cliprdr->map_size; i++)
			{
				SetClipboardData(cliprdr->format_mappings[i].local_format_id, NULL);
			}
		}

		CloseClipboard();
	}
}

static void wf_cliprdr_process_cb_data_response_event(wfContext* wfc, RDP_CB_DATA_RESPONSE_EVENT* event)
{
	char* buff;
	HANDLE hMem;
	cliprdrContext* cliprdr = (cliprdrContext*) wfc->cliprdr_context;

	hMem = GlobalAlloc(GMEM_FIXED, event->size);
	buff = (char*) GlobalLock(hMem);
	memcpy(buff, event->data, event->size);
	GlobalUnlock(hMem);

	cliprdr->hmem = hMem;
	SetEvent(cliprdr->response_data_event);
}

static void wf_cliprdr_process_cb_filecontents_request_event(wfContext* wfc, RDP_CB_FILECONTENTS_REQUEST_EVENT* event)
{
	cliprdrContext* cliprdr = (cliprdrContext*) wfc->cliprdr_context;
	UINT32 uSize = 0;
	BYTE* pData = NULL;
	HRESULT	hRet = S_OK;
	FORMATETC vFormatEtc;
	LPDATAOBJECT pDataObj = NULL;
	STGMEDIUM vStgMedium;
	LPSTREAM pStream = NULL;
	BOOL bIsStreamFile = TRUE;
	static LPSTREAM	pStreamStc = NULL;
	static UINT32 uStreamIdStc = 0;

	pData = (BYTE*) calloc(1, event->cbRequested);
	
	if (!pData)
		goto error;

	hRet = OleGetClipboard(&pDataObj);

	if (!SUCCEEDED(hRet))
	{
		WLog_ERR(TAG,  "filecontents: get ole clipboard failed.");
		goto error;
	}
	
	ZeroMemory(&vFormatEtc, sizeof(FORMATETC));
	ZeroMemory(&vStgMedium, sizeof(STGMEDIUM));

	vFormatEtc.cfFormat = cliprdr->ID_FILECONTENTS;
	vFormatEtc.tymed = TYMED_ISTREAM;
	vFormatEtc.dwAspect = 1;
	vFormatEtc.lindex = event->lindex;
	vFormatEtc.ptd = NULL;

	if (uStreamIdStc != event->streamId || pStreamStc == NULL)
	{
		LPENUMFORMATETC pEnumFormatEtc;
		ULONG CeltFetched;

		FORMATETC vFormatEtc2;

		if (pStreamStc)
		{
			IStream_Release(pStreamStc);
			pStreamStc = NULL;
		}

		bIsStreamFile = FALSE;

		hRet = IDataObject_EnumFormatEtc(pDataObj, DATADIR_GET, &pEnumFormatEtc);

		if (hRet == S_OK)
		{
			do
			{
				hRet = IEnumFORMATETC_Next(pEnumFormatEtc, 1, &vFormatEtc2, &CeltFetched);

				if (hRet == S_OK)
				{
					if (vFormatEtc2.cfFormat == cliprdr->ID_FILECONTENTS)
					{
						hRet = IDataObject_GetData(pDataObj, &vFormatEtc, &vStgMedium);

						if (hRet == S_OK)
						{
							pStreamStc = vStgMedium.pstm;
							uStreamIdStc = event->streamId;
							bIsStreamFile = TRUE;
						}
						break;
					}
				}
			}
			while (hRet == S_OK);
		}
	}

	if (bIsStreamFile == TRUE)
	{
		if (event->dwFlags == 0x00000001)            /* FILECONTENTS_SIZE */
		{
			STATSTG vStatStg;

			ZeroMemory(&vStatStg, sizeof(STATSTG));

			hRet = IStream_Stat(pStreamStc, &vStatStg, STATFLAG_NONAME);

			if (hRet == S_OK)
			{
				Write_UINT32(pData, vStatStg.cbSize.LowPart);
				Write_UINT32(pData + 4, vStatStg.cbSize.HighPart);
				uSize = event->cbRequested;
			}
		}
		else if (event->dwFlags == 0x00000002)     /* FILECONTENTS_RANGE */
		{
			LARGE_INTEGER dlibMove;
			ULARGE_INTEGER dlibNewPosition;

			dlibMove.HighPart = event->nPositionHigh;
			dlibMove.LowPart = event->nPositionLow;

			hRet = IStream_Seek(pStreamStc, dlibMove, STREAM_SEEK_SET, &dlibNewPosition);

			if (SUCCEEDED(hRet))
			{
				hRet = IStream_Read(pStreamStc, pData, event->cbRequested, (PULONG)&uSize);
			}

		}
	}
	else // is local file
	{
		if (event->dwFlags == 0x00000001)            /* FILECONTENTS_SIZE */
		{
			Write_UINT32(pData, cliprdr->fileDescriptor[event->lindex]->nFileSizeLow);
			Write_UINT32(pData + 4, cliprdr->fileDescriptor[event->lindex]->nFileSizeHigh);
			uSize = event->cbRequested;
		}
		else if (event->dwFlags == 0x00000002)     /* FILECONTENTS_RANGE */
		{
			BOOL bRet;

			bRet = wf_cliprdr_get_file_contents(cliprdr->file_names[event->lindex], pData,
				event->nPositionLow, event->nPositionHigh, event->cbRequested, &uSize);

			if (bRet == FALSE)
			{
				WLog_ERR(TAG,  "get file contents failed.");
				uSize = 0;
				goto error;
			}
		}
	}

	IDataObject_Release(pDataObj);

	if (uSize == 0)
	{
		free(pData);
		pData = NULL;
	}

	cliprdr_send_response_filecontents(cliprdr, event->streamId, uSize, pData);

	return;

error:
	if (pData)
	{
		free(pData);
		pData = NULL;
	}

	if (pDataObj)
	{
		IDataObject_Release(pDataObj);
		pDataObj = NULL;
	}
	WLog_ERR(TAG,  "filecontents: send failed response.");
	cliprdr_send_response_filecontents(cliprdr, event->streamId, 0, NULL);
	return;
}

static void wf_cliprdr_process_cb_filecontents_response_event(wfContext* wfc, RDP_CB_FILECONTENTS_RESPONSE_EVENT* event)
{
	cliprdrContext* cliprdr = (cliprdrContext*) wfc->cliprdr_context;

	cliprdr->req_fsize = event->size;
	cliprdr->req_fdata = (char*) malloc(event->size);
	memcpy(cliprdr->req_fdata, event->data, event->size);

	SetEvent(cliprdr->req_fevent);
}

static void wf_cliprdr_process_cb_lock_clipdata_event(wfContext* wfc, RDP_CB_LOCK_CLIPDATA_EVENT* event)
{

}

static void wf_cliprdr_process_cb_unlock_clipdata_event(wfContext* wfc, RDP_CB_UNLOCK_CLIPDATA_EVENT* event)
{

}

void wf_process_cliprdr_event(wfContext* wfc, wMessage* event)
{
	switch (GetMessageType(event->id))
	{
		case CliprdrChannel_ClipCaps:
			wf_cliprdr_process_cb_clip_caps_event(wfc, (RDP_CB_CLIP_CAPS*) event);
			break;

		case CliprdrChannel_MonitorReady:
			wf_cliprdr_process_cb_monitor_ready_event(wfc, (RDP_CB_MONITOR_READY_EVENT*) event);
			break;

		case CliprdrChannel_FormatList:
			wf_cliprdr_process_cb_format_list_event(wfc, (RDP_CB_FORMAT_LIST_EVENT*) event);
			break;

		case CliprdrChannel_DataRequest:
			wf_cliprdr_process_cb_data_request_event(wfc, (RDP_CB_DATA_REQUEST_EVENT*) event);
			break;

		case CliprdrChannel_DataResponse:
			wf_cliprdr_process_cb_data_response_event(wfc, (RDP_CB_DATA_RESPONSE_EVENT*) event);
			break;

		case CliprdrChannel_FilecontentsRequest:
			wf_cliprdr_process_cb_filecontents_request_event(wfc, (RDP_CB_FILECONTENTS_REQUEST_EVENT*) event);
			break;

		case CliprdrChannel_FilecontentsResponse:
			wf_cliprdr_process_cb_filecontents_response_event(wfc, (RDP_CB_FILECONTENTS_RESPONSE_EVENT*) event);
			break;

		case CliprdrChannel_LockClipdata:
			wf_cliprdr_process_cb_lock_clipdata_event(wfc, (RDP_CB_LOCK_CLIPDATA_EVENT*) event);
			break;

		case CliprdrChannel_UnLockClipdata:
			wf_cliprdr_process_cb_unlock_clipdata_event(wfc, (RDP_CB_UNLOCK_CLIPDATA_EVENT*) event);
			break;

		default:
			break;
	}
}
