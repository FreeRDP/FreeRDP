/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * wts_services.h interfaces across their C and C++ forms, C++ side
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

#include <atomic>

#include <freerdp/wts_services.h>

#include "TestWtsServicesCxx.h"

namespace
{
	std::atomic<int> alive{ 0 };

	/* IWTSBitmapRenderer in C++ form, calling back a callback object written in C */
	class CxxRenderer final : public IWTSBitmapRenderer
	{
	  public:
		explicit CxxRenderer(IWTSBitmapRendererCallback* callback) : _callback(callback)
		{
			WINPR_COM_ADDREF(_callback);
			alive++;
		}

		HRESULT WINPR_COM_CALL QueryInterface(const IID* riid, void** ppvObject) override
		{
			if (!ppvObject)
				return E_POINTER;
			*ppvObject = nullptr;
			if (!riid)
				return E_POINTER;
			if (!winpr_com_guid_equal(riid, &WINPR_IID_IUnknown) &&
			    !winpr_com_guid_equal(riid, &IID_IWTSBitmapRenderer))
				return E_NOINTERFACE;
			AddRef();
			*ppvObject = this;
			return S_OK;
		}

		ULONG WINPR_COM_CALL AddRef() override
		{
			return ++_refCount;
		}

		ULONG WINPR_COM_CALL Release() override
		{
			const ULONG count = --_refCount;
			if (count == 0)
				delete this;
			return count;
		}

		HRESULT WINPR_COM_CALL Render(GUID imageFormat, DWORD dwWidth, DWORD dwHeight,
		                              LONG cbStride, DWORD cbImageBuffer,
		                              BYTE* pImageBuffer) override
		{
			if (!winpr_com_guid_equal(&imageFormat, &TEST_WTS_CXX_FORMAT) || !pImageBuffer ||
			    (cbStride < static_cast<LONG>(dwWidth * 4)) ||
			    (cbImageBuffer < static_cast<DWORD>(cbStride) * dwHeight))
				return E_INVALIDARG;
			if (!_callback)
				return E_UNEXPECTED;
			_frames++;
			/* the callback gets the frame size, offset by its first pixel */
			const LONG first = pImageBuffer[0];
			return _callback->OnTargetSizeChanged(WTS_RECT{ first, first,
			                                                first + static_cast<LONG>(dwWidth),
			                                                first + static_cast<LONG>(dwHeight) });
		}

		HRESULT WINPR_COM_CALL
		GetRendererStatistics(BITMAP_RENDERER_STATISTICS* pStatistics) override
		{
			if (!pStatistics)
				return E_POINTER;
			pStatistics->dwFramesDelivered = _frames;
			pStatistics->dwFramesDropped = 0;
			return S_OK;
		}

		HRESULT WINPR_COM_CALL RemoveMapping() override
		{
			if (_callback)
				WINPR_COM_RELEASE(_callback);
			_callback = nullptr;
			return S_OK;
		}

		CxxRenderer(const CxxRenderer&) = delete;
		CxxRenderer(CxxRenderer&&) = delete;
		CxxRenderer& operator=(const CxxRenderer&) = delete;
		CxxRenderer& operator=(CxxRenderer&&) = delete;

	  private:
		~CxxRenderer()
		{
			(void)RemoveMapping();
			alive--;
		}

		std::atomic<ULONG> _refCount{ 1 };
		IWTSBitmapRendererCallback* _callback;
		DWORD _frames = 0;
	};
} // namespace

IWTSBitmapRenderer* test_wts_cxx_renderer_new(IWTSBitmapRendererCallback* callback)
{
	if (!callback)
		return nullptr;
	return new CxxRenderer(callback);
}

int test_wts_cxx_renderers_alive(void)
{
	return alive;
}
