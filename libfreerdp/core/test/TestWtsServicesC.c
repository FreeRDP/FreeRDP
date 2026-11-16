/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * wts_services.h interfaces across their C and C++ forms, C side
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
#include <string.h>

#include <winpr/error.h>

#include <freerdp/wts_services.h>

#include "TestWtsServicesCxx.h"

/* the C side: a callback object in C form handed to a renderer in C++ form, which is used
 * through its C form (TestWtsServicesCxx.cpp) */

#define CHECK(cond)                                                          \
	do                                                                       \
	{                                                                        \
		if (!(cond))                                                         \
		{                                                                    \
			(void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); \
			goto fail;                                                       \
		}                                                                    \
	} while (0)

typedef struct
{
	wComBase com; /* IWTSBitmapRendererCallback */
	WTS_RECT last;
	int calls;
	BOOL freed;
} TestCallback;

static TestCallback testCallback;

WINPR_COM_IUNKNOWN_THUNKS(TestCallback, &IID_IWTSBitmapRendererCallback)

static HRESULT WINPR_COM_CALL TestCallback_OnTargetSizeChanged(IWTSBitmapRendererCallback* This,
                                                               WTS_RECT rcNewSize)
{
	TestCallback* cb = (TestCallback*)This;
	cb->last = rcNewSize;
	cb->calls++;
	return S_OK;
}

static const IWTSBitmapRendererCallbackVtbl TestCallback_vtbl = {
	.iunknown = WINPR_COM_IUNKNOWN_ENTRIES(TestCallback),
	.OnTargetSizeChanged = TestCallback_OnTargetSizeChanged,
};

static void TestCallback_free(wComBase* object)
{
	((TestCallback*)object)->freed = TRUE;
}

int TestWtsServicesC(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int rc = -1;
	IWTSBitmapRenderer* renderer = nullptr;
	IWTSBitmapRenderer* queried = nullptr;
	wIUnknown* unknown = nullptr;
	void* none = nullptr;
	BYTE frame[4 * 4 * 2] = { 7 };
	BITMAP_RENDERER_STATISTICS stats = { 0 };

	winpr_com_base_init(&testCallback.com, "TestCallback", &TestCallback_vtbl.iunknown,
	                    TestCallback_free);
	IWTSBitmapRendererCallback* callback = (IWTSBitmapRendererCallback*)&testCallback;

	renderer = test_wts_cxx_renderer_new(callback);
	CHECK(renderer);
	CHECK(test_wts_cxx_renderers_alive() == 1);
	CHECK(testCallback.com.refCount == 2);

	/* IUnknown, through the C form of a C++ object */
	CHECK(WINPR_COM_QUERY_INTERFACE(renderer, &IID_IWTSBitmapRenderer, &queried) == S_OK);
	CHECK(queried == renderer);
	CHECK(WINPR_COM_RELEASE(queried) == 1);
	CHECK(WINPR_COM_QUERY_INTERFACE(renderer, &WINPR_IID_IUnknown, &unknown) == S_OK);
	CHECK((void*)unknown == (void*)renderer);
	CHECK(WINPR_COM_RELEASE(unknown) == 1);
	CHECK(WINPR_COM_QUERY_INTERFACE(renderer, &IID_IWTSBitmapRenderService, &none) ==
	      E_NOINTERFACE);
	CHECK(!none);

	/* the C++ methods: GUID passed by value, then the C callback gets a RECT by value */
	CHECK(renderer->lpVtbl->Render(renderer, TEST_WTS_CXX_FORMAT, 4, 2, 16, sizeof(frame), frame) ==
	      S_OK);
	CHECK(testCallback.calls == 1);
	CHECK((testCallback.last.left == 7) && (testCallback.last.top == 7));
	CHECK((testCallback.last.right == 11) && (testCallback.last.bottom == 9));
	CHECK(renderer->lpVtbl->Render(renderer, IID_IWTSBitmapRenderer, 4, 2, 16, sizeof(frame),
	                               frame) == E_INVALIDARG);
	CHECK(renderer->lpVtbl->GetRendererStatistics(renderer, &stats) == S_OK);
	CHECK((stats.dwFramesDelivered == 1) && (stats.dwFramesDropped == 0));

	/* the C++ object releases the C one with WINPR_COM_RELEASE in C++ form */
	CHECK(renderer->lpVtbl->RemoveMapping(renderer) == S_OK);
	CHECK(testCallback.com.refCount == 1);
	CHECK(renderer->lpVtbl->Render(renderer, TEST_WTS_CXX_FORMAT, 4, 2, 16, sizeof(frame), frame) ==
	      E_UNEXPECTED);

	CHECK(WINPR_COM_RELEASE(renderer) == 0);
	renderer = nullptr;
	CHECK(test_wts_cxx_renderers_alive() == 0);
	CHECK(WINPR_COM_RELEASE(callback) == 0);
	CHECK(testCallback.freed);
	rc = 0;

fail:
	if (renderer)
		WINPR_COM_RELEASE(renderer);
	return rc;
}
