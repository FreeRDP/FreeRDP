/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Windows Server
 *
 * Copyright 2012 Corey Clayton <can.of.tuna@gmail.com>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CINTERFACE

#include <D3D11.h>
#include <dxgi1_2.h>

#include <tchar.h>
#include "wf_dxgi.h"

// Driver types supported
D3D_DRIVER_TYPE DriverTypes[] =
{
	D3D_DRIVER_TYPE_HARDWARE,
	D3D_DRIVER_TYPE_WARP,
	D3D_DRIVER_TYPE_REFERENCE,
};
UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

// Feature levels supported
D3D_FEATURE_LEVEL FeatureLevels[] =
{
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_1
};
UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);

D3D_FEATURE_LEVEL FeatureLevel;

ID3D11Device* MeinDevice = NULL;
ID3D11DeviceContext* MeinContext = NULL;
IDXGIOutputDuplication* MeinDeskDupl = NULL;
ID3D11Texture2D* MeinAcquiredDesktopImage = NULL;

IDXGISurface* surf;
ID3D11Texture2D * sStage;

DXGI_OUTDUPL_FRAME_INFO FrameInfo;


int wf_dxgi_init(wfInfo* context)
{
	HRESULT hr;

	UINT DriverTypeIndex;
	IDXGIDevice* DxgiDevice = NULL;
	IDXGIAdapter* DxgiAdapter = NULL;
	DXGI_OUTPUT_DESC desc;
	UINT dTop, i = 0;
	IDXGIOutput * pOutput;

	IDXGIOutput* DxgiOutput = NULL;

	IDXGIOutput1* DxgiOutput1 = NULL;


	/////////////////////////////////////////////////////////

	//guessing this must be null
	MeinAcquiredDesktopImage = NULL;


	_tprintf(_T("Hallo, welt!\n"));

	_tprintf(_T("Trying to create a DX11 Device...\n"));
	// Create device
	for (DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex)
	{
		hr = D3D11CreateDevice(NULL, DriverTypes[DriverTypeIndex], NULL, D3D11_CREATE_DEVICE_DEBUG, FeatureLevels, NumFeatureLevels,
								D3D11_SDK_VERSION, &MeinDevice, &FeatureLevel, &MeinContext);
		if (SUCCEEDED(hr))
		{
			// Device creation success, no need to loop anymore
			break;
		}
	}
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to create device in InitializeDx\n"));
		return 1;
	}
	_tprintf(_T("Gut!\n"));

	///////////////////////////////////////////////////////

	
	_tprintf(_T("Trying to get QI for DXGI Device...\n"));
	
	hr = MeinDevice->lpVtbl->QueryInterface(MeinDevice, &IID_IDXGIDevice, (void**) &DxgiDevice);
	if (FAILED(hr))
    {
		_tprintf(_T("Failed to get QI for DXGI Device\n"));
        return 1;
    }
	_tprintf(_T("Gut!\n"));

	//////////////////////////////////////////////////////////

	_tprintf(_T("Trying to get adapter for DXGI Device...\n"));
	
	hr = DxgiDevice->lpVtbl->GetParent(DxgiDevice, &IID_IDXGIAdapter, (void**) &DxgiAdapter);
	DxgiDevice->lpVtbl->Release(DxgiDevice);
    DxgiDevice = NULL;
    if (FAILED(hr))
    {
        _tprintf(_T("Failed to get parent DXGI Adapter\n"));
		return 1;
    }
	_tprintf(_T("Gut!\n"));
	
	////////////////////////////////////////////////////////////

	
	memset(&desc, 0, sizeof(desc));
	pOutput = NULL;
	_tprintf(_T("\nLooping through ouputs on DXGI adapter...\n"));
	while(DxgiAdapter->lpVtbl->EnumOutputs(DxgiAdapter, i, &pOutput) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC* pDesc = &desc;

		hr = pOutput->lpVtbl->GetDesc(pOutput, pDesc);
		if (FAILED(hr))
		{
			_tprintf(_T("Failed to get description\n"));
			return 1;
		}

		_tprintf(_T("Output %d: [%s] [%d]\n"), i, pDesc->DeviceName, pDesc->AttachedToDesktop);

		if(pDesc->AttachedToDesktop)
			dTop = i;

		pOutput->lpVtbl->Release(pOutput);
		++i;
	}

	//for now stick to the first one
	dTop = 0;


	_tprintf(_T("\nTrying to get output...\n"));
    hr = DxgiAdapter->lpVtbl->EnumOutputs(DxgiAdapter, dTop, &DxgiOutput);
    DxgiAdapter->lpVtbl->Release(DxgiAdapter);
    DxgiAdapter = NULL;
    if (FAILED(hr))
    {
        _tprintf(_T("Failed to get output\n"));
		return 1;
	}
	_tprintf(_T("Gut!\n"));
	
	//////////////////////////////////////////////

	_tprintf(_T("Trying to get IDXGIOutput1...\n"));
	hr = DxgiOutput->lpVtbl->QueryInterface(DxgiOutput, &IID_IDXGIOutput1, (void**) &DxgiOutput1);
	DxgiOutput->lpVtbl->Release(DxgiOutput);
    DxgiOutput = NULL;
    if (FAILED(hr))
    {
         _tprintf(_T("Failed to get IDXGIOutput1\n"));
		return 1;
    }
	_tprintf(_T("Gut!\n"));
	
	//////////////////////////////////////////////


	_tprintf(_T("Trying to duplicate the output...\n"));
	hr = DxgiOutput1->lpVtbl->DuplicateOutput(DxgiOutput1, (IUnknown*)MeinDevice, &MeinDeskDupl);
	DxgiOutput1->lpVtbl->Release(DxgiOutput1);
    DxgiOutput1 = NULL;
    if (FAILED(hr))
    {
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
        {
             _tprintf(_T("There is already the maximum number of applications using the Desktop Duplication API running, please close one of those applications and then try again.\n"));
			return 1;
        }
		_tprintf(_T("Failed to get duplicate output\n"));
		return 1;
    }
	_tprintf(_T("Gut! Init Complete!\n"));

	return 0;
}

int wf_dxgi_cleanup(wfInfo* context)
{
	if(MeinAcquiredDesktopImage)
	{
		MeinAcquiredDesktopImage->lpVtbl->Release(MeinAcquiredDesktopImage);
		MeinAcquiredDesktopImage = NULL;
	}

	if(MeinDeskDupl)
	{
		MeinDeskDupl->lpVtbl->Release(MeinDeskDupl);
		MeinDeskDupl = NULL;
	}

	if(MeinContext)
	{
		MeinContext->lpVtbl->Release(MeinContext);
		MeinContext = NULL;
	}

	if(MeinDevice)
	{
		MeinDevice->lpVtbl->Release(MeinDevice);
		MeinDevice = NULL;
	}

	return 0;
}

int wf_dxgi_nextFrame(wfInfo* wfi, UINT timeout)
{
	HRESULT hr;
	IDXGIResource* DesktopResource = NULL;
	BYTE* MeinMetaDataBuffer = NULL;
	UINT MeinMetaDataSize = 0;
	UINT i = 0;

	if(wfi->framesWaiting > 0)
	{
		wf_dxgi_releasePixelData(wfi);
	}

	if(MeinAcquiredDesktopImage)
	{
		MeinAcquiredDesktopImage->lpVtbl->Release(MeinAcquiredDesktopImage);
		MeinAcquiredDesktopImage = NULL;
	}

	_tprintf(_T("\nTrying to acquire a frame...\n"));
	hr = MeinDeskDupl->lpVtbl->AcquireNextFrame(MeinDeskDupl, timeout, &FrameInfo, &DesktopResource);
	_tprintf(_T("hr = %#0X\n"), hr);
	if (hr == DXGI_ERROR_WAIT_TIMEOUT)
	{
		_tprintf(_T("Timeout\n"));
		return 1;
	}
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to acquire next frame\n"));

		hr = MeinDeskDupl->lpVtbl->ReleaseFrame(MeinDeskDupl);
		if (FAILED(hr))
		{
			_tprintf(_T("Failed to release frame\n"));
		}
		return 1;
	}
	_tprintf(_T("Gut!\n"));

	///////////////////////////////////////////////

		
	_tprintf(_T("Trying to QI for ID3D11Texture2D...\n"));
	hr = DesktopResource->lpVtbl->QueryInterface(DesktopResource, &IID_ID3D11Texture2D, (void**) &MeinAcquiredDesktopImage);
	DesktopResource->lpVtbl->Release(DesktopResource);
	DesktopResource = NULL;
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to QI for ID3D11Texture2D from acquired IDXGIResource\n"));
			return 1;
	}
	_tprintf(_T("Gut!\n"));

	wfi->framesWaiting = FrameInfo.AccumulatedFrames;


	return 0;

}

int wf_dxgi_getPixelData(wfInfo* context, BYTE** data, int* pitch, RECT* invalid)
{
	HRESULT hr;
	DXGI_MAPPED_RECT MeinData;
	D3D11_TEXTURE2D_DESC tDesc;
	D3D11_BOX Box;

	tDesc.Width = (invalid->right - invalid->left) + 1;
	tDesc.Height = (invalid->bottom - invalid->top) + 1;
	tDesc.MipLevels = 1;
	tDesc.ArraySize = 1;
	tDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	tDesc.SampleDesc.Count = 1;
	tDesc.SampleDesc.Quality = 0;
	tDesc.Usage = D3D11_USAGE_STAGING;
	tDesc.BindFlags = 0;
	tDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;// | D3D11_CPU_ACCESS_WRITE;
	tDesc.MiscFlags = 0;

	Box.top = invalid->top;
	Box.left = invalid->left;
	Box.right = invalid->right;
	Box.bottom = invalid->bottom;
	Box.front = 0;
	Box.back = 1;

	_tprintf(_T("Trying to create staging surface\n"));
	hr = MeinDevice->lpVtbl->CreateTexture2D(MeinDevice, &tDesc, NULL, &sStage);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to create staging surface\n"));
		exit(1);
		return 1;
	}
	_tprintf(_T("Gut!\n"));

	MeinContext->lpVtbl->CopySubresourceRegion(MeinContext, (ID3D11Resource*)sStage, 0,0,0,0, (ID3D11Resource*)MeinAcquiredDesktopImage, 0, &Box);	 
		
	_tprintf(_T("Trying to QI staging surface\n"));
	hr = sStage->lpVtbl->QueryInterface(sStage, &IID_IDXGISurface, (void**)&surf);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to QI staging surface\n"));
		exit(1);
		return 1;
	}
	_tprintf(_T("Gut!\n"));

	_tprintf(_T("Trying to map staging surface\n"));
	surf->lpVtbl->Map(surf, &MeinData, DXGI_MAP_READ);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to map staging surface\n"));
		exit(1);
		return 1;
	}
	_tprintf(_T("Gut!\n"));
		
	//access pixel data 

	*data = MeinData.pBits;
	*pitch = MeinData.Pitch;

	return 0;
}

int wf_dxgi_releasePixelData(wfInfo* wfi)
{
	HRESULT hr;

	surf->lpVtbl->Unmap(surf);
	surf->lpVtbl->Release(surf);
	surf = NULL;
	sStage->lpVtbl->Release(sStage);
	sStage = NULL;

	hr = MeinDeskDupl->lpVtbl->ReleaseFrame(MeinDeskDupl);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to release frame\n"));
		return 1;
	}

	wfi->framesWaiting = 0;
	_tprintf(_T("PixelData Release\n"));
	return 0;
}

int wf_dxgi_getInvalidRegion(RECT* invalid)
{
	HRESULT hr;
	UINT MeinMetaDataSize = 0;
	UINT BufSize;
	UINT i;
	BYTE* DirtyRects;
	UINT dirty;
	RECT* pRect;

	//optimization note: make this buffer global and allocate only once (or grow only when needed)
	BYTE* MeinMetaDataBuffer = NULL;

	if(FrameInfo.AccumulatedFrames == 0)
	{
		//we dont care
		return 1;
	}

	_tprintf(_T("FrameInfo\n"));
	_tprintf(_T("\tAccumulated Frames: %d\n"), FrameInfo.AccumulatedFrames);
	_tprintf(_T("\tCoalesced Rectangles: %d\n"), FrameInfo.RectsCoalesced);
	_tprintf(_T("\tMetadata buffer size: %d\n"), FrameInfo.TotalMetadataBufferSize);


	if(FrameInfo.TotalMetadataBufferSize)
	{

		if (FrameInfo.TotalMetadataBufferSize > MeinMetaDataSize)
		{
			if (MeinMetaDataBuffer)
			{
				free(MeinMetaDataBuffer);
				MeinMetaDataBuffer = NULL;
			}
			MeinMetaDataBuffer = (BYTE*) malloc(FrameInfo.TotalMetadataBufferSize);
			if (!MeinMetaDataBuffer)
			{
				MeinMetaDataSize = 0;
				_tprintf(_T("Failed to allocate memory for metadata\n"));
				exit(1);
			}
			MeinMetaDataSize = FrameInfo.TotalMetadataBufferSize;
		}

		BufSize = FrameInfo.TotalMetadataBufferSize;

		// Get move rectangles
		hr = MeinDeskDupl->lpVtbl->GetFrameMoveRects(MeinDeskDupl, BufSize, (DXGI_OUTDUPL_MOVE_RECT*) MeinMetaDataBuffer, &BufSize);
		if (FAILED(hr))
		{
			_tprintf(_T("Failed to get frame move rects\n"));
			return 1;
		}
		_tprintf(_T("Move rects: %d\n"), BufSize / sizeof(DXGI_OUTDUPL_MOVE_RECT));

		DirtyRects = MeinMetaDataBuffer + BufSize;
		BufSize = FrameInfo.TotalMetadataBufferSize - BufSize;

			// Get dirty rectangles
		hr = MeinDeskDupl->lpVtbl->GetFrameDirtyRects(MeinDeskDupl, BufSize, (RECT*) DirtyRects, &BufSize);
		if (FAILED(hr))
		{
			_tprintf(_T("Failed to get frame dirty rects\n"));
			return 1;
		}
		dirty = BufSize / sizeof(RECT);
		_tprintf(_T("Dirty rects: %d\n"), dirty);

		pRect = (RECT*) DirtyRects;
		for(i = 0; i<dirty; ++i)
		{
			_tprintf(_T("\tRect: (%d, %d), (%d, %d)\n"),
				pRect->left,
				pRect->top,
				pRect->right,
				pRect->bottom);

			UnionRect(invalid, invalid, pRect);

			++pRect;
		}
	}

	return 0;
}

