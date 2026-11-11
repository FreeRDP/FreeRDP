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

#ifndef WINPR_COM_H
#define WINPR_COM_H

#include <string.h>

#include <winpr/platform.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/error.h>

/** @file
 *  Just enough COM to implement and use objects that are binary compatible with Windows COM
 *  objects, on all platforms.
 *
 *  The SDK COM headers (unknwn.h, objbase.h) are never included (codespell:ignore unknwn), even
 *  on Windows: everything here has its own name (wIUnknown, WINPR_IID_IUnknown, ...), so it
 *  coexists with them in a file that includes them. The binary layout and calling convention are
 *  the Windows ones: a wIUnknown* can be passed where Windows expects an IUnknown*, and the other
 *  way around.
 *
 *  In C, an interface is a struct whose only member is `lpVtbl`, and its vtable starts with a
 *  wIUnknownVtbl named `iunknown`, followed by the interface's own methods:
 *
 *	typedef struct IFoo IFoo;
 *	typedef struct
 *	{
 *		wIUnknownVtbl iunknown;
 *		UINT32(WINPR_COM_CALL* Foo)(IFoo* This);
 *	} IFooVtbl;
 *	struct IFoo
 *	{
 *		const IFooVtbl* lpVtbl;
 *	};
 *
 *  This is the layout of a MIDL vtable (the three IUnknown entries, then the others), and
 *  &vtbl->iunknown is a wIUnknownVtbl without any cast. The IUnknown entries take a wIUnknown*;
 *  the WINPR_COM_QUERY_INTERFACE / ADDREF / RELEASE macros call them on any interface.
 *
 *  In C++, wIUnknown is also given as a class with pure virtual methods, which has the same
 *  layout (define WINPR_COM_CINTERFACE to get the C form in C++ too). Such C++ interfaces must not
 *  have a virtual destructor nor overloaded methods, and only use single inheritance, or the
 *  vtable layout differs between compilers.
 *
 *  IIDs are passed as `const IID*` (winpr's POSIX REFIID is not const; on Windows it is the same
 *  pointer).
 *
 *  @since version 3.34.0
 */

/** @brief the calling convention of COM methods (STDMETHODCALLTYPE) */
#if defined(_WIN32)
#define WINPR_COM_CALL __stdcall
#else
#define WINPR_COM_CALL
#endif

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct wIUnknown wIUnknown;

	/** @brief the vtable of IUnknown, the first entries of every COM vtable */
	typedef struct
	{
		HRESULT(WINPR_COM_CALL* QueryInterface)
		(wIUnknown* This, const IID* riid, void** ppvObject);
		ULONG(WINPR_COM_CALL* AddRef)(wIUnknown* This);
		ULONG(WINPR_COM_CALL* Release)(wIUnknown* This);
	} wIUnknownVtbl;

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus) && !defined(WINPR_COM_CINTERFACE)
/** @brief IUnknown, C++ form */
struct wIUnknown
{
	virtual HRESULT WINPR_COM_CALL QueryInterface(const IID* riid, void** ppvObject) = 0;
	virtual ULONG WINPR_COM_CALL AddRef() = 0;
	virtual ULONG WINPR_COM_CALL Release() = 0;

  protected:
	/* not virtual, which would add vtable entries: objects are freed by Release() */
	~wIUnknown() = default;
};
#else
/** @brief IUnknown, C form */
struct wIUnknown
{
	const wIUnknownVtbl* lpVtbl;
};
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief defines a GUID constant visible in the including file only (nothing to link) */
#define WINPR_COM_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
	static const GUID name = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }

	/** @brief IID_IUnknown, {00000000-0000-0000-C000-000000000046} */
	WINPR_COM_DEFINE_GUID(WINPR_IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00,
	                      0x00, 0x00, 0x00, 0x46);

	/** @brief IsEqualGUID */
	static inline BOOL winpr_com_guid_equal(const GUID* a, const GUID* b)
	{
		return memcmp(a, b, sizeof(*a)) == 0;
	}

/** @brief printf a GUID as {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}:
 *  WLog_DBG(TAG, "id=" WINPR_COM_GUID_FMT, WINPR_COM_GUID_ARGS(guid)) */
#define WINPR_COM_GUID_FMT                                                                      \
	"{%08" PRIX32 "-%04" PRIX16 "-%04" PRIX16 "-%02" PRIX8 "%02" PRIX8 "-%02" PRIX8 "%02" PRIX8 \
	"%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "}"
#define WINPR_COM_GUID_ARGS(g)                                                                \
	(UINT32)(g)->Data1, (UINT16)(g)->Data2, (UINT16)(g)->Data3, (g)->Data4[0], (g)->Data4[1], \
	    (g)->Data4[2], (g)->Data4[3], (g)->Data4[4], (g)->Data4[5], (g)->Data4[6], (g)->Data4[7]

/** @brief the wIUnknownVtbl of `obj`, an interface in C form (wIUnknown itself or one whose vtable
 *  starts with `iunknown`) */
#define WINPR_COM_IUNKNOWN_VTBL(obj) ((const wIUnknownVtbl*)(obj)->lpVtbl)

/** @brief IUnknown calls on an interface in C form. In C++, the C++ form of wIUnknown is called
 *  directly: unknown->Release() */
#define WINPR_COM_QUERY_INTERFACE(obj, iid, ppv) \
	(WINPR_COM_IUNKNOWN_VTBL(obj)->QueryInterface((wIUnknown*)(obj), (iid), (void**)(ppv)))
#define WINPR_COM_ADDREF(obj) (WINPR_COM_IUNKNOWN_VTBL(obj)->AddRef((wIUnknown*)(obj)))
#define WINPR_COM_RELEASE(obj) (WINPR_COM_IUNKNOWN_VTBL(obj)->Release((wIUnknown*)(obj)))

	/* ---- implementing COM objects in C ----
	 *
	 * An object starts with a wComBase, whose first member is lpVtbl: a pointer to the object is
	 * the pointer to its first interface, and methods cast This back to the object type.
	 *
	 *	typedef struct {
	 *		wComBase com;   // IWTSFooService
	 *		...state...
	 *	} CFooService;
	 *
	 *	WINPR_COM_IUNKNOWN_THUNKS(CFooService, &IID_IWTSFooService)
	 *	static const IWTSFooServiceVtbl vtbl = {
	 *		.iunknown = WINPR_COM_IUNKNOWN_ENTRIES(CFooService),
	 *		.DoSomething = CFooService_DoSomething,
	 *	};
	 *	...
	 *	winpr_com_base_init(&foo->com, "CFooService", &vtbl.iunknown, CFooService_free);
	 *	return (IWTSFooService*)foo;
	 *
	 * WINPR_COM_IUNKNOWN_THUNKS answers QueryInterface for IUnknown and one interface. An object
	 * with several interfaces uses WINPR_COM_REFCOUNT_THUNKS and writes its own QueryInterface:
	 * each interface not derived from the first one needs its own vtable pointer in the object,
	 * QueryInterface hands out the address of that member, and the methods of that interface get
	 * the object back from it. QueryInterface(IID_IUnknown) must always give the same pointer.
	 */

	typedef struct s_wComBase wComBase;

	/** @brief called when the last reference goes away: releases what the object holds and frees
	 *  it */
	typedef void (*pfnwComBaseFree)(wComBase* object);

	struct s_wComBase
	{
		const wIUnknownVtbl* lpVtbl; /* the object's first interface, must stay first */
		volatile LONG refCount;
		const char* className; /* for logs */
		pfnwComBaseFree Free;
	};

	/** @brief sets the vtable and the reference count to 1, the caller owns that first reference.
	 *  @param vtbl the `iunknown` member of the vtable of the object's first interface
	 *  @since version 3.34.0 */
	WINPR_API void winpr_com_base_init(wComBase* object, const char* className,
	                                   const wIUnknownVtbl* vtbl, pfnwComBaseFree Free);

	/** @brief AddRef of an object starting with a wComBase
	 *  @since version 3.34.0 */
	WINPR_API ULONG winpr_com_base_add_ref(wComBase* object);

	/** @brief Release of an object starting with a wComBase: calls its Free at 0
	 *  @since version 3.34.0 */
	WINPR_API ULONG winpr_com_base_release(wComBase* object);

	/** @brief QueryInterface of an object whose only interface is `iid` (nullptr: IUnknown only).
	 *  On success *ppv is the object, with a new reference
	 *  @since version 3.34.0 */
	WINPR_API HRESULT winpr_com_base_query(wComBase* object, const IID* iid, const IID* riid,
	                                       void** ppv);

/* The IUnknown entries of a vtable, as static functions of the including file: a vtable is then
 * a constant initializer (the address of a function imported from a DLL is not one for MSVC). */

/** @brief prefix_AddRef and prefix_Release, for an object starting with a wComBase */
#define WINPR_COM_REFCOUNT_THUNKS(prefix)                         \
	static ULONG WINPR_COM_CALL prefix##_AddRef(wIUnknown* This)  \
	{                                                             \
		return winpr_com_base_add_ref((wComBase*)This);           \
	}                                                             \
	static ULONG WINPR_COM_CALL prefix##_Release(wIUnknown* This) \
	{                                                             \
		return winpr_com_base_release((wComBase*)This);           \
	}

/** @brief WINPR_COM_REFCOUNT_THUNKS plus prefix_QueryInterface answering IUnknown and `iid`
 *  (a `const IID*`, nullptr for IUnknown only) */
#define WINPR_COM_IUNKNOWN_THUNKS(prefix, iid)                                              \
	static HRESULT WINPR_COM_CALL prefix##_QueryInterface(wIUnknown* This, const IID* riid, \
	                                                      void** ppvObject)                 \
	{                                                                                       \
		return winpr_com_base_query((wComBase*)This, (iid), riid, ppvObject);               \
	}                                                                                       \
	WINPR_COM_REFCOUNT_THUNKS(prefix)

/** @brief the initializer of a wIUnknownVtbl from prefix_QueryInterface (the thunk above or the
 *  object's own), prefix_AddRef and prefix_Release */
#define WINPR_COM_IUNKNOWN_ENTRIES(prefix) \
	{ prefix##_QueryInterface, prefix##_AddRef, prefix##_Release }

#ifdef __cplusplus
}
#endif

#endif /* WINPR_COM_H */
