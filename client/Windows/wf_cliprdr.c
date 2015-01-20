/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Clipboard Redirection
 *
 * Copyright 2012 Jason Champion
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/tchar.h>
#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/client/cliprdr.h>

#include <Strsafe.h>

#include "wf_cliprdr.h"

#define TAG CLIENT_TAG("windows")

extern BOOL WINAPI AddClipboardFormatListener(_In_ HWND hwnd);
extern BOOL WINAPI RemoveClipboardFormatListener(_In_  HWND hwnd);

#define WM_CLIPRDR_MESSAGE  (WM_USER + 156)
#define OLE_SETCLIPBOARD    1

BOOL wf_create_file_obj(wfClipboard* cliprdrrdr, IDataObject** ppDataObject);
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

HRESULT STDMETHODCALLTYPE CliprdrStream_Read(IStream* This, void *pv, ULONG cb, ULONG *pcbRead)
{
	int ret;
	CliprdrStream* instance = (CliprdrStream*) This;
	wfClipboard* clipboard = (wfClipboard*) instance->m_pData;

	if (!pv || !pcbRead)
		return E_INVALIDARG;

	*pcbRead = 0;

	if (instance->m_lOffset.QuadPart >= instance->m_lSize.QuadPart)
		return S_FALSE;

	ret = cliprdr_send_request_filecontents(clipboard, (void*) This,
			instance->m_lIndex, FILECONTENTS_RANGE,
			instance->m_lOffset.HighPart, instance->m_lOffset.LowPart, cb);

	if (ret < 0)
		return S_FALSE;

	if (clipboard->req_fdata)
	{
		CopyMemory(pv, clipboard->req_fdata, clipboard->req_fsize);
		free(clipboard->req_fdata);
	}

	*pcbRead = clipboard->req_fsize;
	instance->m_lOffset.QuadPart += clipboard->req_fsize;

	if (clipboard->req_fsize < cb)
		return S_FALSE;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Write(IStream* This, const void* pv, ULONG cb, ULONG *pcbWritten)
{
	CliprdrStream* instance = (CliprdrStream*) This;

	return STG_E_ACCESSDENIED;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Seek(IStream* This, LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition)
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

CliprdrStream* CliprdrStream_New(LONG index, void* pData)
{
	IStream* iStream;
	CliprdrStream* instance;
	wfClipboard* clipboard = (wfClipboard*) pData;

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
			cliprdr_send_request_filecontents(clipboard, (void*) instance,
				instance->m_lIndex, FILECONTENTS_SIZE, 0, 0, 8);
			instance->m_lSize.QuadPart = *((LONGLONG*) clipboard->req_fdata);
			free(clipboard->req_fdata);
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
	wfClipboard* clipboard = (wfClipboard*) instance->m_pData;

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

	if (instance->m_pFormatEtc[idx].cfFormat == clipboard->ID_FILEDESCRIPTORW)
	{
		if (cliprdr_send_data_request(clipboard, instance->m_pFormatEtc[idx].cfFormat) != 0)
			return E_UNEXPECTED;

		pMedium->hGlobal = clipboard->hmem;   /* points to a FILEGROUPDESCRIPTOR structure */

		/* GlobalLock returns a pointer to the first byte of the memory block,
		* in which is a FILEGROUPDESCRIPTOR structure, whose first UINT member
		* is the number of FILEDESCRIPTOR's */
		instance->m_nStreams = *((PUINT) GlobalLock(clipboard->hmem));
		GlobalUnlock(clipboard->hmem);

		if (instance->m_nStreams > 0)
		{
			if (!instance->m_pStream)
			{
				instance->m_pStream = (LPSTREAM*) calloc(instance->m_nStreams, sizeof(LPSTREAM));

				if (instance->m_pStream)
				{
					for (i = 0; i < instance->m_nStreams; i++)
					{
						instance->m_pStream[i] = (IStream*) CliprdrStream_New(i, clipboard);
					}
				}
			}
		}

		if (!instance->m_pStream)
		{
			if (clipboard->hmem)
			{
				GlobalFree(clipboard->hmem);
				clipboard->hmem = NULL;
			}

			pMedium->hGlobal = NULL;

			return E_OUTOFMEMORY;
		}
	}
	else if (instance->m_pFormatEtc[idx].cfFormat == clipboard->ID_FILECONTENTS)
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
	else if (instance->m_pFormatEtc[idx].cfFormat == clipboard->ID_PREFERREDDROPEFFECT)
	{
		if (cliprdr_send_data_request(clipboard, instance->m_pFormatEtc[idx].cfFormat) != 0)
			return E_UNEXPECTED;

		pMedium->hGlobal = clipboard->hmem;
	}
	else
	{
		return E_UNEXPECTED;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetDataHere(IDataObject* This, FORMATETC* pformatetc, STGMEDIUM* pmedium)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	return DATA_E_FORMATETC;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_QueryGetData(IDataObject* This, FORMATETC* pformatetc)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	if (!pformatetc)
		return E_INVALIDARG;

	return (cliprdr_lookup_format(instance, pformatetc) == -1) ? DV_E_FORMATETC : S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetCanonicalFormatEtc(IDataObject* This, FORMATETC* pformatectIn, FORMATETC* pformatetcOut)
{
	CliprdrDataObject* instance = (CliprdrDataObject*) This;

	if (!pformatetcOut)
		return E_INVALIDARG;

	pformatetcOut->ptd = NULL;

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_SetData(IDataObject* This, FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease)
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

HRESULT STDMETHODCALLTYPE CliprdrDataObject_DAdvise(IDataObject* This, FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection)
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

BOOL wf_create_file_obj(wfClipboard* clipboard, IDataObject** ppDataObject)
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

	*ppDataObject = (IDataObject*) CliprdrDataObject_New(fmtetc, stgmeds, 3, clipboard);

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

HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Next(IEnumFORMATETC* This, ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched)
{
	ULONG copied  = 0;
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*) This;

	if (!celt || !rgelt)
		return E_INVALIDARG;

	while ((instance->m_nIndex < instance->m_nNumFormats) && (copied < celt))
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

static UINT32 get_local_format_id_by_name(wfClipboard* clipboard, void* format_name)
{
	int i;
	formatMapping* map;

	for (i = 0; i < clipboard->map_size; i++)
	{
		map = &clipboard->format_mappings[i];

		if ((clipboard->capabilities & CB_USE_LONG_FORMAT_NAMES) != 0)
		{
			if (map->name)
			{
				if (memcmp(map->name, format_name, wcslen((LPCWSTR) format_name)) == 0)
					return map->local_format_id;
			}
		}
	}

	return 0;
}

static INLINE BOOL file_transferring(wfClipboard* clipboard)
{
	return get_local_format_id_by_name(clipboard, _T("FileGroupDescriptorW")) ? TRUE : FALSE;
}

static UINT32 get_remote_format_id(wfClipboard* clipboard, UINT32 local_format)
{
	int i;
	formatMapping* map;

	for (i = 0; i < clipboard->map_size; i++)
	{
		map = &clipboard->format_mappings[i];

		if (map->local_format_id == local_format)
		{
			return map->remote_format_id;
		}
	}

	return local_format;
}

static void map_ensure_capacity(wfClipboard* clipboard)
{
	if (clipboard->map_size >= clipboard->map_capacity)
	{
		clipboard->map_capacity *= 2;
		clipboard->format_mappings = (formatMapping*) realloc(clipboard->format_mappings,
			sizeof(formatMapping) * clipboard->map_capacity);
	}
}

static void clear_format_map(wfClipboard* clipboard)
{
	int i;
	formatMapping* map;

	if (clipboard->format_mappings)
	{
		for (i = 0; i < clipboard->map_capacity; i++)
		{
			map = &clipboard->format_mappings[i];
			map->remote_format_id = 0;
			map->local_format_id = 0;

			if (map->name)
			{
				free(map->name);
				map->name = NULL;
			}
		}
	}

	clipboard->map_size = 0;
}

int cliprdr_send_tempdir(wfClipboard* clipboard)
{
	CLIPRDR_TEMP_DIRECTORY tempDirectory;

	GetEnvironmentVariableA("TEMP", tempDirectory.szTempDir, sizeof(tempDirectory.szTempDir));

	clipboard->context->TempDirectory(clipboard->context, &tempDirectory);

	return 1;
}

static int cliprdr_send_format_list(wfClipboard* clipboard)
{
	int count;
	int length;
	UINT32 index;
	UINT32 numFormats;
	UINT32 formatId = 0;
	char formatName[1024];
	CLIPRDR_FORMAT* format;
	CLIPRDR_FORMAT* formats;
	CLIPRDR_FORMAT_LIST formatList;

	ZeroMemory(&formatList, sizeof(CLIPRDR_FORMAT_LIST));

	if (!OpenClipboard(clipboard->hwnd))
		return -1;

	count = CountClipboardFormats();

	numFormats = (UINT32) count;
	formats = (CLIPRDR_FORMAT*) calloc(numFormats, sizeof(CLIPRDR_FORMAT));

	index = 0;

	while (formatId = EnumClipboardFormats(formatId))
	{
		format = &formats[index++];

		format->formatId = formatId;

		length = 0;
		format->formatName = NULL;

		if (formatId >= CF_MAX)
		{
			length = GetClipboardFormatNameA(formatId, formatName, sizeof(formatName) - 1);
		}

		if (length > 0)
		{
			format->formatName = _strdup(formatName);
		}
	}

	CloseClipboard();

	formatList.msgFlags = 0;
	formatList.numFormats = numFormats;
	formatList.formats = formats;

	clipboard->context->ClientFormatList(clipboard->context, &formatList);

	for (index = 0; index < numFormats; index++)
	{
		format = &formats[index];
		free(format->formatName);
	}

	free(formats);

	return 1;
}

int cliprdr_send_data_request(wfClipboard* clipboard, UINT32 formatId)
{
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;

	formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.msgFlags = CB_RESPONSE_OK;

	formatDataRequest.requestedFormatId = formatId;
	clipboard->requestedFormatId = formatId;

	clipboard->context->ClientFormatDataRequest(clipboard->context, &formatDataRequest);

	WaitForSingleObject(clipboard->response_data_event, INFINITE);
	ResetEvent(clipboard->response_data_event);

	return 0;
}

int cliprdr_send_request_filecontents(wfClipboard* clipboard, void* streamid,
		int index, int flag, DWORD positionhigh, DWORD positionlow, ULONG nreq)
{
	CLIPRDR_FILE_CONTENTS_REQUEST fileContentsRequest;

	ZeroMemory(&fileContentsRequest, sizeof(CLIPRDR_FILE_CONTENTS_REQUEST));

	fileContentsRequest.streamId = (UINT32) streamid;
	fileContentsRequest.listIndex = index;
	fileContentsRequest.dwFlags = flag;
	fileContentsRequest.nPositionLow = positionlow;
	fileContentsRequest.nPositionHigh = positionhigh;
	fileContentsRequest.cbRequested = nreq;
	fileContentsRequest.clipDataId = 0;

	clipboard->context->ClientFileContentsRequest(clipboard->context, &fileContentsRequest);

	WaitForSingleObject(clipboard->req_fevent, INFINITE);
	ResetEvent(clipboard->req_fevent);

	return 0;
}

int cliprdr_send_response_filecontents(wfClipboard* clipboard, UINT32 streamId, UINT32 size, BYTE* data)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE fileContentsResponse;

	ZeroMemory(&fileContentsResponse, sizeof(CLIPRDR_FILE_CONTENTS_RESPONSE));

	fileContentsResponse.streamId = streamId;
	fileContentsResponse.cbRequested = size;
	fileContentsResponse.requestedData = data;
	
	clipboard->context->ClientFileContentsResponse(clipboard->context, &fileContentsResponse);

	return 0;
}

static LRESULT CALLBACK cliprdr_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static wfClipboard* clipboard = NULL;

	switch (Msg)
	{
		case WM_CREATE:
			DEBUG_CLIPRDR("info: WM_CREATE");
			clipboard = (wfClipboard*)((CREATESTRUCT*) lParam)->lpCreateParams;
			if (!AddClipboardFormatListener(hWnd)) {
				DEBUG_CLIPRDR("error: AddClipboardFormatListener failed with %#x.", GetLastError());
			}
			clipboard->hwnd = hWnd;
			break;

		case WM_CLOSE:
			DEBUG_CLIPRDR("info: WM_CLOSE");
			RemoveClipboardFormatListener(hWnd);
			break;

		case WM_CLIPBOARDUPDATE:
			DEBUG_CLIPRDR("info: WM_CLIPBOARDUPDATE");
			if (clipboard->sync)
			{
				if ((GetClipboardOwner() != clipboard->hwnd) &&
					(S_FALSE == OleIsCurrentClipboard(clipboard->data_obj)))
				{
						if (clipboard->hmem)
						{
							GlobalFree(clipboard->hmem);
							clipboard->hmem = NULL;
						}

						cliprdr_send_format_list(clipboard);
				}
			}
			break;

		case WM_RENDERALLFORMATS:
			DEBUG_CLIPRDR("info: WM_RENDERALLFORMATS");
			/* discard all contexts in clipboard */
			if (!OpenClipboard(clipboard->hwnd))
			{
				DEBUG_CLIPRDR("OpenClipboard failed with 0x%x", GetLastError());
				break;
			}
			EmptyClipboard();
			CloseClipboard();
			break;

		case WM_RENDERFORMAT:
			DEBUG_CLIPRDR("info: WM_RENDERFORMAT");
			if (cliprdr_send_data_request(clipboard, (UINT32) wParam) != 0)
			{
				DEBUG_CLIPRDR("error: cliprdr_send_data_request failed.");
				break;
			}

			if (!SetClipboardData((UINT) wParam, clipboard->hmem))
			{
				DEBUG_CLIPRDR("SetClipboardData failed with 0x%x", GetLastError());

				if (clipboard->hmem)
				{
					GlobalFree(clipboard->hmem);
					clipboard->hmem = NULL;
				}
			}
			/* Note: GlobalFree() is not needed when success */
			break;

		case WM_CLIPRDR_MESSAGE:
			DEBUG_CLIPRDR("info: WM_CLIPRDR_MESSAGE");
			switch (wParam)
			{
				case OLE_SETCLIPBOARD:
					DEBUG_CLIPRDR("info: OLE_SETCLIPBOARD");
					if (wf_create_file_obj(clipboard, &clipboard->data_obj))
					{
						if (OleSetClipboard(clipboard->data_obj) != S_OK)
						{
							wf_destroy_file_obj(clipboard->data_obj);
							clipboard->data_obj = NULL;
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

static int create_cliprdr_window(wfClipboard* clipboard)
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
	wnd_cls.lpszClassName = _T("ClipboardHiddenMessageProcessor");
	wnd_cls.hInstance = GetModuleHandle(NULL);
	wnd_cls.hIconSm = NULL;

	RegisterClassEx(&wnd_cls);

	clipboard->hwnd = CreateWindowEx(WS_EX_LEFT,
		_T("ClipboardHiddenMessageProcessor"),
		_T("rdpclip"),
		0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), clipboard);

	if (!clipboard->hwnd)
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
	wfClipboard* clipboard = (wfClipboard*) arg;

	OleInitialize(0);

	if ((ret = create_cliprdr_window(clipboard)) != 0)
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

static void clear_file_array(wfClipboard* clipboard)
{
	int i;

	/* clear file_names array */
	if (clipboard->file_names)
	{
		for (i = 0; i < clipboard->nFiles; i++)
		{
			if (clipboard->file_names[i])
			{
				free(clipboard->file_names[i]);
				clipboard->file_names[i] = NULL;
			}
		}
	}

	/* clear fileDescriptor array */
	if (clipboard->fileDescriptor)
	{
		for (i = 0; i < clipboard->nFiles; i++)
		{
			if (clipboard->fileDescriptor[i])
			{
				free(clipboard->fileDescriptor[i]);
				clipboard->fileDescriptor[i] = NULL;
			}
		}
	}

	clipboard->nFiles = 0;
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

static void wf_cliprdr_array_ensure_capacity(wfClipboard* clipboard)
{
	if (clipboard->nFiles == clipboard->file_array_size)
	{
		clipboard->file_array_size *= 2;
		clipboard->fileDescriptor = (FILEDESCRIPTORW**) realloc(clipboard->fileDescriptor, clipboard->file_array_size * sizeof(FILEDESCRIPTORW*));
		clipboard->file_names = (WCHAR**) realloc(clipboard->file_names, clipboard->file_array_size * sizeof(WCHAR*));
	}
}

static void wf_cliprdr_add_to_file_arrays(wfClipboard* clipboard, WCHAR* full_file_name, int pathLen)
{
	/* add to name array */
	clipboard->file_names[clipboard->nFiles] = (LPWSTR) malloc(MAX_PATH * 2);
	wcscpy_s(clipboard->file_names[clipboard->nFiles], MAX_PATH, full_file_name);

	/* add to descriptor array */
	clipboard->fileDescriptor[clipboard->nFiles] = wf_cliprdr_get_file_descriptor(full_file_name, pathLen);

	clipboard->nFiles++;

	wf_cliprdr_array_ensure_capacity(clipboard);
}

static void wf_cliprdr_traverse_directory(wfClipboard* clipboard, WCHAR* Dir, int pathLen)
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
			&& wcscmp(FindFileData.cFileName, _T(".")) == 0
			|| wcscmp(FindFileData.cFileName, _T("..")) == 0)
		{
			continue;
		}

		if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) !=0)
		{
			WCHAR DirAdd[MAX_PATH];

			StringCchCopy(DirAdd, MAX_PATH, Dir);
			StringCchCat(DirAdd, MAX_PATH, _T("\\"));
			StringCchCat(DirAdd, MAX_PATH, FindFileData.cFileName);
			wf_cliprdr_add_to_file_arrays(clipboard, DirAdd, pathLen);
			wf_cliprdr_traverse_directory(clipboard, DirAdd, pathLen);
		}
		else
		{
			WCHAR fileName[MAX_PATH];

			StringCchCopy(fileName, MAX_PATH, Dir);
			StringCchCat(fileName, MAX_PATH, _T("\\"));
			StringCchCat(fileName, MAX_PATH, FindFileData.cFileName);

			wf_cliprdr_add_to_file_arrays(clipboard, fileName, pathLen);
		}
	}

	FindClose(hFind);
}

int wf_cliprdr_send_client_capabilities(wfClipboard* clipboard)
{
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*) &(generalCapabilitySet);

	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;

	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;

	clipboard->context->ClientCapabilities(clipboard->context, &capabilities);

	return 1;
}

static int wf_cliprdr_monitor_ready(CliprdrClientContext* context, CLIPRDR_MONITOR_READY* monitorReady)
{
	wfClipboard* clipboard = (wfClipboard*) context->custom;

	clipboard->sync = TRUE;
	wf_cliprdr_send_client_capabilities(clipboard);
	cliprdr_send_format_list(clipboard);

	return 1;
}

static int wf_cliprdr_server_capabilities(CliprdrClientContext* context, CLIPRDR_CAPABILITIES* capabilities)
{
	UINT32 index;
	CLIPRDR_CAPABILITY_SET* capabilitySet;
	wfClipboard* clipboard = (wfClipboard*) context->custom;

	for (index = 0; index < capabilities->cCapabilitiesSets; index++)
	{
		capabilitySet = &(capabilities->capabilitySets[index]);

		if ((capabilitySet->capabilitySetType == CB_CAPSTYPE_GENERAL) &&
			(capabilitySet->capabilitySetLength >= CB_CAPSTYPE_GENERAL_LEN))
		{
			CLIPRDR_GENERAL_CAPABILITY_SET* generalCapabilitySet
				= (CLIPRDR_GENERAL_CAPABILITY_SET*) capabilitySet;

			clipboard->capabilities = generalCapabilitySet->generalFlags;
			break;
		}
	}
	
	return 1;
}

static int wf_cliprdr_server_format_list(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST* formatList)
{
	UINT32 i, j;
	formatMapping* mapping;
	CLIPRDR_FORMAT* format;
	wfClipboard* clipboard = (wfClipboard*) context->custom;

	clear_format_map(clipboard);

	for (i = j = 0; i < formatList->numFormats; i++)
	{
		format = &(formatList->formats[i]);
		mapping = &(clipboard->format_mappings[j++]);

		mapping->remote_format_id = format->formatId;

		if (format->formatName)
		{
			mapping->name = _strdup(format->formatName);
			mapping->local_format_id = RegisterClipboardFormatA((LPCSTR) mapping->name);
		}
		else
		{
			mapping->name = NULL;
			mapping->local_format_id = mapping->remote_format_id;
		}

		clipboard->map_size++;
		map_ensure_capacity(clipboard);
	}

	if (file_transferring(clipboard))
	{
		PostMessage(clipboard->hwnd, WM_CLIPRDR_MESSAGE, OLE_SETCLIPBOARD, 0);
	}
	else
	{
		if (!OpenClipboard(clipboard->hwnd))
			return -1;

		if (EmptyClipboard())
		{
			for (i = 0; i < (UINT32) clipboard->map_size; i++)
			{
				SetClipboardData(clipboard->format_mappings[i].local_format_id, NULL);
			}
		}

		CloseClipboard();
	}

	return 1;
}

static int wf_cliprdr_server_format_list_response(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	wfClipboard* clipboard = (wfClipboard*) context->custom;
	return 1;
}

int wf_cliprdr_server_lock_clipboard_data(CliprdrClientContext* context, CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	wfClipboard* clipboard = (wfClipboard*) context->custom;
	return 1;
}

int wf_cliprdr_server_unlock_clipboard_data(CliprdrClientContext* context, CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	wfClipboard* clipboard = (wfClipboard*) context->custom;
	return 1;
}

static int wf_cliprdr_server_format_data_request(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	int size = 0;
	char* buff = NULL;
	char* globlemem = NULL;
	HANDLE hClipdata = NULL;
	UINT32 requestedFormatId;
	CLIPRDR_FORMAT_DATA_RESPONSE response;
	wfClipboard* clipboard = (wfClipboard*) context->custom;

	requestedFormatId = formatDataRequest->requestedFormatId;

	if (requestedFormatId == RegisterClipboardFormatW(_T("FileGroupDescriptorW")))
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

		result = OleGetClipboard(&dataObj);

		if (!SUCCEEDED(result))
			return -1;

		ZeroMemory(&format_etc, sizeof(FORMATETC));
		ZeroMemory(&stg_medium, sizeof(STGMEDIUM));

		/* try to get FileGroupDescriptorW struct from OLE */
		format_etc.cfFormat = requestedFormatId;
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

			clear_file_array(clipboard);
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
				clipboard->nFiles = 0;

				goto exit;
			}
			uSize = GlobalSize(stg_medium.hGlobal);

			dropFiles = (DROPFILES*) malloc(uSize);
			memcpy(dropFiles, globlemem, uSize);

			GlobalUnlock(stg_medium.hGlobal);

			ReleaseStgMedium(&stg_medium);

			clear_file_array(clipboard);

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

					wf_cliprdr_add_to_file_arrays(clipboard, wFileName, pathLen);

					if ((clipboard->fileDescriptor[clipboard->nFiles - 1]->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
					{
						/* this is a directory */
						wf_cliprdr_traverse_directory(clipboard, wFileName, pathLen);
					}
				}
			}
			else
			{
				char* p;

				for (p = (char*)((char*)dropFiles + dropFiles->pFiles); (len = strlen(p)) > 0; p += len + 1, clipboard->nFiles++)
				{
					int cchWideChar;

					cchWideChar = MultiByteToWideChar(CP_ACP, MB_COMPOSITE, p, len, NULL, 0);
					clipboard->file_names[clipboard->nFiles] = (LPWSTR) malloc(cchWideChar * 2);
					MultiByteToWideChar(CP_ACP, MB_COMPOSITE, p, len, clipboard->file_names[clipboard->nFiles], cchWideChar);

					if (clipboard->nFiles == clipboard->file_array_size)
					{
						clipboard->file_array_size *= 2;
						clipboard->fileDescriptor = (FILEDESCRIPTORW**) realloc(clipboard->fileDescriptor, clipboard->file_array_size * sizeof(FILEDESCRIPTORW*));
						clipboard->file_names = (WCHAR**) realloc(clipboard->file_names, clipboard->file_array_size * sizeof(WCHAR*));
					}
				}
			}

exit:
			size = 4 + clipboard->nFiles * sizeof(FILEDESCRIPTORW);
			buff = (char*) malloc(size);

			*((UINT32*) buff) = clipboard->nFiles;

			for (i = 0; i < clipboard->nFiles; i++)
			{
				if (clipboard->fileDescriptor[i])
				{
					memcpy(buff + 4 + i * sizeof(FILEDESCRIPTORW), clipboard->fileDescriptor[i], sizeof(FILEDESCRIPTORW));
				}
			}
		}

		IDataObject_Release(dataObj);
	}
	else
	{
		if (!OpenClipboard(clipboard->hwnd))
			return -1;

		hClipdata = GetClipboardData(requestedFormatId);

		if (!hClipdata)
		{
			CloseClipboard();
			return -1;
		}

		globlemem = (char*) GlobalLock(hClipdata);
		size = (int) GlobalSize(hClipdata);

		buff = (char*) malloc(size);
		CopyMemory(buff, globlemem, size);

		GlobalUnlock(hClipdata);

		CloseClipboard();
	}

	ZeroMemory(&response, sizeof(CLIPRDR_FORMAT_DATA_RESPONSE));

	response.msgFlags = CB_RESPONSE_OK;
	response.dataLen = size;
	response.requestedFormatData = (BYTE*) buff;

	clipboard->context->ClientFormatDataResponse(clipboard->context, &response);

	free(buff);

	return 1;
}

static int wf_cliprdr_server_format_data_response(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	BYTE* data;
	HANDLE hMem;
	wfClipboard* clipboard = (wfClipboard*) context->custom;

	hMem = GlobalAlloc(GMEM_FIXED, formatDataResponse->dataLen);
	data = (BYTE*) GlobalLock(hMem);
	CopyMemory(data, formatDataResponse->requestedFormatData, formatDataResponse->dataLen);
	GlobalUnlock(hMem);

	clipboard->hmem = hMem;
	SetEvent(clipboard->response_data_event);

	return 1;
}

int wf_cliprdr_server_file_contents_request(CliprdrClientContext* context, CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
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
	wfClipboard* clipboard = (wfClipboard*) context->custom;

	if (fileContentsRequest->dwFlags == FILECONTENTS_SIZE)
		fileContentsRequest->cbRequested = sizeof(UINT64);

	pData = (BYTE*) calloc(1, fileContentsRequest->cbRequested);
	
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

	vFormatEtc.cfFormat = clipboard->ID_FILECONTENTS;
	vFormatEtc.tymed = TYMED_ISTREAM;
	vFormatEtc.dwAspect = 1;
	vFormatEtc.lindex = fileContentsRequest->listIndex;
	vFormatEtc.ptd = NULL;

	if ((uStreamIdStc != fileContentsRequest->streamId) || !pStreamStc)
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
					if (vFormatEtc2.cfFormat == clipboard->ID_FILECONTENTS)
					{
						hRet = IDataObject_GetData(pDataObj, &vFormatEtc, &vStgMedium);

						if (hRet == S_OK)
						{
							pStreamStc = vStgMedium.pstm;
							uStreamIdStc = fileContentsRequest->streamId;
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
		if (fileContentsRequest->dwFlags == FILECONTENTS_SIZE)
		{
			STATSTG vStatStg;

			ZeroMemory(&vStatStg, sizeof(STATSTG));

			hRet = IStream_Stat(pStreamStc, &vStatStg, STATFLAG_NONAME);

			if (hRet == S_OK)
			{
				*((UINT32*) &pData[0]) = vStatStg.cbSize.LowPart;
				*((UINT32*) &pData[4]) = vStatStg.cbSize.HighPart;
				uSize = fileContentsRequest->cbRequested;
			}
		}
		else if (fileContentsRequest->dwFlags == FILECONTENTS_RANGE)
		{
			LARGE_INTEGER dlibMove;
			ULARGE_INTEGER dlibNewPosition;

			dlibMove.HighPart = fileContentsRequest->nPositionHigh;
			dlibMove.LowPart = fileContentsRequest->nPositionLow;

			hRet = IStream_Seek(pStreamStc, dlibMove, STREAM_SEEK_SET, &dlibNewPosition);

			if (SUCCEEDED(hRet))
			{
				hRet = IStream_Read(pStreamStc, pData, fileContentsRequest->cbRequested, (PULONG) &uSize);
			}

		}
	}
	else
	{
		if (fileContentsRequest->dwFlags == FILECONTENTS_SIZE)
		{
			*((UINT32*) &pData[0]) = clipboard->fileDescriptor[fileContentsRequest->listIndex]->nFileSizeLow;
			*((UINT32*) &pData[4]) = clipboard->fileDescriptor[fileContentsRequest->listIndex]->nFileSizeHigh;
			uSize = fileContentsRequest->cbRequested;
		}
		else if (fileContentsRequest->dwFlags == FILECONTENTS_RANGE)
		{
			BOOL bRet;

			bRet = wf_cliprdr_get_file_contents(clipboard->file_names[fileContentsRequest->listIndex], pData,
				fileContentsRequest->nPositionLow, fileContentsRequest->nPositionHigh,
				fileContentsRequest->cbRequested, &uSize);

			if (bRet == FALSE)
			{
				WLog_ERR(TAG, "get file contents failed.");
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

	cliprdr_send_response_filecontents(clipboard, fileContentsRequest->streamId, uSize, pData);

	free(pData);

	return 1;

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
	cliprdr_send_response_filecontents(clipboard, fileContentsRequest->streamId, 0, NULL);

	return -1;
}

int wf_cliprdr_server_file_contents_response(CliprdrClientContext* context, CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse)
{
	wfClipboard* clipboard = (wfClipboard*) context->custom;

	clipboard->req_fsize = fileContentsResponse->cbRequested;
	clipboard->req_fdata = (char*) malloc(fileContentsResponse->cbRequested);
	CopyMemory(clipboard->req_fdata, fileContentsResponse->requestedData, fileContentsResponse->cbRequested);

	SetEvent(clipboard->req_fevent);

	return 1;
}

void wf_cliprdr_init(wfContext* wfc, CliprdrClientContext* cliprdr)
{
	wfClipboard* clipboard;
	rdpContext* context = (rdpContext*) wfc;

	wfc->clipboard = (wfClipboard*) calloc(1, sizeof(wfClipboard));

	if (!wfc->clipboard)
		return;

	clipboard = wfc->clipboard;
	clipboard->wfc = wfc;
	clipboard->context = cliprdr;

	cliprdr->custom = (void*) wfc->clipboard;
	clipboard->context = cliprdr;
		
	cliprdr->MonitorReady = wf_cliprdr_monitor_ready;
	cliprdr->ServerCapabilities = wf_cliprdr_server_capabilities;
	cliprdr->ServerFormatList = wf_cliprdr_server_format_list;
	cliprdr->ServerFormatListResponse = wf_cliprdr_server_format_list_response;
	cliprdr->ServerLockClipboardData = wf_cliprdr_server_lock_clipboard_data;
	cliprdr->ServerUnlockClipboardData = wf_cliprdr_server_unlock_clipboard_data;
	cliprdr->ServerFormatDataRequest = wf_cliprdr_server_format_data_request;
	cliprdr->ServerFormatDataResponse = wf_cliprdr_server_format_data_response;
	cliprdr->ServerFileContentsRequest = wf_cliprdr_server_file_contents_request;
	cliprdr->ServerFileContentsResponse = wf_cliprdr_server_file_contents_response;

	clipboard->channels = context->channels;
	clipboard->sync = FALSE;

	clipboard->map_capacity = 32;
	clipboard->map_size = 0;

	clipboard->format_mappings = (formatMapping*) calloc(1, sizeof(formatMapping) * clipboard->map_capacity);

	clipboard->file_array_size = 32;
	clipboard->file_names = (WCHAR**) calloc(1, clipboard->file_array_size * sizeof(WCHAR*));
	clipboard->fileDescriptor = (FILEDESCRIPTORW**) calloc(1, clipboard->file_array_size * sizeof(FILEDESCRIPTORW*));

	clipboard->response_data_event = CreateEvent(NULL, TRUE, FALSE, _T("response_data_event"));

	clipboard->req_fevent = CreateEvent(NULL, TRUE, FALSE, _T("request_filecontents_event"));
	clipboard->ID_FILEDESCRIPTORW = RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW);
	clipboard->ID_FILECONTENTS = RegisterClipboardFormatW(CFSTR_FILECONTENTS);
	clipboard->ID_PREFERREDDROPEFFECT = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);

	clipboard->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) cliprdr_thread_func, clipboard, 0, NULL);
}

void wf_cliprdr_uninit(wfContext* wfc, CliprdrClientContext* cliprdr)
{
	wfClipboard* clipboard = wfc->clipboard;

	cliprdr->custom = NULL;

	if (!clipboard)
		return;

	if (clipboard->hwnd)
		PostMessage(clipboard->hwnd, WM_QUIT, 0, 0);

	if (clipboard->thread)
	{
		WaitForSingleObject(clipboard->thread, INFINITE);
		CloseHandle(clipboard->thread);
		clipboard->thread = NULL;
	}

	if (clipboard->response_data_event)
	{
		CloseHandle(clipboard->response_data_event);
		clipboard->response_data_event = NULL;
	}

	clear_file_array(clipboard);
	clear_format_map(clipboard);

	if (clipboard->file_names)
		free(clipboard->file_names);
	if (clipboard->fileDescriptor)
		free(clipboard->fileDescriptor);
	if (clipboard->format_mappings)
		free(clipboard->format_mappings);

	free(clipboard);
}
