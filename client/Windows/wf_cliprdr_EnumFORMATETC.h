/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Implementation of the IEnumFORMATETC COM interface
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

#ifndef __WF_CLIPRDR_ENUMFORMATETC_H__
#define __WF_CLIPRDR_ENUMFORMATETC_H__

#define CINTERFACE
#define COBJMACROS

#include <windows.h>
#include <Ole2.h>

typedef struct _CliprdrEnumFORMATETC {
	IEnumFORMATETC iEnumFORMATETC;

	// private
	LONG           m_lRefCount;
	LONG           m_nIndex;
	LONG           m_nNumFormats;
	FORMATETC     *m_pFormatEtc;
} CliprdrEnumFORMATETC;

CliprdrEnumFORMATETC *CliprdrEnumFORMATETC_New(int nFormats, FORMATETC *pFormatEtc);
void CliprdrEnumFORMATETC_Delete(CliprdrEnumFORMATETC *This);

HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_QueryInterface(IEnumFORMATETC *This, REFIID riid, void **ppvObject);
ULONG   STDMETHODCALLTYPE CliprdrEnumFORMATETC_AddRef(IEnumFORMATETC *This);
ULONG   STDMETHODCALLTYPE CliprdrEnumFORMATETC_Release(IEnumFORMATETC *This);
HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Next(IEnumFORMATETC *This, ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched);
HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Skip(IEnumFORMATETC *This, ULONG celt);
HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Reset(IEnumFORMATETC *This);
HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Clone(IEnumFORMATETC *This, IEnumFORMATETC **ppenum);

#endif // __WF_CLIPRDR_ENUMFORMATETC_H__
