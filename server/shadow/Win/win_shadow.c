/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

#include "../shadow_screen.h"
#include "../shadow_surface.h"

#include "win_shadow.h"

#ifdef WITH_DXGI_1_2

static D3D_DRIVER_TYPE DriverTypes[] =
{
	D3D_DRIVER_TYPE_HARDWARE,
	D3D_DRIVER_TYPE_WARP,
	D3D_DRIVER_TYPE_REFERENCE,
};

static UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

static D3D_FEATURE_LEVEL FeatureLevels[] =
{
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_1
};

static UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);

static HMODULE d3d11_module = NULL;

typedef HRESULT (WINAPI * fnD3D11CreateDevice)(
	IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
	CONST D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
	ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext);

static fnD3D11CreateDevice pfnD3D11CreateDevice = NULL;

#undef DEFINE_GUID
#define INITGUID

#include <initguid.h>

/* d3d11.h GUIDs */

DEFINE_GUID(IID_ID3D11DeviceChild,0x1841e5c8,0x16b0,0x489b,0xbc,0xc8,0x44,0xcf,0xb0,0xd5,0xde,0xae);
DEFINE_GUID(IID_ID3D11DepthStencilState,0x03823efb,0x8d8f,0x4e1c,0x9a,0xa2,0xf6,0x4b,0xb2,0xcb,0xfd,0xf1);
DEFINE_GUID(IID_ID3D11BlendState,0x75b68faa,0x347d,0x4159,0x8f,0x45,0xa0,0x64,0x0f,0x01,0xcd,0x9a);
DEFINE_GUID(IID_ID3D11RasterizerState,0x9bb4ab81,0xab1a,0x4d8f,0xb5,0x06,0xfc,0x04,0x20,0x0b,0x6e,0xe7);
DEFINE_GUID(IID_ID3D11Resource,0xdc8e63f3,0xd12b,0x4952,0xb4,0x7b,0x5e,0x45,0x02,0x6a,0x86,0x2d);
DEFINE_GUID(IID_ID3D11Buffer,0x48570b85,0xd1ee,0x4fcd,0xa2,0x50,0xeb,0x35,0x07,0x22,0xb0,0x37);
DEFINE_GUID(IID_ID3D11Texture1D,0xf8fb5c27,0xc6b3,0x4f75,0xa4,0xc8,0x43,0x9a,0xf2,0xef,0x56,0x4c);
DEFINE_GUID(IID_ID3D11Texture2D,0x6f15aaf2,0xd208,0x4e89,0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c);
DEFINE_GUID(IID_ID3D11Texture3D,0x037e866e,0xf56d,0x4357,0xa8,0xaf,0x9d,0xab,0xbe,0x6e,0x25,0x0e);
DEFINE_GUID(IID_ID3D11View,0x839d1216,0xbb2e,0x412b,0xb7,0xf4,0xa9,0xdb,0xeb,0xe0,0x8e,0xd1);
DEFINE_GUID(IID_ID3D11ShaderResourceView,0xb0e06fe0,0x8192,0x4e1a,0xb1,0xca,0x36,0xd7,0x41,0x47,0x10,0xb2);
DEFINE_GUID(IID_ID3D11RenderTargetView,0xdfdba067,0x0b8d,0x4865,0x87,0x5b,0xd7,0xb4,0x51,0x6c,0xc1,0x64);
DEFINE_GUID(IID_ID3D11DepthStencilView,0x9fdac92a,0x1876,0x48c3,0xaf,0xad,0x25,0xb9,0x4f,0x84,0xa9,0xb6);
DEFINE_GUID(IID_ID3D11UnorderedAccessView,0x28acf509,0x7f5c,0x48f6,0x86,0x11,0xf3,0x16,0x01,0x0a,0x63,0x80);
DEFINE_GUID(IID_ID3D11VertexShader,0x3b301d64,0xd678,0x4289,0x88,0x97,0x22,0xf8,0x92,0x8b,0x72,0xf3);
DEFINE_GUID(IID_ID3D11HullShader,0x8e5c6061,0x628a,0x4c8e,0x82,0x64,0xbb,0xe4,0x5c,0xb3,0xd5,0xdd);
DEFINE_GUID(IID_ID3D11DomainShader,0xf582c508,0x0f36,0x490c,0x99,0x77,0x31,0xee,0xce,0x26,0x8c,0xfa);
DEFINE_GUID(IID_ID3D11GeometryShader,0x38325b96,0xeffb,0x4022,0xba,0x02,0x2e,0x79,0x5b,0x70,0x27,0x5c);
DEFINE_GUID(IID_ID3D11PixelShader,0xea82e40d,0x51dc,0x4f33,0x93,0xd4,0xdb,0x7c,0x91,0x25,0xae,0x8c);
DEFINE_GUID(IID_ID3D11ComputeShader,0x4f5b196e,0xc2bd,0x495e,0xbd,0x01,0x1f,0xde,0xd3,0x8e,0x49,0x69);
DEFINE_GUID(IID_ID3D11InputLayout,0xe4819ddc,0x4cf0,0x4025,0xbd,0x26,0x5d,0xe8,0x2a,0x3e,0x07,0xb7);
DEFINE_GUID(IID_ID3D11SamplerState,0xda6fea51,0x564c,0x4487,0x98,0x10,0xf0,0xd0,0xf9,0xb4,0xe3,0xa5);
DEFINE_GUID(IID_ID3D11Asynchronous,0x4b35d0cd,0x1e15,0x4258,0x9c,0x98,0x1b,0x13,0x33,0xf6,0xdd,0x3b);
DEFINE_GUID(IID_ID3D11Query,0xd6c00747,0x87b7,0x425e,0xb8,0x4d,0x44,0xd1,0x08,0x56,0x0a,0xfd);
DEFINE_GUID(IID_ID3D11Predicate,0x9eb576dd,0x9f77,0x4d86,0x81,0xaa,0x8b,0xab,0x5f,0xe4,0x90,0xe2);
DEFINE_GUID(IID_ID3D11Counter,0x6e8c49fb,0xa371,0x4770,0xb4,0x40,0x29,0x08,0x60,0x22,0xb7,0x41);
DEFINE_GUID(IID_ID3D11ClassInstance,0xa6cd7faa,0xb0b7,0x4a2f,0x94,0x36,0x86,0x62,0xa6,0x57,0x97,0xcb);
DEFINE_GUID(IID_ID3D11ClassLinkage,0xddf57cba,0x9543,0x46e4,0xa1,0x2b,0xf2,0x07,0xa0,0xfe,0x7f,0xed);
DEFINE_GUID(IID_ID3D11CommandList,0xa24bc4d1,0x769e,0x43f7,0x80,0x13,0x98,0xff,0x56,0x6c,0x18,0xe2);
DEFINE_GUID(IID_ID3D11DeviceContext,0xc0bfa96c,0xe089,0x44fb,0x8e,0xaf,0x26,0xf8,0x79,0x61,0x90,0xda);
DEFINE_GUID(IID_ID3D11VideoDecoder,0x3C9C5B51,0x995D,0x48d1,0x9B,0x8D,0xFA,0x5C,0xAE,0xDE,0xD6,0x5C);
DEFINE_GUID(IID_ID3D11VideoProcessorEnumerator,0x31627037,0x53AB,0x4200,0x90,0x61,0x05,0xFA,0xA9,0xAB,0x45,0xF9);
DEFINE_GUID(IID_ID3D11VideoProcessor,0x1D7B0652,0x185F,0x41c6,0x85,0xCE,0x0C,0x5B,0xE3,0xD4,0xAE,0x6C);
DEFINE_GUID(IID_ID3D11AuthenticatedChannel,0x3015A308,0xDCBD,0x47aa,0xA7,0x47,0x19,0x24,0x86,0xD1,0x4D,0x4A);
DEFINE_GUID(IID_ID3D11CryptoSession,0x9B32F9AD,0xBDCC,0x40a6,0xA3,0x9D,0xD5,0xC8,0x65,0x84,0x57,0x20);
DEFINE_GUID(IID_ID3D11VideoDecoderOutputView,0xC2931AEA,0x2A85,0x4f20,0x86,0x0F,0xFB,0xA1,0xFD,0x25,0x6E,0x18);
DEFINE_GUID(IID_ID3D11VideoProcessorInputView,0x11EC5A5F,0x51DC,0x4945,0xAB,0x34,0x6E,0x8C,0x21,0x30,0x0E,0xA5);
DEFINE_GUID(IID_ID3D11VideoProcessorOutputView,0xA048285E,0x25A9,0x4527,0xBD,0x93,0xD6,0x8B,0x68,0xC4,0x42,0x54);
DEFINE_GUID(IID_ID3D11VideoContext,0x61F21C45,0x3C0E,0x4a74,0x9C,0xEA,0x67,0x10,0x0D,0x9A,0xD5,0xE4);
DEFINE_GUID(IID_ID3D11VideoDevice,0x10EC4D5B,0x975A,0x4689,0xB9,0xE4,0xD0,0xAA,0xC3,0x0F,0xE3,0x33);
DEFINE_GUID(IID_ID3D11Device,0xdb6f6ddb,0xac77,0x4e88,0x82,0x53,0x81,0x9d,0xf9,0xbb,0xf1,0x40);

/* dxgi.h GUIDs */

DEFINE_GUID(IID_IDXGIObject, 0xaec22fb8, 0x76f3, 0x4639, 0x9b, 0xe0, 0x28, 0xeb, 0x43, 0xa6, 0x7a, 0x2e);
DEFINE_GUID(IID_IDXGIDeviceSubObject, 0x3d3e0379, 0xf9de, 0x4d58, 0xbb, 0x6c, 0x18, 0xd6, 0x29, 0x92, 0xf1, 0xa6);
DEFINE_GUID(IID_IDXGIResource, 0x035f3ab4, 0x482e, 0x4e50, 0xb4, 0x1f, 0x8a, 0x7f, 0x8b, 0xd8, 0x96, 0x0b);
DEFINE_GUID(IID_IDXGIKeyedMutex, 0x9d8e1289, 0xd7b3, 0x465f, 0x81, 0x26, 0x25, 0x0e, 0x34, 0x9a, 0xf8, 0x5d);
DEFINE_GUID(IID_IDXGISurface, 0xcafcb56c, 0x6ac3, 0x4889, 0xbf, 0x47, 0x9e, 0x23, 0xbb, 0xd2, 0x60, 0xec);
DEFINE_GUID(IID_IDXGISurface1, 0x4AE63092, 0x6327, 0x4c1b, 0x80, 0xAE, 0xBF, 0xE1, 0x2E, 0xA3, 0x2B, 0x86);
DEFINE_GUID(IID_IDXGIAdapter, 0x2411e7e1, 0x12ac, 0x4ccf, 0xbd, 0x14, 0x97, 0x98, 0xe8, 0x53, 0x4d, 0xc0);
DEFINE_GUID(IID_IDXGIOutput, 0xae02eedb, 0xc735, 0x4690, 0x8d, 0x52, 0x5a, 0x8d, 0xc2, 0x02, 0x13, 0xaa);
DEFINE_GUID(IID_IDXGISwapChain, 0x310d36a0, 0xd2e7, 0x4c0a, 0xaa, 0x04, 0x6a, 0x9d, 0x23, 0xb8, 0x88, 0x6a);
DEFINE_GUID(IID_IDXGIFactory, 0x7b7166ec, 0x21c7, 0x44ae, 0xb2, 0x1a, 0xc9, 0xae, 0x32, 0x1a, 0xe3, 0x69);
DEFINE_GUID(IID_IDXGIDevice, 0x54ec77fa, 0x1377, 0x44e6, 0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c);
DEFINE_GUID(IID_IDXGIFactory1, 0x770aae78, 0xf26f, 0x4dba, 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87);
DEFINE_GUID(IID_IDXGIAdapter1, 0x29038f61, 0x3839, 0x4626, 0x91, 0xfd, 0x08, 0x68, 0x79, 0x01, 0x1a, 0x05);
DEFINE_GUID(IID_IDXGIDevice1, 0x77db970f, 0x6276, 0x48ba, 0xba, 0x28, 0x07, 0x01, 0x43, 0xb4, 0x39, 0x2c);

/* dxgi1_2.h GUIDs */

DEFINE_GUID(IID_IDXGIDisplayControl, 0xea9dbf1a, 0xc88e, 0x4486, 0x85, 0x4a, 0x98, 0xaa, 0x01, 0x38, 0xf3, 0x0c);
DEFINE_GUID(IID_IDXGIOutputDuplication, 0x191cfac3, 0xa341, 0x470d, 0xb2, 0x6e, 0xa8, 0x64, 0xf4, 0x28, 0x31, 0x9c);
DEFINE_GUID(IID_IDXGISurface2, 0xaba496dd, 0xb617, 0x4cb8, 0xa8, 0x66, 0xbc, 0x44, 0xd7, 0xeb, 0x1f, 0xa2);
DEFINE_GUID(IID_IDXGIResource1, 0x30961379, 0x4609, 0x4a41, 0x99, 0x8e, 0x54, 0xfe, 0x56, 0x7e, 0xe0, 0xc1);
DEFINE_GUID(IID_IDXGIDevice2, 0x05008617, 0xfbfd, 0x4051, 0xa7, 0x90, 0x14, 0x48, 0x84, 0xb4, 0xf6, 0xa9);
DEFINE_GUID(IID_IDXGISwapChain1, 0x790a45f7, 0x0d42, 0x4876, 0x98, 0x3a, 0x0a, 0x55, 0xcf, 0xe6, 0xf4, 0xaa);
DEFINE_GUID(IID_IDXGIFactory2, 0x50c83a1c, 0xe072, 0x4c48, 0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0);
DEFINE_GUID(IID_IDXGIAdapter2, 0x0AA1AE0A, 0xFA0E, 0x4B84, 0x86, 0x44, 0xE0, 0x5F, 0xF8, 0xE5, 0xAC, 0xB5);
DEFINE_GUID(IID_IDXGIOutput1, 0x00cddea8, 0x939b, 0x4b83, 0xa3, 0x40, 0xa6, 0x85, 0x22, 0x66, 0x66, 0xcc);

static void win_shadow_d3d11_module_init()
{
	if (d3d11_module)
		return;

	d3d11_module = LoadLibraryA("d3d11.dll");

	if (!d3d11_module)
		return;

	pfnD3D11CreateDevice = (fnD3D11CreateDevice) GetProcAddress(d3d11_module, "D3D11CreateDevice");
}

int win_shadow_dxgi_init_duplication(winShadowSubsystem* subsystem)
{
	HRESULT hr;
	UINT dTop, i = 0;
	IDXGIOutput* pOutput;
	DXGI_OUTPUT_DESC outputDesc;
	D3D11_TEXTURE2D_DESC textureDesc;
	IDXGIDevice* DxgiDevice = NULL;
	IDXGIAdapter* DxgiAdapter = NULL;
	IDXGIOutput* DxgiOutput = NULL;
	IDXGIOutput1* DxgiOutput1 = NULL;

	hr = subsystem->dxgiDevice->lpVtbl->QueryInterface(subsystem->dxgiDevice,
		&IID_IDXGIDevice, (void**) &DxgiDevice);

	if (FAILED(hr))
		return -1;

	hr = DxgiDevice->lpVtbl->GetParent(DxgiDevice, &IID_IDXGIAdapter, (void**) &DxgiAdapter);

	DxgiDevice->lpVtbl->Release(DxgiDevice);
	DxgiDevice = NULL;

	if (FAILED(hr))
		return -1;

	pOutput = NULL;
	ZeroMemory(&outputDesc, sizeof(outputDesc));

	while (DxgiAdapter->lpVtbl->EnumOutputs(DxgiAdapter, i, &pOutput) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC* pDesc = &outputDesc;

		hr = pOutput->lpVtbl->GetDesc(pOutput, pDesc);

		if (FAILED(hr))
			return -1;

		if (pDesc->AttachedToDesktop)
			dTop = i;

		pOutput->lpVtbl->Release(pOutput);
		i++;
	}

	dTop = 0; /* screen id */

	hr = DxgiAdapter->lpVtbl->EnumOutputs(DxgiAdapter, dTop, &DxgiOutput);

	DxgiAdapter->lpVtbl->Release(DxgiAdapter);
	DxgiAdapter = NULL;

	if (FAILED(hr))
		return -1;

	hr = DxgiOutput->lpVtbl->QueryInterface(DxgiOutput, &IID_IDXGIOutput1, (void**) &DxgiOutput1);

	DxgiOutput->lpVtbl->Release(DxgiOutput);
	DxgiOutput = NULL;

	if (FAILED(hr))
		return -1;

	hr = DxgiOutput1->lpVtbl->DuplicateOutput(DxgiOutput1, (IUnknown*) subsystem->dxgiDevice,
		&(subsystem->dxgiOutputDuplication));

	DxgiOutput1->lpVtbl->Release(DxgiOutput1);
	DxgiOutput1 = NULL;

	if (FAILED(hr))
		return -1;

	textureDesc.Width = subsystem->width;
	textureDesc.Height = subsystem->height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_STAGING;
	textureDesc.BindFlags = 0;
	textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	textureDesc.MiscFlags = 0;

	hr = subsystem->dxgiDevice->lpVtbl->CreateTexture2D(subsystem->dxgiDevice,
		&textureDesc, NULL, &(subsystem->dxgiStage));

	if (FAILED(hr))
		return -1;

	return 1;
}

int win_shadow_dxgi_init(winShadowSubsystem* subsystem)
{
	UINT i = 0;
	HRESULT hr;
	UINT DriverTypeIndex;
	IDXGIDevice* DxgiDevice = NULL;
	IDXGIAdapter* DxgiAdapter = NULL;
	IDXGIOutput* DxgiOutput = NULL;
	IDXGIOutput1* DxgiOutput1 = NULL;

	win_shadow_d3d11_module_init();

	if (!pfnD3D11CreateDevice)
		return -1;

	for (DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex)
	{
		hr = pfnD3D11CreateDevice(NULL, DriverTypes[DriverTypeIndex], NULL, 0, FeatureLevels,
			NumFeatureLevels, D3D11_SDK_VERSION, &(subsystem->dxgiDevice), &(subsystem->featureLevel),
			&(subsystem->dxgiDeviceContext));

		if (SUCCEEDED(hr))
			break;
	}

	if (FAILED(hr))
		return -1;

	win_shadow_dxgi_init_duplication(subsystem);

	return 1;
}

int win_shadow_dxgi_uninit(winShadowSubsystem* subsystem)
{
	if (subsystem->dxgiStage)
	{
		subsystem->dxgiStage->lpVtbl->Release(subsystem->dxgiStage);
		subsystem->dxgiStage = NULL;
	}

	if (subsystem->dxgiDesktopImage)
	{
		subsystem->dxgiDesktopImage->lpVtbl->Release(subsystem->dxgiDesktopImage);
		subsystem->dxgiDesktopImage = NULL;
	}

	if (subsystem->dxgiOutputDuplication)
	{
		subsystem->dxgiOutputDuplication->lpVtbl->Release(subsystem->dxgiOutputDuplication);
		subsystem->dxgiOutputDuplication = NULL;
	}

	if (subsystem->dxgiDeviceContext)
	{
		subsystem->dxgiDeviceContext->lpVtbl->Release(subsystem->dxgiDeviceContext);
		subsystem->dxgiDeviceContext = NULL;
	}

	if (subsystem->dxgiDevice)
	{
		subsystem->dxgiDevice->lpVtbl->Release(subsystem->dxgiDevice);
		subsystem->dxgiDevice = NULL;
	}

	return 1;
}

int win_shadow_dxgi_fetch_frame_data(winShadowSubsystem* subsystem,
	BYTE** ppDstData, int* pnDstStep, int x, int y, int width, int height)
{
	HRESULT hr;
	D3D11_BOX Box;
	DXGI_MAPPED_RECT mappedRect;

	Box.top = x;
	Box.left = y;
	Box.right = x + width;
	Box.bottom = y + height;
	Box.front = 0;
	Box.back = 1;

	subsystem->dxgiDeviceContext->lpVtbl->CopySubresourceRegion(subsystem->dxgiDeviceContext,
		(ID3D11Resource*) subsystem->dxgiStage, 0, x, y, 0, (ID3D11Resource*) subsystem->dxgiDesktopImage, 0, &Box);	 

	hr = subsystem->dxgiStage->lpVtbl->QueryInterface(subsystem->dxgiStage,
		&IID_IDXGISurface, (void**) &(subsystem->dxgiSurface));

	if (FAILED(hr))
		return -1;

	hr = subsystem->dxgiSurface->lpVtbl->Map(subsystem->dxgiSurface, &mappedRect, DXGI_MAP_READ);

	if (FAILED(hr))
		return -1;
		
	*ppDstData = mappedRect.pBits;
	*pnDstStep = mappedRect.Pitch;

	return 1;
}

int win_shadow_dxgi_release_frame_data(winShadowSubsystem* subsystem)
{
	HRESULT hr;

	if (subsystem->dxgiSurface)
	{
		subsystem->dxgiSurface->lpVtbl->Unmap(subsystem->dxgiSurface);
		subsystem->dxgiSurface->lpVtbl->Release(subsystem->dxgiSurface);
		subsystem->dxgiSurface = NULL;
	}

	hr = subsystem->dxgiOutputDuplication->lpVtbl->ReleaseFrame(subsystem->dxgiOutputDuplication);

	subsystem->pendingFrames = 0;

	if (FAILED(hr))
		return -1;

	return 1;
}

int win_shadow_dxgi_get_next_frame(winShadowSubsystem* subsystem)
{
	UINT i = 0;
	int status;
	HRESULT hr = 0;
	UINT DataBufferSize = 0;
	BYTE* DataBuffer = NULL;

	if (subsystem->pendingFrames > 0)
	{
		win_shadow_dxgi_release_frame_data(subsystem);
	}

	if (subsystem->dxgiDesktopImage)
	{
		subsystem->dxgiDesktopImage->lpVtbl->Release(subsystem->dxgiDesktopImage);
		subsystem->dxgiDesktopImage = NULL;
	}

	hr = subsystem->dxgiOutputDuplication->lpVtbl->AcquireNextFrame(subsystem->dxgiOutputDuplication,
		0, &(subsystem->dxgiFrameInfo), &(subsystem->dxgiResource));

	if (hr == DXGI_ERROR_WAIT_TIMEOUT)
		return 0;

	if (FAILED(hr))
	{
		if (hr == DXGI_ERROR_ACCESS_LOST)
		{
			if (subsystem->dxgiDesktopImage)
			{
				subsystem->dxgiDesktopImage->lpVtbl->Release(subsystem->dxgiDesktopImage);
				subsystem->dxgiDesktopImage = NULL;
			}

			if (subsystem->dxgiOutputDuplication)
			{
				subsystem->dxgiOutputDuplication->lpVtbl->Release(subsystem->dxgiOutputDuplication);
				subsystem->dxgiOutputDuplication = NULL;
			} 

			status = win_shadow_dxgi_init_duplication(subsystem);

			if (status < 0)
				return -1;

			return 0;
		}
		else
		{
			hr = subsystem->dxgiOutputDuplication->lpVtbl->ReleaseFrame(subsystem->dxgiOutputDuplication);

			if (FAILED(hr))
				return -1;
		
			return -1;
		}
	}
		
	hr = subsystem->dxgiResource->lpVtbl->QueryInterface(subsystem->dxgiResource,
		&IID_ID3D11Texture2D, (void**) &(subsystem->dxgiDesktopImage));

	subsystem->dxgiResource->lpVtbl->Release(subsystem->dxgiResource);
	subsystem->dxgiResource = NULL;

	if (FAILED(hr))
		return -1;

	subsystem->pendingFrames = subsystem->dxgiFrameInfo.AccumulatedFrames;

	if (subsystem->pendingFrames == 0)
	{
		hr = subsystem->dxgiOutputDuplication->lpVtbl->ReleaseFrame(subsystem->dxgiOutputDuplication);

		if (FAILED(hr))
			return -1;
	}

	return 1;
}

int win_shadow_dxgi_get_invalid_region(winShadowSubsystem* subsystem)
{
	UINT i;
	HRESULT hr;
	POINT* pSrcPt;
	RECT*  pDstRect;
	RECT* pDirtyRect;
	UINT numMoveRects;
	UINT numDirtyRects;
	UINT UsedBufferSize;
	rdpShadowServer* server;
	rdpShadowScreen* screen;
	RECTANGLE_16 invalidRect;
	UINT MetadataBufferSize;
	UINT MoveRectsBufferSize;
	UINT DirtyRectsBufferSize;
	UINT PointerShapeBufferSize;
	RECT* pDirtyRectsBuffer;
	void* pPointerShapeBuffer;
	DXGI_OUTDUPL_MOVE_RECT* pMoveRect;
	DXGI_OUTDUPL_MOVE_RECT* pMoveRectBuffer;
	DXGI_OUTDUPL_POINTER_SHAPE_INFO PointerShapeInfo;

	server = subsystem->server;
	screen = server->screen;

	if (subsystem->dxgiFrameInfo.AccumulatedFrames == 0)
		return 0;

	if (subsystem->dxgiFrameInfo.TotalMetadataBufferSize == 0)
		return 0;

	MetadataBufferSize = subsystem->dxgiFrameInfo.TotalMetadataBufferSize;

	if (MetadataBufferSize > subsystem->MetadataBufferSize)
	{
		subsystem->MetadataBuffer = (BYTE*) realloc(subsystem->MetadataBuffer, MetadataBufferSize);
			
		if (!subsystem->MetadataBuffer)
			return -1;

		subsystem->MetadataBufferSize = MetadataBufferSize;
	}

	/* GetFrameMoveRects */

	UsedBufferSize = 0;

	MoveRectsBufferSize = MetadataBufferSize - UsedBufferSize;
	pMoveRectBuffer = (DXGI_OUTDUPL_MOVE_RECT*) &(subsystem->MetadataBuffer[UsedBufferSize]);

	hr = subsystem->dxgiOutputDuplication->lpVtbl->GetFrameMoveRects(subsystem->dxgiOutputDuplication,
		MoveRectsBufferSize, pMoveRectBuffer, &MoveRectsBufferSize);

	if (FAILED(hr))
		return -1;

	/* GetFrameDirtyRects */

	UsedBufferSize += MoveRectsBufferSize;

	DirtyRectsBufferSize = MetadataBufferSize - UsedBufferSize;
	pDirtyRectsBuffer = (RECT*) &(subsystem->MetadataBuffer[UsedBufferSize]);

	hr = subsystem->dxgiOutputDuplication->lpVtbl->GetFrameDirtyRects(subsystem->dxgiOutputDuplication,
		DirtyRectsBufferSize, pDirtyRectsBuffer, &DirtyRectsBufferSize);

	if (FAILED(hr))
		return -1;

	/* GetFramePointerShape */

	UsedBufferSize += MoveRectsBufferSize;

	PointerShapeBufferSize = MetadataBufferSize - UsedBufferSize;
	pPointerShapeBuffer = (void*) &(subsystem->MetadataBuffer[UsedBufferSize]);

	hr = subsystem->dxgiOutputDuplication->lpVtbl->GetFramePointerShape(subsystem->dxgiOutputDuplication,
		PointerShapeBufferSize, pPointerShapeBuffer, &PointerShapeBufferSize, &PointerShapeInfo);

	if (FAILED(hr))
		return -1;

	EnterCriticalSection(&(screen->lock));

	numMoveRects = MoveRectsBufferSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);

	for (i = 0; i < numMoveRects; i++)
	{
		pMoveRect = &pMoveRectBuffer[i];
		pSrcPt = &(pMoveRect->SourcePoint);
		pDstRect = &(pMoveRect->DestinationRect);

		invalidRect.left = (UINT16) pDstRect->left;
		invalidRect.top = (UINT16) pDstRect->top;
		invalidRect.right = (UINT16) pDstRect->right;
		invalidRect.bottom = (UINT16) pDstRect->bottom;

		region16_union_rect(&(screen->invalidRegion), &(screen->invalidRegion), &invalidRect);
	}

	numDirtyRects = DirtyRectsBufferSize / sizeof(RECT);

	for (i = 0; i < numDirtyRects; i++)
	{
		pDirtyRect = &pDirtyRectsBuffer[i];

		invalidRect.left = (UINT16) pDirtyRect->left;
		invalidRect.top = (UINT16) pDirtyRect->top;
		invalidRect.right = (UINT16) pDirtyRect->right;
		invalidRect.bottom = (UINT16) pDirtyRect->bottom;

		region16_union_rect(&(screen->invalidRegion), &(screen->invalidRegion), &invalidRect);
	}

	LeaveCriticalSection(&(screen->lock));

	return 1;
}

#endif

void win_shadow_input_synchronize_event(winShadowSubsystem* subsystem, UINT32 flags)
{

}

void win_shadow_input_keyboard_event(winShadowSubsystem* subsystem, UINT16 flags, UINT16 code)
{
	INPUT event;

	event.type = INPUT_KEYBOARD;
	event.ki.wVk = 0;
	event.ki.wScan = code;
	event.ki.dwFlags = KEYEVENTF_SCANCODE;
	event.ki.dwExtraInfo = 0;
	event.ki.time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		event.ki.dwFlags |= KEYEVENTF_KEYUP;

	if (flags & KBD_FLAGS_EXTENDED)
		event.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;

	SendInput(1, &event, sizeof(INPUT));
}

void win_shadow_input_unicode_keyboard_event(winShadowSubsystem* subsystem, UINT16 flags, UINT16 code)
{
	INPUT event;

	event.type = INPUT_KEYBOARD;
	event.ki.wVk = 0;
	event.ki.wScan = code;
	event.ki.dwFlags = KEYEVENTF_UNICODE;
	event.ki.dwExtraInfo = 0;
	event.ki.time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		event.ki.dwFlags |= KEYEVENTF_KEYUP;

	SendInput(1, &event, sizeof(INPUT));
}

void win_shadow_input_mouse_event(winShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{
	INPUT event;
	float width;
	float height;

	ZeroMemory(&event, sizeof(INPUT));

	event.type = INPUT_MOUSE;

	if (flags & PTR_FLAGS_WHEEL)
	{
		event.mi.dwFlags = MOUSEEVENTF_WHEEL;
		event.mi.mouseData = flags & WheelRotationMask;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			event.mi.mouseData *= -1;

		SendInput(1, &event, sizeof(INPUT));
	}
	else
	{
		width = (float) GetSystemMetrics(SM_CXSCREEN);
		height = (float) GetSystemMetrics(SM_CYSCREEN);

		event.mi.dx = (LONG) ((float) x * (65535.0f / width));
		event.mi.dy = (LONG) ((float) y * (65535.0f / height));
		event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_MOVE)
		{
			event.mi.dwFlags |= MOUSEEVENTF_MOVE;
			SendInput(1, &event, sizeof(INPUT));
		}

		event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_BUTTON1)
		{
			if (flags & PTR_FLAGS_DOWN)
				event.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
			else
				event.mi.dwFlags |= MOUSEEVENTF_LEFTUP;

			SendInput(1, &event, sizeof(INPUT));
		}
		else if (flags & PTR_FLAGS_BUTTON2)
		{
			if (flags & PTR_FLAGS_DOWN)
				event.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
			else
				event.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;

			SendInput(1, &event, sizeof(INPUT));
		}
		else if (flags & PTR_FLAGS_BUTTON3)
		{
			if (flags & PTR_FLAGS_DOWN)
				event.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
			else
				event.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;

			SendInput(1, &event, sizeof(INPUT));
		}
	}
}

void win_shadow_input_extended_mouse_event(winShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{
	INPUT event;
	float width;
	float height;

	ZeroMemory(&event, sizeof(INPUT));

	if ((flags & PTR_XFLAGS_BUTTON1) || (flags & PTR_XFLAGS_BUTTON2))
	{
		event.type = INPUT_MOUSE;

		if (flags & PTR_FLAGS_MOVE)
		{
			width = (float) GetSystemMetrics(SM_CXSCREEN);
			height = (float) GetSystemMetrics(SM_CYSCREEN);

			event.mi.dx = (LONG)((float) x * (65535.0f / width));
			event.mi.dy = (LONG)((float) y * (65535.0f / height));
			event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

			SendInput(1, &event, sizeof(INPUT));
		}

		event.mi.dx = event.mi.dy = event.mi.dwFlags = 0;

		if (flags & PTR_XFLAGS_DOWN)
			event.mi.dwFlags |= MOUSEEVENTF_XDOWN;
		else
			event.mi.dwFlags |= MOUSEEVENTF_XUP;

		if (flags & PTR_XFLAGS_BUTTON1)
			event.mi.mouseData = XBUTTON1;
		else if (flags & PTR_XFLAGS_BUTTON2)
			event.mi.mouseData = XBUTTON2;

		SendInput(1, &event, sizeof(INPUT));
	}
}


int win_shadow_invalidate_region(winShadowSubsystem* subsystem, int x, int y, int width, int height)
{
	rdpShadowServer* server;
	rdpShadowScreen* screen;
	RECTANGLE_16 invalidRect;

	server = subsystem->server;
	screen = server->screen;

	invalidRect.left = x;
	invalidRect.top = y;
	invalidRect.right = x + width;
	invalidRect.bottom = y + height;

	EnterCriticalSection(&(screen->lock));
	region16_union_rect(&(screen->invalidRegion), &(screen->invalidRegion), &invalidRect);
	LeaveCriticalSection(&(screen->lock));

	return 1;
}

int win_shadow_surface_copy(winShadowSubsystem* subsystem)
{
	int x, y;
	int width;
	int height;
	int status;
	int nDstStep = 0;
	BYTE* pDstData = NULL;
	rdpShadowScreen* screen;
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	RECTANGLE_16 surfaceRect;
	const RECTANGLE_16* extents;

	server = subsystem->server;
	surface = server->surface;
	screen = server->screen;

	surfaceRect.left = surface->x;
	surfaceRect.top = surface->y;
	surfaceRect.right = surface->x + surface->width;
	surfaceRect.bottom = surface->y + surface->height;

	region16_clear(&(surface->invalidRegion));
	region16_intersect_rect(&(surface->invalidRegion), &(screen->invalidRegion), &surfaceRect);

	if (region16_is_empty(&(surface->invalidRegion)))
		return 1;

	extents = region16_extents(&(surface->invalidRegion));

	x = extents->left;
	y = extents->top;
	width = extents->right - extents->left;
	height = extents->bottom - extents->top;

	printf("x: %d y: %d width: %d height: %d right: %d bottom: %d\n",
		x, y, width, height, x + width, y + height);

	status = win_shadow_dxgi_fetch_frame_data(subsystem, &pDstData, &nDstStep, x, y, width, height);

	if (status < 0)
		return -1;

	freerdp_image_copy(surface->data, PIXEL_FORMAT_XRGB32,
			surface->scanline, x - surface->x, y - surface->y, width, height,
			pDstData, PIXEL_FORMAT_XRGB32, nDstStep, x, y);

	return 1;
}

void* win_shadow_subsystem_thread(winShadowSubsystem* subsystem)
{
	int fps;
	DWORD status;
	DWORD nCount;
	UINT64 cTime;
	DWORD dwTimeout;
	DWORD dwInterval;
	UINT64 frameTime;
	HANDLE events[32];
	HANDLE StopEvent;

	StopEvent = subsystem->server->StopEvent;

	nCount = 0;
	events[nCount++] = StopEvent;

	fps = 16;
	dwInterval = 1000 / fps;
	frameTime = GetTickCount64() + dwInterval;

	win_shadow_invalidate_region(subsystem, 0, 0, subsystem->width, subsystem->height);

	while (1)
	{
		dwTimeout = INFINITE;

		cTime = GetTickCount64();
		dwTimeout = (DWORD) ((cTime > frameTime) ? 0 : frameTime - cTime);

		status = WaitForMultipleObjects(nCount, events, FALSE, dwTimeout);

		if (WaitForSingleObject(StopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		if ((status == WAIT_TIMEOUT) || (GetTickCount64() > frameTime))
		{
#ifdef WITH_DXGI_1_2
			int dxgi_status;

			dxgi_status = win_shadow_dxgi_get_next_frame(subsystem);

			if (subsystem->pendingFrames > 0)
			{
				dxgi_status = win_shadow_dxgi_get_invalid_region(subsystem);

				win_shadow_invalidate_region(subsystem, 0, 0, subsystem->width, subsystem->height);
			}
#endif

			dwInterval = 1000 / fps;
			frameTime += dwInterval;
		}
	}

	ExitThread(0);
	return NULL;
}

int win_shadow_subsystem_init(winShadowSubsystem* subsystem)
{
	HDC hdc;
	int status;
	DWORD iDevNum = 0;
	MONITOR_DEF* virtualScreen;
	DISPLAY_DEVICE DisplayDevice;

	ZeroMemory(&DisplayDevice, sizeof(DISPLAY_DEVICE));
	DisplayDevice.cb = sizeof(DISPLAY_DEVICE);

	if (!EnumDisplayDevices(NULL, iDevNum, &DisplayDevice, 0))
		return -1;

	hdc = CreateDC(DisplayDevice.DeviceName, NULL, NULL, NULL);

	subsystem->width = GetDeviceCaps(hdc, HORZRES);
	subsystem->height = GetDeviceCaps(hdc, VERTRES);
	subsystem->bpp = GetDeviceCaps(hdc, BITSPIXEL);

	DeleteDC(hdc);

#ifdef WITH_DXGI_1_2
	status = win_shadow_dxgi_init(subsystem);
#endif

	virtualScreen = &(subsystem->virtualScreen);

	virtualScreen->left = 0;
	virtualScreen->top = 0;
	virtualScreen->right = subsystem->width;
	virtualScreen->bottom = subsystem->height;
	virtualScreen->flags = 1;

	if (subsystem->monitorCount < 1)
	{
		subsystem->monitorCount = 1;
		subsystem->monitors[0].left = virtualScreen->left;
		subsystem->monitors[0].top = virtualScreen->top;
		subsystem->monitors[0].right = virtualScreen->right;
		subsystem->monitors[0].bottom = virtualScreen->bottom;
		subsystem->monitors[0].flags = 1;
	}

	printf("width: %d height: %d\n", subsystem->width, subsystem->height);

	return 1;
}

int win_shadow_subsystem_uninit(winShadowSubsystem* subsystem)
{
	if (!subsystem)
		return -1;

#ifdef WITH_DXGI_1_2
	win_shadow_dxgi_uninit(subsystem);
#endif

	return 1;
}

int win_shadow_subsystem_start(winShadowSubsystem* subsystem)
{
	HANDLE thread;

	if (!subsystem)
		return -1;

	thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) win_shadow_subsystem_thread,
			(void*) subsystem, 0, NULL);

	return 1;
}

int win_shadow_subsystem_stop(winShadowSubsystem* subsystem)
{
	if (!subsystem)
		return -1;

	return 1;
}

void win_shadow_subsystem_free(winShadowSubsystem* subsystem)
{
	if (!subsystem)
		return;

	win_shadow_subsystem_uninit(subsystem);

	free(subsystem);
}

winShadowSubsystem* win_shadow_subsystem_new(rdpShadowServer* server)
{
	winShadowSubsystem* subsystem;

	subsystem = (winShadowSubsystem*) calloc(1, sizeof(winShadowSubsystem));

	if (!subsystem)
		return NULL;

	subsystem->server = server;

	subsystem->Init = (pfnShadowSubsystemInit) win_shadow_subsystem_init;
	subsystem->Uninit = (pfnShadowSubsystemInit) win_shadow_subsystem_uninit;
	subsystem->Start = (pfnShadowSubsystemStart) win_shadow_subsystem_start;
	subsystem->Stop = (pfnShadowSubsystemStop) win_shadow_subsystem_stop;
	subsystem->Free = (pfnShadowSubsystemFree) win_shadow_subsystem_free;

	subsystem->SurfaceCopy = (pfnShadowSurfaceCopy) win_shadow_surface_copy;

	subsystem->SynchronizeEvent = (pfnShadowSynchronizeEvent) win_shadow_input_synchronize_event;
	subsystem->KeyboardEvent = (pfnShadowKeyboardEvent) win_shadow_input_keyboard_event;
	subsystem->UnicodeKeyboardEvent = (pfnShadowUnicodeKeyboardEvent) win_shadow_input_unicode_keyboard_event;
	subsystem->MouseEvent = (pfnShadowMouseEvent) win_shadow_input_mouse_event;
	subsystem->ExtendedMouseEvent = (pfnShadowExtendedMouseEvent) win_shadow_input_extended_mouse_event;

	return subsystem;
}

rdpShadowSubsystem* Win_ShadowCreateSubsystem(rdpShadowServer* server)
{
	return (rdpShadowSubsystem*) win_shadow_subsystem_new(server);
}
