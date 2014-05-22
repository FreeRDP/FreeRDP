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

#include <stdio.h>
#include "wf_cliprdr_EnumFORMATETC.h"

static void cliprdr_format_deep_copy(FORMATETC *dest, FORMATETC *source)
{
	*dest = *source;

	if (source->ptd)
	{
		dest->ptd = (DVTARGETDEVICE *)CoTaskMemAlloc(sizeof(DVTARGETDEVICE));
		*(dest->ptd) = *(source->ptd);
	}
}

HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_QueryInterface(IEnumFORMATETC *This, REFIID riid, void **ppvObject)
{
	CliprdrEnumFORMATETC *instance = (CliprdrEnumFORMATETC *)This;

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

ULONG STDMETHODCALLTYPE CliprdrEnumFORMATETC_AddRef(IEnumFORMATETC *This)
{
	CliprdrEnumFORMATETC *instance = (CliprdrEnumFORMATETC *)This;

	return InterlockedIncrement(&instance->m_lRefCount);
}

ULONG STDMETHODCALLTYPE CliprdrEnumFORMATETC_Release(IEnumFORMATETC *This)
{
	CliprdrEnumFORMATETC *instance = (CliprdrEnumFORMATETC *)This;
	LONG count;

	count = InterlockedDecrement(&instance->m_lRefCount);

	if(count == 0)
	{
		CliprdrEnumFORMATETC_Delete(instance);
		return 0;
	}
	else
	{
		return count;
	}
}

HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Next(IEnumFORMATETC *This, ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched)
{
	CliprdrEnumFORMATETC *instance = (CliprdrEnumFORMATETC *)This;
	ULONG copied  = 0;

	if (celt == 0 || !rgelt)
		return E_INVALIDARG;

	while (instance->m_nIndex < instance->m_nNumFormats && copied < celt)
		cliprdr_format_deep_copy(&rgelt[copied++], &instance->m_pFormatEtc[instance->m_nIndex++]);

	if (pceltFetched != 0)
		*pceltFetched = copied;

	return (copied == celt) ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Skip(IEnumFORMATETC *This, ULONG celt)
{
	CliprdrEnumFORMATETC *instance = (CliprdrEnumFORMATETC *)This;

	if (instance->m_nIndex + (LONG) celt > instance->m_nNumFormats)
		return S_FALSE;

	instance->m_nIndex += celt;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Reset(IEnumFORMATETC *This)
{
	CliprdrEnumFORMATETC *instance = (CliprdrEnumFORMATETC *)This;

	instance->m_nIndex = 0;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Clone(IEnumFORMATETC *This, IEnumFORMATETC **ppEnum)
{
	CliprdrEnumFORMATETC *instance = (CliprdrEnumFORMATETC *)This;

	if (!ppEnum)
		return E_INVALIDARG;

	*ppEnum = (IEnumFORMATETC *)CliprdrEnumFORMATETC_New(instance->m_nNumFormats, instance->m_pFormatEtc);

	if (!*ppEnum)
		return E_OUTOFMEMORY;

	((CliprdrEnumFORMATETC *) *ppEnum)->m_nIndex = instance->m_nIndex;

	return S_OK;
}

CliprdrEnumFORMATETC *CliprdrEnumFORMATETC_New(int nFormats, FORMATETC *pFormatEtc)
{
	CliprdrEnumFORMATETC *instance;
	IEnumFORMATETC *iEnumFORMATETC;
	int i;

	if (!pFormatEtc)
		return NULL;

	instance = (CliprdrEnumFORMATETC *)calloc(1, sizeof(CliprdrEnumFORMATETC));

	if (instance)
	{
		iEnumFORMATETC = &instance->iEnumFORMATETC;

		iEnumFORMATETC->lpVtbl = (IEnumFORMATETCVtbl *)calloc(1, sizeof(IEnumFORMATETCVtbl));
		if (iEnumFORMATETC->lpVtbl)
		{
			iEnumFORMATETC->lpVtbl->QueryInterface = CliprdrEnumFORMATETC_QueryInterface;
			iEnumFORMATETC->lpVtbl->AddRef         = CliprdrEnumFORMATETC_AddRef;
			iEnumFORMATETC->lpVtbl->Release        = CliprdrEnumFORMATETC_Release;
			iEnumFORMATETC->lpVtbl->Next           = CliprdrEnumFORMATETC_Next;
			iEnumFORMATETC->lpVtbl->Skip           = CliprdrEnumFORMATETC_Skip;
			iEnumFORMATETC->lpVtbl->Reset          = CliprdrEnumFORMATETC_Reset;
			iEnumFORMATETC->lpVtbl->Clone          = CliprdrEnumFORMATETC_Clone;

			instance->m_lRefCount   = 0;
			instance->m_nIndex      = 0;
			instance->m_nNumFormats = nFormats;
			instance->m_pFormatEtc  = (FORMATETC *)calloc(nFormats, sizeof(FORMATETC));

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

void CliprdrEnumFORMATETC_Delete(CliprdrEnumFORMATETC *instance)
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
