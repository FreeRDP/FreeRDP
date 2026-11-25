/**
 * WinPR: Windows Portable Runtime
 * Minimal COM object support
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#include <winpr/config.h>

#include <winpr/assert.h>
#include <winpr/interlocked.h>
#include <winpr/wlog.h>
#include <winpr/com.h>

#include "../log.h"
#define TAG WINPR_TAG("utils.com")

void winpr_com_base_init(wComBase* object, const char* className, const wIUnknownVtbl* vtbl,
                         pfnwComBaseFree Free)
{
	WINPR_ASSERT(object);
	WINPR_ASSERT(vtbl);
	WINPR_ASSERT(Free);

	object->lpVtbl = vtbl;
	object->refCount = 1;
	object->className = className ? className : "wComBase";
	object->Free = Free;
}

ULONG winpr_com_base_add_ref(wComBase* object)
{
	WINPR_ASSERT(object);
	const LONG count = InterlockedIncrement(&object->refCount);
	WLog_VRB(TAG, "%s::AddRef() -> %" PRId32, object->className, (INT32)count);
	return (ULONG)count;
}

ULONG winpr_com_base_release(wComBase* object)
{
	WINPR_ASSERT(object);
	const LONG count = InterlockedDecrement(&object->refCount);
	WLog_VRB(TAG, "%s::Release() -> %" PRId32, object->className, (INT32)count);
	if (count == 0)
		object->Free(object);
	else if (count < 0)
		WLog_ERR(TAG, "%s: released more often than referenced", object->className);
	return (ULONG)count;
}

HRESULT winpr_com_base_query(wComBase* object, const IID* iid, const IID* riid, void** ppv)
{
	WINPR_ASSERT(object);

	if (!ppv)
		return E_POINTER;
	*ppv = nullptr;
	if (!riid)
		return E_POINTER;

	if (!winpr_com_guid_equal(riid, &WINPR_IID_IUnknown) &&
	    (!iid || !winpr_com_guid_equal(riid, iid)))
	{
		WLog_DBG(TAG, "%s::QueryInterface(" WINPR_COM_GUID_FMT ") -> E_NOINTERFACE",
		         object->className, WINPR_COM_GUID_ARGS(riid));
		return E_NOINTERFACE;
	}

	(void)winpr_com_base_add_ref(object);
	*ppv = object;
	return S_OK;
}
