/**
 * WinPR: Windows Portable Runtime
 * User Environment
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef WINPR_USER_H
#define WINPR_USER_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/platform.h>

/**
 * Standard Clipboard Formats
 */

#ifndef _WIN32

#define MB_OK					0x00000000L
#define MB_OKCANCEL				0x00000001L
#define MB_ABORTRETRYIGNORE		0x00000002L
#define MB_YESNOCANCEL			0x00000003L
#define MB_YESNO				0x00000004L
#define MB_RETRYCANCEL			0x00000005L
#define MB_CANCELTRYCONTINUE	0x00000006L

#define IDOK					1
#define IDCANCEL				2
#define IDABORT					3
#define IDRETRY					4
#define IDIGNORE				5
#define IDYES					6
#define IDNO					7
#define IDTRYAGAIN				10
#define IDCONTINUE				11
#define IDTIMEOUT				32000
#define IDASYNC					32001

#define CF_RAW			0
#define CF_TEXT			1
#define CF_BITMAP		2
#define CF_METAFILEPICT		3
#define CF_SYLK			4
#define CF_DIF			5
#define CF_TIFF			6
#define CF_OEMTEXT		7
#define CF_DIB			8
#define CF_PALETTE		9
#define CF_PENDATA		10
#define CF_RIFF			11
#define CF_WAVE			12
#define CF_UNICODETEXT		13
#define CF_ENHMETAFILE		14
#define CF_HDROP		15
#define CF_LOCALE		16
#define CF_DIBV5		17
#define CF_MAX			18

#define CF_OWNERDISPLAY		0x0080
#define CF_DSPTEXT		0x0081
#define CF_DSPBITMAP		0x0082
#define CF_DSPMETAFILEPICT	0x0083
#define CF_DSPENHMETAFILE	0x008E

#define CF_PRIVATEFIRST		0x0200
#define CF_PRIVATELAST		0x02FF

#define CF_GDIOBJFIRST		0x0300
#define CF_GDIOBJLAST		0x03FF

/* Windows Metafile Picture Format */

#define MM_TEXT			1
#define MM_LOMETRIC		2
#define MM_HIMETRIC		3
#define MM_LOENGLISH		4
#define MM_HIENGLISH		5
#define MM_TWIPS		6
#define MM_ISOTROPIC		7
#define MM_ANISOTROPIC		8

#define MM_MIN			MM_TEXT
#define MM_MAX			MM_ANISOTROPIC
#define MM_MAX_FIXEDSCALE	MM_TWIPS

#endif

/**
 * Bitmap Definitions
 */

#if !defined(_WIN32)

#pragma pack(push, 1)

typedef LONG FXPT16DOT16, FAR *LPFXPT16DOT16;
typedef LONG FXPT2DOT30, FAR *LPFXPT2DOT30;

typedef struct tagCIEXYZ
{
	FXPT2DOT30 ciexyzX;
	FXPT2DOT30 ciexyzY;
	FXPT2DOT30 ciexyzZ;
} CIEXYZ;

typedef CIEXYZ FAR *LPCIEXYZ;

typedef struct tagICEXYZTRIPLE
{
	CIEXYZ ciexyzRed;
	CIEXYZ ciexyzGreen;
	CIEXYZ ciexyzBlue;
} CIEXYZTRIPLE;

typedef CIEXYZTRIPLE FAR *LPCIEXYZTRIPLE;

typedef struct tagBITMAP
{
	LONG bmType;
	LONG bmWidth;
	LONG bmHeight;
	LONG bmWidthBytes;
	WORD bmPlanes;
	WORD bmBitsPixel;
	LPVOID bmBits;
} BITMAP, *PBITMAP, NEAR *NPBITMAP, FAR *LPBITMAP;

typedef struct tagRGBTRIPLE
{
	BYTE rgbtBlue;
	BYTE rgbtGreen;
	BYTE rgbtRed;
} RGBTRIPLE, *PRGBTRIPLE, NEAR *NPRGBTRIPLE, FAR *LPRGBTRIPLE;

typedef struct tagRGBQUAD
{
	BYTE rgbBlue;
	BYTE rgbGreen;
	BYTE rgbRed;
	BYTE rgbReserved;
} RGBQUAD;

typedef RGBQUAD FAR* LPRGBQUAD;

#define BI_RGB			0
#define BI_RLE8			1
#define BI_RLE4			2
#define BI_BITFIELDS		3
#define BI_JPEG			4
#define BI_PNG			5

#define PROFILE_LINKED          'LINK'
#define PROFILE_EMBEDDED        'MBED'

typedef struct tagBITMAPINFOHEADER
{
	DWORD biSize;
	LONG biWidth;
	LONG biHeight;
	WORD biPlanes;
	WORD biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG biXPelsPerMeter;
	LONG biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
} BITMAPINFOHEADER, FAR *LPBITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct tagBITMAPINFO
{
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD bmiColors[1];
} BITMAPINFO, FAR *LPBITMAPINFO, *PBITMAPINFO;

typedef enum _ORIENTATION_PREFERENCE
{
	ORIENTATION_PREFERENCE_NONE = 0x0,
	ORIENTATION_PREFERENCE_LANDSCAPE = 0x1,

	ORIENTATION_PREFERENCE_PORTRAIT	= 0x2,
	ORIENTATION_PREFERENCE_LANDSCAPE_FLIPPED = 0x4,
	ORIENTATION_PREFERENCE_PORTRAIT_FLIPPED = 0x8
} ORIENTATION_PREFERENCE;

#pragma pack(pop)

#endif

#if !defined(_WIN32) || defined(_UWP)

#pragma pack(push, 1)

typedef struct tagBITMAPCOREHEADER
{
	DWORD bcSize;
	WORD bcWidth;
	WORD bcHeight;
	WORD bcPlanes;
	WORD bcBitCount;
} BITMAPCOREHEADER, FAR *LPBITMAPCOREHEADER, *PBITMAPCOREHEADER;

typedef struct
{
	DWORD bV4Size;
	LONG bV4Width;
	LONG bV4Height;
	WORD bV4Planes;
	WORD bV4BitCount;
	DWORD bV4V4Compression;
	DWORD bV4SizeImage;
	LONG bV4XPelsPerMeter;
	LONG bV4YPelsPerMeter;
	DWORD bV4ClrUsed;
	DWORD bV4ClrImportant;
	DWORD bV4RedMask;
	DWORD bV4GreenMask;
	DWORD bV4BlueMask;
	DWORD bV4AlphaMask;
	DWORD bV4CSType;
	CIEXYZTRIPLE bV4Endpoints;
	DWORD bV4GammaRed;
	DWORD bV4GammaGreen;
	DWORD bV4GammaBlue;
} BITMAPV4HEADER, FAR *LPBITMAPV4HEADER, *PBITMAPV4HEADER;

typedef struct
{
	DWORD bV5Size;
	LONG bV5Width;
	LONG bV5Height;
	WORD bV5Planes;
	WORD bV5BitCount;
	DWORD bV5Compression;
	DWORD bV5SizeImage;
	LONG bV5XPelsPerMeter;
	LONG bV5YPelsPerMeter;
	DWORD bV5ClrUsed;
	DWORD bV5ClrImportant;
	DWORD bV5RedMask;
	DWORD bV5GreenMask;
	DWORD bV5BlueMask;
	DWORD bV5AlphaMask;
	DWORD bV5CSType;
	CIEXYZTRIPLE bV5Endpoints;
	DWORD bV5GammaRed;
	DWORD bV5GammaGreen;
	DWORD bV5GammaBlue;
	DWORD bV5Intent;
	DWORD bV5ProfileData;
	DWORD bV5ProfileSize;
	DWORD bV5Reserved;
} BITMAPV5HEADER, FAR *LPBITMAPV5HEADER, *PBITMAPV5HEADER;

typedef struct tagBITMAPCOREINFO
{
	BITMAPCOREHEADER bmciHeader;
	RGBTRIPLE bmciColors[1];
} BITMAPCOREINFO, FAR *LPBITMAPCOREINFO, *PBITMAPCOREINFO;

typedef struct tagBITMAPFILEHEADER
{
	WORD bfType;
	DWORD bfSize;
	WORD bfReserved1;
	WORD bfReserved2;
	DWORD bfOffBits;
} BITMAPFILEHEADER, FAR *LPBITMAPFILEHEADER, *PBITMAPFILEHEADER;

#pragma pack(pop)

#endif

#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}
#endif

#endif /* WINPR_USER_H */

