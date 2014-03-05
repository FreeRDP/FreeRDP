/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Implementation of the IDataObject COM interface
 *
 * Copyright 2014 Zhang Zhaolong <zhangzl2013@126.com>
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

#include "wf_cliprdr.h"
#include "wf_cliprdr_Stream.h"
#include "wf_cliprdr_DataObject.h"
#include "wf_cliprdr_EnumFORMATETC.h"

static int cliprdr_lookup_format(CliprdrDataObject *instance, FORMATETC *pFormatEtc)
{
	int i;
	for (i = 0; i < instance->m_nNumFormats; i++)
	{
		if((pFormatEtc->tymed & instance->m_pFormatEtc[i].tymed) &&
			pFormatEtc->cfFormat == instance->m_pFormatEtc[i].cfFormat &&
			pFormatEtc->dwAspect == instance->m_pFormatEtc[i].dwAspect)
		{
			return i;
		}
	}

	return -1;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_QueryInterface(IDataObject *This, REFIID riid, void **ppvObject)
{
	CliprdrDataObject *instance = (CliprdrDataObject *)This;

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

ULONG STDMETHODCALLTYPE CliprdrDataObject_AddRef(IDataObject *This)
{
	CliprdrDataObject *instance = (CliprdrDataObject *)This;

	return InterlockedIncrement(&instance->m_lRefCount);
}

ULONG STDMETHODCALLTYPE CliprdrDataObject_Release(IDataObject *This)
{
	CliprdrDataObject *instance = (CliprdrDataObject *)This;
	LONG count;

	count = InterlockedDecrement(&instance->m_lRefCount);

	if(count == 0)
	{
		CliprdrDataObject_Delete(instance);
		return 0;
	}
	else
	{
		return count;
	}
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetData(IDataObject *This, FORMATETC *pFormatEtc, STGMEDIUM *pMedium)
{
	CliprdrDataObject *instance = (CliprdrDataObject *)This;
	cliprdrContext *cliprdr = (cliprdrContext *)instance->m_pData;
	int idx;
	int i;

	if (pFormatEtc == NULL || pMedium == NULL)
	{
		return E_INVALIDARG;
	}

	if((idx = cliprdr_lookup_format(instance, pFormatEtc)) == -1)
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
				instance->m_pStream = (LPSTREAM *)calloc(instance->m_nStreams, sizeof(LPSTREAM));
				if (instance->m_pStream)
					for(i = 0; i < instance->m_nStreams; i++)
						instance->m_pStream[i] = (IStream *)CliprdrStream_New(i, cliprdr);
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

HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetDataHere(IDataObject *This, FORMATETC *pformatetc, STGMEDIUM *pmedium)
{
	CliprdrDataObject *instance = (CliprdrDataObject *)This;

	return DATA_E_FORMATETC;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_QueryGetData(IDataObject *This, FORMATETC *pformatetc)
{
	CliprdrDataObject *instance = (CliprdrDataObject *)This;

	if (!pformatetc)
		return E_INVALIDARG;

	return (cliprdr_lookup_format(instance, pformatetc) == -1) ? DV_E_FORMATETC : S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetCanonicalFormatEtc(IDataObject *This, FORMATETC *pformatectIn, FORMATETC *pformatetcOut)
{
	CliprdrDataObject *instance = (CliprdrDataObject *)This;

	if (!pformatetcOut)
		return E_INVALIDARG;

	pformatetcOut->ptd = NULL;

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_SetData(IDataObject *This, FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease)
{
	CliprdrDataObject *instance = (CliprdrDataObject *)This;

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_EnumFormatEtc(IDataObject *This, DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc)
{
	CliprdrDataObject *instance = (CliprdrDataObject *)This;

	if (!ppenumFormatEtc)
		return E_INVALIDARG;

	if(dwDirection == DATADIR_GET)
	{
		*ppenumFormatEtc = (IEnumFORMATETC *)CliprdrEnumFORMATETC_New(instance->m_nNumFormats, instance->m_pFormatEtc);
		return (*ppenumFormatEtc) ? S_OK : E_OUTOFMEMORY;
	}
	else
	{
		return E_NOTIMPL;
	}
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_DAdvise(IDataObject *This, FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection)
{
	CliprdrDataObject *instance = (CliprdrDataObject *)This;

	return OLE_E_ADVISENOTSUPPORTED;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_DUnadvise(IDataObject *This, DWORD dwConnection)
{
	CliprdrDataObject *instance = (CliprdrDataObject *)This;

	return OLE_E_ADVISENOTSUPPORTED;
}

HRESULT STDMETHODCALLTYPE CliprdrDataObject_EnumDAdvise(IDataObject *This, IEnumSTATDATA **ppenumAdvise)
{
	CliprdrDataObject *instance = (CliprdrDataObject *)This;

	return OLE_E_ADVISENOTSUPPORTED;
}

CliprdrDataObject *CliprdrDataObject_New(FORMATETC *fmtetc, STGMEDIUM *stgmed, int count, void *data)
{
	CliprdrDataObject *instance;
	IDataObject *iDataObject;
	int i;

	instance = (CliprdrDataObject *)calloc(1, sizeof(CliprdrDataObject));

	if (instance)
	{
		iDataObject = &instance->iDataObject;

		iDataObject->lpVtbl = (IDataObjectVtbl *)calloc(1, sizeof(IDataObjectVtbl));
		if (iDataObject->lpVtbl)
		{
			iDataObject->lpVtbl->QueryInterface        = CliprdrDataObject_QueryInterface;
			iDataObject->lpVtbl->AddRef                = CliprdrDataObject_AddRef;
			iDataObject->lpVtbl->Release               = CliprdrDataObject_Release;
			iDataObject->lpVtbl->GetData               = CliprdrDataObject_GetData;
			iDataObject->lpVtbl->GetDataHere           = CliprdrDataObject_GetDataHere;
			iDataObject->lpVtbl->QueryGetData          = CliprdrDataObject_QueryGetData;
			iDataObject->lpVtbl->GetCanonicalFormatEtc = CliprdrDataObject_GetCanonicalFormatEtc;
			iDataObject->lpVtbl->SetData               = CliprdrDataObject_SetData;
			iDataObject->lpVtbl->EnumFormatEtc         = CliprdrDataObject_EnumFormatEtc;
			iDataObject->lpVtbl->DAdvise               = CliprdrDataObject_DAdvise;
			iDataObject->lpVtbl->DUnadvise             = CliprdrDataObject_DUnadvise;
			iDataObject->lpVtbl->EnumDAdvise           = CliprdrDataObject_EnumDAdvise;

			instance->m_lRefCount   = 1;
			instance->m_nNumFormats = count;
			instance->m_pData       = data;
			instance->m_nStreams    = 0;
			instance->m_pStream     = NULL;

			instance->m_pFormatEtc  = (FORMATETC *)calloc(count, sizeof(FORMATETC));
			instance->m_pStgMedium  = (STGMEDIUM *)calloc(count, sizeof(STGMEDIUM));

			for(i = 0; i < count; i++)
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

void CliprdrDataObject_Delete(CliprdrDataObject *instance)
{
	if (instance)
	{
		if (instance->iDataObject.lpVtbl)
			free(instance->iDataObject.lpVtbl);

		if (instance->m_pFormatEtc)
			free(instance->m_pFormatEtc);

		if (instance->m_pStgMedium)
			free(instance->m_pStgMedium);

		if(instance->m_pStream)
		{
			int i;

			for (i = 0; i < instance->m_nStreams; i++)
				CliprdrStream_Release(instance->m_pStream[i]);

			free(instance->m_pStream);
		}

		free(instance);
	}
}


BOOL wf_create_file_obj(cliprdrContext *cliprdr, IDataObject **ppDataObject)
{
	FORMATETC fmtetc[3];
	STGMEDIUM stgmeds[3];

	if(!ppDataObject)
		return FALSE;

	fmtetc[0].cfFormat        = RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW);
	fmtetc[0].dwAspect        = DVASPECT_CONTENT;
	fmtetc[0].lindex          = 0;
	fmtetc[0].ptd             = NULL;
	fmtetc[0].tymed           = TYMED_HGLOBAL;
	stgmeds[0].tymed          = TYMED_HGLOBAL;
	stgmeds[0].hGlobal        = NULL;
	stgmeds[0].pUnkForRelease = NULL;

	fmtetc[1].cfFormat        = RegisterClipboardFormatW(CFSTR_FILECONTENTS);
	fmtetc[1].dwAspect        = DVASPECT_CONTENT;
	fmtetc[1].lindex          = 0;
	fmtetc[1].ptd             = NULL;
	fmtetc[1].tymed           = TYMED_ISTREAM;
	stgmeds[1].tymed          = TYMED_ISTREAM;
	stgmeds[1].pstm           = NULL;
	stgmeds[1].pUnkForRelease = NULL;

	fmtetc[2].cfFormat        = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
	fmtetc[2].dwAspect        = DVASPECT_CONTENT;
	fmtetc[2].lindex          = 0;
	fmtetc[2].ptd             = NULL;
	fmtetc[2].tymed           = TYMED_HGLOBAL;
	stgmeds[2].tymed          = TYMED_HGLOBAL;
	stgmeds[2].hGlobal        = NULL;
	stgmeds[2].pUnkForRelease = NULL;

	*ppDataObject = (IDataObject *)CliprdrDataObject_New(fmtetc, stgmeds, 3, cliprdr);

	return (*ppDataObject) ? TRUE : FALSE;
}

void wf_destroy_file_obj(IDataObject *instance)
{
	if(instance)
		IDataObject_Release(instance);
}
