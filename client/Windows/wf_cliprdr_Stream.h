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

#ifndef __WF_CLIPRDR_STREAM_H__
#define __WF_CLIPRDR_STREAM_H__

#define CINTERFACE
#define COBJMACROS

#include <windows.h>
#include <Ole2.h>

typedef struct _CliprdrStream {
	IStream         iStream;

	// private
	LONG            m_lRefCount;
	LONG            m_lIndex;
	ULARGE_INTEGER  m_lSize;
	ULARGE_INTEGER  m_lOffset;
	void           *m_pData;
} CliprdrStream;

CliprdrStream *CliprdrStream_New(LONG index, void *pData);
void CliprdrStream_Delete(CliprdrStream *instance);

HRESULT STDMETHODCALLTYPE CliprdrStream_QueryInterface(IStream *This, REFIID riid, void **ppvObject);
ULONG   STDMETHODCALLTYPE CliprdrStream_AddRef(IStream *This);
ULONG   STDMETHODCALLTYPE CliprdrStream_Release(IStream * This);
HRESULT STDMETHODCALLTYPE CliprdrStream_Read(IStream *This, void *pv, ULONG cb, ULONG *pcbRead);
HRESULT STDMETHODCALLTYPE CliprdrStream_Write(IStream *This, const void *pv, ULONG cb, ULONG *pcbWritten);
HRESULT STDMETHODCALLTYPE CliprdrStream_Seek(IStream *This, LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition);
HRESULT STDMETHODCALLTYPE CliprdrStream_SetSize(IStream *This, ULARGE_INTEGER libNewSize);
HRESULT STDMETHODCALLTYPE CliprdrStream_CopyTo(IStream *This, IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten);
HRESULT STDMETHODCALLTYPE CliprdrStream_Commit(IStream *This, DWORD grfCommitFlags);
HRESULT STDMETHODCALLTYPE CliprdrStream_Revert(IStream *This);
HRESULT STDMETHODCALLTYPE CliprdrStream_LockRegion(IStream *This, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
HRESULT STDMETHODCALLTYPE CliprdrStream_UnlockRegion(IStream *This, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
HRESULT STDMETHODCALLTYPE CliprdrStream_Stat(IStream *This, STATSTG *pstatstg, DWORD grfStatFlag);
HRESULT STDMETHODCALLTYPE CliprdrStream_Clone(IStream *This, IStream **ppstm);

#endif // __WF_CLIPRDR_STREAM_H__
