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

#ifndef __WF_CLIPRDR_DATAOBJECT_H__
#define __WF_CLIPRDR_DATAOBJECT_H__

#define CINTERFACE
#define COBJMACROS

#include <windows.h>
#include <Ole2.h>
#include <ShlObj.h>

typedef struct _CliprdrDataObject {
	IDataObject        iDataObject;

	// private
	LONG               m_lRefCount;
	FORMATETC         *m_pFormatEtc;
	STGMEDIUM         *m_pStgMedium;
	LONG               m_nNumFormats;
	LONG               m_nStreams;
	IStream          **m_pStream;
	void              *m_pData;
}CliprdrDataObject;

CliprdrDataObject *CliprdrDataObject_New(FORMATETC *fmtetc, STGMEDIUM *stgmed, int count, void *data);
void CliprdrDataObject_Delete(CliprdrDataObject *instance);

HRESULT STDMETHODCALLTYPE CliprdrDataObject_QueryInterface(IDataObject *This, REFIID riid, void **ppvObject);
ULONG STDMETHODCALLTYPE   CliprdrDataObject_AddRef(IDataObject *This);
ULONG STDMETHODCALLTYPE   CliprdrDataObject_Release(IDataObject *This);
HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetData(IDataObject *This, FORMATETC *pformatetcIn, STGMEDIUM *pmedium);
HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetDataHere(IDataObject *This, FORMATETC *pformatetc, STGMEDIUM *pmedium);
HRESULT STDMETHODCALLTYPE CliprdrDataObject_QueryGetData(IDataObject *This, FORMATETC *pformatetc);
HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetCanonicalFormatEtc(IDataObject *This, FORMATETC *pformatectIn, FORMATETC *pformatetcOut);
HRESULT STDMETHODCALLTYPE CliprdrDataObject_SetData(IDataObject *This, FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease);
HRESULT STDMETHODCALLTYPE CliprdrDataObject_EnumFormatEtc(IDataObject *This, DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc);
HRESULT STDMETHODCALLTYPE CliprdrDataObject_DAdvise(IDataObject *This, FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection);
HRESULT STDMETHODCALLTYPE CliprdrDataObject_DUnadvise(IDataObject *This, DWORD dwConnection);
HRESULT STDMETHODCALLTYPE CliprdrDataObject_EnumDAdvise(IDataObject *This, IEnumSTATDATA **ppenumAdvise);

#endif // __WF_CLIPRDR_DATAOBJECT_H__
