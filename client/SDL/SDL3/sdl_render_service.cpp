/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client: IWTSBitmapRenderService
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
#include <mutex>
#include <new>

#include <winpr/assert.h>
#include <winpr/error.h>

#include "sdl_context.hpp"
#include "sdl_render_service.hpp"

/* The renderers hand their frames to SdlContext, which places and draws them
 * (presentRenderers). The service lock is held while calling into SdlContext, so detaching
 * waits for the calls under way. */

/** @brief QueryInterface of an object whose only interface is `iid` */
template <typename T>
static HRESULT queryInterface(T* self, const IID* iid, const IID* riid, void** ppvObject)
{
	if (!ppvObject)
		return E_POINTER;
	*ppvObject = nullptr;
	if (!riid)
		return E_POINTER;
	if (!winpr_com_guid_equal(riid, &WINPR_IID_IUnknown) && !winpr_com_guid_equal(riid, iid))
		return E_NOINTERFACE;
	self->AddRef();
	*ppvObject = self;
	return S_OK;
}

namespace
{
	class SdlRenderService final : public IWTSBitmapRenderService
	{
	  public:
		explicit SdlRenderService(SdlContext* sdl) : _sdl(sdl)
		{
		}

		HRESULT WINPR_COM_CALL QueryInterface(const IID* riid, void** ppvObject) override
		{
			return queryInterface(this, &IID_IWTSBitmapRenderService, riid, ppvObject);
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

		HRESULT WINPR_COM_CALL
		GetMappedRenderer(UINT64 mappingId, IWTSBitmapRendererCallback* pMappedRendererCallback,
		                  IWTSBitmapRenderer** ppMappedRenderer) override;

		void detach()
		{
			std::scoped_lock lock(_lock);
			_sdl = nullptr;
		}

		/** @brief `fn(sdl)` with the service lock held, false once detached */
		template <typename Fn> bool withContext(Fn fn)
		{
			std::scoped_lock lock(_lock);
			if (!_sdl)
				return false;
			fn(*_sdl);
			return true;
		}

		SdlRenderService(const SdlRenderService&) = delete;
		SdlRenderService(SdlRenderService&&) = delete;
		SdlRenderService& operator=(const SdlRenderService&) = delete;
		SdlRenderService& operator=(SdlRenderService&&) = delete;

	  private:
		~SdlRenderService() = default;

		std::atomic<ULONG> _refCount{ 1 };
		std::mutex _lock;
		SdlContext* _sdl; /* nullptr once detached */
	};

	class SdlRenderer final : public IWTSBitmapRenderer
	{
	  public:
		SdlRenderer(SdlRenderService* service, Uint64 id) : _service(service), _id(id)
		{
			_service->AddRef();
		}

		HRESULT WINPR_COM_CALL QueryInterface(const IID* riid, void** ppvObject) override
		{
			return queryInterface(this, &IID_IWTSBitmapRenderer, riid, ppvObject);
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
			/* BGRA only (GUID_NULL) */
			static const GUID guid_null = {};
			if (!winpr_com_guid_equal(&imageFormat, &guid_null))
				return E_INVALIDARG;
			if (!pImageBuffer || (dwWidth == 0) || (dwHeight == 0) || (dwWidth > 16384) ||
			    (dwHeight > 16384) || (cbStride < static_cast<LONG>(dwWidth * 4)) ||
			    (cbImageBuffer < (static_cast<DWORD>(cbStride) * (dwHeight - 1)) + (dwWidth * 4)))
				return E_INVALIDARG;

			HRESULT hr = E_UNEXPECTED;
			std::ignore = _service->withContext(
			    [&](SdlContext& sdl)
			    {
				    if (_removed)
					    return;
				    if (sdl.rendererFrame(_id, pImageBuffer, dwWidth, dwHeight, cbStride))
				    {
					    _stats.dwFramesDelivered++;
					    hr = S_OK;
				    }
				    else
					    _stats.dwFramesDropped++;
			    });
			return hr;
		}

		HRESULT WINPR_COM_CALL
		GetRendererStatistics(BITMAP_RENDERER_STATISTICS* pStatistics) override
		{
			if (!pStatistics)
				return E_POINTER;
			/* the stats only change under the service lock */
			if (!_service->withContext([&](SdlContext&) { *pStatistics = _stats; }))
				*pStatistics = _stats;
			return S_OK;
		}

		HRESULT WINPR_COM_CALL RemoveMapping() override
		{
			remove();
			return S_OK;
		}

		SdlRenderer(const SdlRenderer&) = delete;
		SdlRenderer(SdlRenderer&&) = delete;
		SdlRenderer& operator=(const SdlRenderer&) = delete;
		SdlRenderer& operator=(SdlRenderer&&) = delete;

	  private:
		~SdlRenderer()
		{
			remove();
			_service->Release();
		}

		void remove()
		{
			std::ignore = _service->withContext(
			    [&](SdlContext& sdl)
			    {
				    if (_removed)
					    return;
				    _removed = true;
				    sdl.rendererRemove(_id);
			    });
		}

		std::atomic<ULONG> _refCount{ 1 };
		SdlRenderService* _service; /* a reference */
		Uint64 _id;                 /* SdlContext's */
		bool _removed = false;
		BITMAP_RENDERER_STATISTICS _stats{};
	};
} // namespace

HRESULT WINPR_COM_CALL SdlRenderService::GetMappedRenderer(
    UINT64 mappingId, IWTSBitmapRendererCallback* pMappedRendererCallback,
    IWTSBitmapRenderer** ppMappedRenderer)
{
	if (!ppMappedRenderer)
		return E_POINTER;
	*ppMappedRenderer = nullptr;

	Uint64 id = 0;
	if (!withContext([&](SdlContext& sdl)
	                 { id = sdl.rendererAdd(mappingId, pMappedRendererCallback); }))
		return E_UNEXPECTED;

	auto renderer = new (std::nothrow) SdlRenderer(this, id);
	if (!renderer)
	{
		std::ignore = withContext([&](SdlContext& sdl) { sdl.rendererRemove(id); });
		return E_OUTOFMEMORY;
	}
	*ppMappedRenderer = renderer;
	return S_OK;
}

IWTSBitmapRenderService* sdl_render_service_new(SdlContext* sdl)
{
	WINPR_ASSERT(sdl);
	return new (std::nothrow) SdlRenderService(sdl);
}

void sdl_render_service_detach(IWTSBitmapRenderService* service)
{
	auto s = dynamic_cast<SdlRenderService*>(service);
	if (s)
		s->detach();
}
