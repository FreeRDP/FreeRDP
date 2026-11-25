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

#include <stdio.h>
#include <stddef.h>

#include <winpr/error.h>
#include <winpr/com.h>

#define CHECK(cond)                                                          \
	do                                                                       \
	{                                                                        \
		if (!(cond))                                                         \
		{                                                                    \
			(void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); \
			return FALSE;                                                    \
		}                                                                    \
	} while (0)

/* two interfaces of our own, declared in C form */
typedef struct ITestFoo ITestFoo;
typedef struct ITestBar ITestBar;

WINPR_COM_DEFINE_GUID(IID_ITestFoo, 0x01234567, 0x89ab, 0xcdef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab,
                      0xcd, 0xef);
WINPR_COM_DEFINE_GUID(IID_ITestBar, 0x76543210, 0xba98, 0xfedc, 0x10, 0x32, 0x54, 0x76, 0x98, 0xba,
                      0xdc, 0xfe);

typedef struct
{
	wIUnknownVtbl iunknown;
	UINT32(WINPR_COM_CALL* Foo)(ITestFoo* This);
} ITestFooVtbl;

struct ITestFoo
{
	const ITestFooVtbl* lpVtbl;
};

typedef struct
{
	wIUnknownVtbl iunknown;
	UINT32(WINPR_COM_CALL* Bar)(ITestBar* This);
} ITestBarVtbl;

struct ITestBar
{
	const ITestBarVtbl* lpVtbl;
};

static int freed = 0;

/* ---- an object with one interface ---- */

typedef struct
{
	wComBase com; /* ITestFoo */
	UINT32 value;
} CSingle;

WINPR_COM_IUNKNOWN_THUNKS(CSingle, &IID_ITestFoo)

static UINT32 WINPR_COM_CALL CSingle_Foo(ITestFoo* This)
{
	return ((CSingle*)This)->value;
}

static const ITestFooVtbl CSingle_vtbl = {
	.iunknown = WINPR_COM_IUNKNOWN_ENTRIES(CSingle),
	.Foo = CSingle_Foo,
};

static void CSingle_free(wComBase* object)
{
	freed++;
	free(object);
}

static BOOL test_single(void)
{
	/* same layout as the flat vtable MIDL generates */
	WINPR_STATIC_ASSERT(offsetof(ITestFooVtbl, Foo) == 3 * sizeof(void*));

	freed = 0;
	CSingle* obj = calloc(1, sizeof(*obj));
	CHECK(obj);
	winpr_com_base_init(&obj->com, "CSingle", &CSingle_vtbl.iunknown, CSingle_free);
	obj->value = 42;
	ITestFoo* foo = (ITestFoo*)obj;
	CHECK(obj->com.refCount == 1);
	CHECK(foo->lpVtbl->Foo(foo) == 42);

	/* the interface implemented and IUnknown, both on the same pointer */
	ITestFoo* foo2 = nullptr;
	CHECK(SUCCEEDED(WINPR_COM_QUERY_INTERFACE(foo, &IID_ITestFoo, &foo2)));
	CHECK(foo2 == foo);
	wIUnknown* unk = nullptr;
	CHECK(SUCCEEDED(WINPR_COM_QUERY_INTERFACE(foo, &WINPR_IID_IUnknown, &unk)));
	CHECK((void*)unk == (void*)foo);
	CHECK(obj->com.refCount == 3);

	/* anything else */
	void* other = foo;
	CHECK(WINPR_COM_QUERY_INTERFACE(foo, &IID_ITestBar, &other) == E_NOINTERFACE);
	CHECK(other == nullptr);
	CHECK(foo->lpVtbl->iunknown.QueryInterface((wIUnknown*)foo, &IID_ITestFoo, nullptr) ==
	      E_POINTER);
	CHECK(WINPR_COM_QUERY_INTERFACE(foo, nullptr, &other) == E_POINTER);
	CHECK(obj->com.refCount == 3);

	CHECK(WINPR_COM_ADDREF(foo) == 4);
	CHECK(WINPR_COM_RELEASE(foo) == 3);
	CHECK(WINPR_COM_RELEASE(unk) == 2);
	CHECK(WINPR_COM_RELEASE(foo2) == 1);
	CHECK(freed == 0);
	CHECK(WINPR_COM_RELEASE(foo) == 0);
	CHECK(freed == 1);
	return TRUE;
}

/* ---- an object with two interfaces and its own QueryInterface ---- */

typedef struct
{
	wComBase com;              /* ITestFoo, and IUnknown */
	const ITestBarVtbl* lpBar; /* ITestBar */
	UINT32 foo;
	UINT32 bar;
} CDouble;

static CDouble* CDouble_from_bar(wIUnknown* This)
{
	return (CDouble*)((BYTE*)This - offsetof(CDouble, lpBar));
}

static HRESULT CDouble_query(CDouble* obj, const IID* riid, void** ppvObject)
{
	if (!ppvObject)
		return E_POINTER;
	*ppvObject = nullptr;
	if (!riid)
		return E_POINTER;

	if (winpr_com_guid_equal(riid, &WINPR_IID_IUnknown) ||
	    winpr_com_guid_equal(riid, &IID_ITestFoo))
		*ppvObject = &obj->com;
	else if (winpr_com_guid_equal(riid, &IID_ITestBar))
		*ppvObject = (void*)&obj->lpBar;
	else
		return E_NOINTERFACE;
	(void)winpr_com_base_add_ref(&obj->com);
	return S_OK;
}

/* ITestFoo, at the start of the object */
WINPR_COM_REFCOUNT_THUNKS(CDouble)

static HRESULT WINPR_COM_CALL CDouble_QueryInterface(wIUnknown* This, const IID* riid,
                                                     void** ppvObject)
{
	return CDouble_query((CDouble*)This, riid, ppvObject);
}

static UINT32 WINPR_COM_CALL CDouble_Foo(ITestFoo* This)
{
	return ((CDouble*)This)->foo;
}

static const ITestFooVtbl CDouble_vtbl = {
	.iunknown = WINPR_COM_IUNKNOWN_ENTRIES(CDouble),
	.Foo = CDouble_Foo,
};

/* ITestBar, at lpBar */
static HRESULT WINPR_COM_CALL CDoubleBar_QueryInterface(wIUnknown* This, const IID* riid,
                                                        void** ppvObject)
{
	return CDouble_query(CDouble_from_bar(This), riid, ppvObject);
}

static ULONG WINPR_COM_CALL CDoubleBar_AddRef(wIUnknown* This)
{
	return winpr_com_base_add_ref(&CDouble_from_bar(This)->com);
}

static ULONG WINPR_COM_CALL CDoubleBar_Release(wIUnknown* This)
{
	return winpr_com_base_release(&CDouble_from_bar(This)->com);
}

static UINT32 WINPR_COM_CALL CDoubleBar_Bar(ITestBar* This)
{
	return CDouble_from_bar((wIUnknown*)This)->bar;
}

static const ITestBarVtbl CDoubleBar_vtbl = {
	.iunknown = WINPR_COM_IUNKNOWN_ENTRIES(CDoubleBar),
	.Bar = CDoubleBar_Bar,
};

static void CDouble_free(wComBase* object)
{
	freed++;
	free(object);
}

static BOOL test_double(void)
{
	freed = 0;
	CDouble* obj = calloc(1, sizeof(*obj));
	CHECK(obj);
	winpr_com_base_init(&obj->com, "CDouble", &CDouble_vtbl.iunknown, CDouble_free);
	obj->lpBar = &CDoubleBar_vtbl;
	obj->foo = 1;
	obj->bar = 2;
	ITestFoo* foo = (ITestFoo*)obj;

	ITestBar* bar = nullptr;
	CHECK(SUCCEEDED(WINPR_COM_QUERY_INTERFACE(foo, &IID_ITestBar, &bar)));
	CHECK((void*)bar != (void*)foo);
	CHECK(bar->lpVtbl->Bar(bar) == 2);

	/* and back */
	ITestFoo* foo2 = nullptr;
	CHECK(SUCCEEDED(WINPR_COM_QUERY_INTERFACE(bar, &IID_ITestFoo, &foo2)));
	CHECK(foo2 == foo);
	CHECK(foo2->lpVtbl->Foo(foo2) == 1);

	/* identity: IUnknown is the same pointer from any interface */
	wIUnknown* unk1 = nullptr;
	wIUnknown* unk2 = nullptr;
	CHECK(SUCCEEDED(WINPR_COM_QUERY_INTERFACE(foo, &WINPR_IID_IUnknown, &unk1)));
	CHECK(SUCCEEDED(WINPR_COM_QUERY_INTERFACE(bar, &WINPR_IID_IUnknown, &unk2)));
	CHECK(unk1 == unk2);

	/* one reference count for the object, whatever the interface */
	CHECK(obj->com.refCount == 5);
	CHECK(WINPR_COM_RELEASE(unk1) == 4);
	CHECK(WINPR_COM_RELEASE(unk2) == 3);
	CHECK(WINPR_COM_RELEASE(foo2) == 2);
	CHECK(WINPR_COM_RELEASE(foo) == 1);
	CHECK(freed == 0);
	CHECK(WINPR_COM_RELEASE(bar) == 0);
	CHECK(freed == 1);
	return TRUE;
}

static BOOL test_guid(void)
{
	CHECK(winpr_com_guid_equal(&IID_ITestFoo, &IID_ITestFoo));
	CHECK(!winpr_com_guid_equal(&IID_ITestFoo, &IID_ITestBar));

	char guid[64] = WINPR_C_ARRAY_INIT;
	(void)snprintf(guid, sizeof(guid), WINPR_COM_GUID_FMT, WINPR_COM_GUID_ARGS(&IID_ITestFoo));
	CHECK(strcmp(guid, "{01234567-89AB-CDEF-0123-456789ABCDEF}") == 0);
	return TRUE;
}

int TestCom(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_guid() || !test_single() || !test_double())
		return -1;
	return 0;
}
