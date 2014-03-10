/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Implementation of the IStream COM interface
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

HRESULT STDMETHODCALLTYPE CliprdrStream_QueryInterface(IStream *This, REFIID riid, void **ppvObject)
{
	CliprdrStream *instance = (CliprdrStream *)This;

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

ULONG STDMETHODCALLTYPE CliprdrStream_AddRef(IStream *This)
{
	CliprdrStream *instance = (CliprdrStream *)This;

	return InterlockedIncrement(&instance->m_lRefCount);
}

ULONG STDMETHODCALLTYPE CliprdrStream_Release(IStream * This)
{
	CliprdrStream *instance = (CliprdrStream *)This;
	LONG count;

	count = InterlockedDecrement(&instance->m_lRefCount);

	if(count == 0)
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

HRESULT STDMETHODCALLTYPE CliprdrStream_Read(IStream *This, void *pv, ULONG cb, ULONG *pcbRead)
{
	CliprdrStream *instance = (CliprdrStream *)This;
	cliprdrContext *cliprdr = (cliprdrContext *)instance->m_pData;
	int ret;

	if (pv == NULL || pcbRead == NULL)
		return E_INVALIDARG;

	*pcbRead = 0;
	if (instance->m_lOffset.QuadPart >= instance->m_lSize.QuadPart)
		return S_FALSE;

	ret = cliprdr_send_request_filecontents(cliprdr, (void *)This,
									instance->m_lIndex, FILECONTENTS_RANGE,
									instance->m_lOffset.HighPart, instance->m_lOffset.LowPart,
									cb);
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

HRESULT STDMETHODCALLTYPE CliprdrStream_Write(IStream *This, const void *pv, ULONG cb, ULONG *pcbWritten)
{
	CliprdrStream *instance = (CliprdrStream *)This;

	return STG_E_ACCESSDENIED;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Seek(IStream *This, LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
{
	CliprdrStream *instance = (CliprdrStream *)This;
	ULONGLONG newoffset;

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

HRESULT STDMETHODCALLTYPE CliprdrStream_SetSize(IStream *This, ULARGE_INTEGER libNewSize)
{
	CliprdrStream *instance = (CliprdrStream *)This;

	return STG_E_INSUFFICIENTMEMORY;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_CopyTo(IStream *This, IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten)
{
	CliprdrStream *instance = (CliprdrStream *)This;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Commit(IStream *This, DWORD grfCommitFlags)
{
	CliprdrStream *instance = (CliprdrStream *)This;

	return STG_E_MEDIUMFULL;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Revert(IStream *This)
{
	CliprdrStream *instance = (CliprdrStream *)This;

	return STG_E_INSUFFICIENTMEMORY;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_LockRegion(IStream *This, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	CliprdrStream *instance = (CliprdrStream *)This;

	return STG_E_INSUFFICIENTMEMORY;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_UnlockRegion(IStream *This, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	CliprdrStream *instance = (CliprdrStream *)This;

	return STG_E_INSUFFICIENTMEMORY;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Stat(IStream *This, STATSTG *pstatstg, DWORD grfStatFlag)
{
	CliprdrStream *instance = (CliprdrStream *)This;

	if (pstatstg == NULL)
		return STG_E_INVALIDPOINTER;

	ZeroMemory(pstatstg, sizeof(STATSTG));

	switch (grfStatFlag)
	{
		case STATFLAG_DEFAULT:
			return STG_E_INSUFFICIENTMEMORY;

		case STATFLAG_NONAME:
			pstatstg->cbSize.QuadPart   = instance->m_lSize.QuadPart;
			pstatstg->grfLocksSupported = LOCK_EXCLUSIVE;
			pstatstg->grfMode           = GENERIC_READ;
			pstatstg->grfStateBits      = 0;
			pstatstg->type              = STGTY_STREAM;
			break;

		case STATFLAG_NOOPEN:
			return STG_E_INVALIDFLAG;

		default:
			return STG_E_INVALIDFLAG;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrStream_Clone(IStream *This, IStream **ppstm)
{
	CliprdrStream *instance = (CliprdrStream *)This;

	return STG_E_INSUFFICIENTMEMORY;
}

CliprdrStream *CliprdrStream_New(LONG index, void *pData)
{
	cliprdrContext *cliprdr = (cliprdrContext *)pData;
	CliprdrStream *instance;
	IStream *iStream;

	instance = (CliprdrStream *)calloc(1, sizeof(CliprdrStream));

	if (instance)
	{
		iStream = &instance->iStream;

		iStream->lpVtbl = (IStreamVtbl *)calloc(1, sizeof(IStreamVtbl));
		if (iStream->lpVtbl)
		{
			iStream->lpVtbl->QueryInterface = CliprdrStream_QueryInterface;
			iStream->lpVtbl->AddRef         = CliprdrStream_AddRef;
			iStream->lpVtbl->Release        = CliprdrStream_Release;
			iStream->lpVtbl->Read           = CliprdrStream_Read;
			iStream->lpVtbl->Write          = CliprdrStream_Write;
			iStream->lpVtbl->Seek           = CliprdrStream_Seek;
			iStream->lpVtbl->SetSize        = CliprdrStream_SetSize;
			iStream->lpVtbl->CopyTo         = CliprdrStream_CopyTo;
			iStream->lpVtbl->Commit         = CliprdrStream_Commit;
			iStream->lpVtbl->Revert         = CliprdrStream_Revert;
			iStream->lpVtbl->LockRegion     = CliprdrStream_LockRegion;
			iStream->lpVtbl->UnlockRegion   = CliprdrStream_UnlockRegion;
			iStream->lpVtbl->Stat           = CliprdrStream_Stat;
			iStream->lpVtbl->Clone          = CliprdrStream_Clone;

			instance->m_lRefCount        = 1;
			instance->m_lIndex           = index;
			instance->m_pData            = pData;
			instance->m_lOffset.QuadPart = 0;

			/* get content size of this stream */
			cliprdr_send_request_filecontents(cliprdr, (void *)instance, instance->m_lIndex, FILECONTENTS_SIZE, 0, 0, 8);
			instance->m_lSize.QuadPart = *(LONGLONG *)cliprdr->req_fdata;
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

void CliprdrStream_Delete(CliprdrStream *instance)
{
	if (instance)
	{
		if (instance->iStream.lpVtbl)
			free(instance->iStream.lpVtbl);
		free(instance);
	}
}
