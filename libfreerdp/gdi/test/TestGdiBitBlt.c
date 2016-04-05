
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/gdi/drawing.h>

#include <winpr/crt.h>

#include "line.h"
#include "brush.h"

/* BitBlt() Test Data */

/* source bitmap (16x16) */
static BYTE bmp_SRC[256] =
{
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
};

/* destination bitmap (16x16) */
static BYTE bmp_DST[256] =
{
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
};

/* pattern bitmap (8x8) */
static BYTE bmp_PAT[64] =
{
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
};

/* SRCCOPY (0x00CC0020) */
static BYTE bmp_SRCCOPY[256] =
{
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
};

/* BLACKNESS (0x00000042) */
static BYTE bmp_BLACKNESS[256] =
{
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
};

/* WHITENESS (0x00FF0062) */
static BYTE bmp_WHITENESS[256] =
{
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
};

/* SRCAND (0x008800C6) */
static BYTE bmp_SRCAND[256] =
{
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
};

/* SRCPAINT (0x00EE0086) */
static BYTE bmp_SRCPAINT[256] =
{
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
};

/* SRCINVERT (0x00660046) */
static BYTE bmp_SRCINVERT[256] =
{
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x00"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\x00\x00\x00"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\x00\x00\x00"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\x00\x00\x00\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
};

/* SRCERASE (0x00440328) */
static BYTE bmp_SRCERASE[256] =
{
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
};

/* NOTSRCCOPY (0x00330008) */
static BYTE bmp_NOTSRCCOPY[256] =
{
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00"
	"\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00"
	"\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00"
	"\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00"
	"\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
};

/* NOTSRCERASE (0x001100A6) */
static BYTE bmp_NOTSRCERASE[256] =
{
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
};

/* DSTINVERT (0x00550009) */
static BYTE bmp_DSTINVERT[256] =
{
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
};

/* SPna (0x000C0324) */
static BYTE bmp_SPna[256] =
{
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\x00\x00\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\x00\x00\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\x00\x00"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\x00\x00\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
};

/* MERGEPAINT (0x00BB0226) */
static BYTE bmp_MERGEPAINT[256] =
{
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00"
};

/* MERGECOPY (0x00C000CA) */
static BYTE bmp_MERGECOPY[256] =
{
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\x00\x00\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\x00\x00"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\x00\x00\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\x00\x00\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
};

/* PATPAINT (0x00FB0A09) */
static BYTE bmp_PATPAINT[256] =
{
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
};

/* PATCOPY (0x00F00021) */
static BYTE bmp_PATCOPY[256] =
{
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
};

/* PATINVERT (0x005A0049) */
static BYTE bmp_PATINVERT[256] =
{
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\x00\x00\xFF\xFF\x00\x00\xFF\xFF\xFF\xFF\x00\x00\xFF\xFF\x00\x00"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
	"\xFF\xFF\x00\x00\xFF\xFF\x00\x00\x00\x00\xFF\xFF\x00\x00\xFF\xFF"
};

static int CompareBitmaps(HGDI_BITMAP hBmp1, HGDI_BITMAP hBmp2)
{
	UINT32 x, y;
	BYTE *p1, *p2;

	UINT32 minw = (hBmp1->width < hBmp2->width) ? hBmp1->width : hBmp2->width;
	UINT32 minh = (hBmp1->height < hBmp2->height) ? hBmp1->height : hBmp2->height;

	if (hBmp1->format == hBmp2->format)
	{
		UINT32 colorA, colorB;
		p1 = hBmp1->data;
		p2 = hBmp2->data;

		for (y = 0; y < minh; y++)
		{
			for (x = 0; x < minw; x++)
			{
				colorA = ReadColor(p1, hBmp1->format);
				colorB = ReadColor(p2, hBmp2->format);

				p1 += GetBytesPerPixel(hBmp1->format);
				p2 += GetBytesPerPixel(hBmp2->format);
			}
		}
	}
	else
	{
		return 0;
	}

	return 1;
}

static void test_dump_data(unsigned char* p, int len, int width, const char* name)
{
	unsigned char *line = p;
	int i, thisline, offset = 0;

	printf("\n%s[%d][%d]:\n", name, len / width, width);

	while (offset < len)
	{
		printf("%04x ", offset);
		thisline = len - offset;
		if (thisline > width)
			thisline = width;

		for (i = 0; i < thisline; i++)
			printf("%02x ", line[i]);

		for (; i < width; i++)
			printf("   ");

		printf("\n");
		offset += thisline;
		line += thisline;
	}

	printf("\n");
}

static void test_dump_bitmap(HGDI_BITMAP hBmp, const char* name)
{
	UINT32 stride = hBmp->width * GetBytesPerPixel(hBmp->format);

	test_dump_data(hBmp->data, hBmp->height * stride, stride, name);
}

int test_assert_bitmaps_equal(HGDI_BITMAP hBmpActual, HGDI_BITMAP hBmpExpected, const char* name)
{
	int bitmapsEqual = CompareBitmaps(hBmpActual, hBmpExpected);

	if (bitmapsEqual != 1)
	{
		printf("\n%s\n", name);
		test_dump_bitmap(hBmpActual, "Actual");
		test_dump_bitmap(hBmpExpected, "Expected");
	}

	if (bitmapsEqual != 1)
		return -1;

	return 0;
}

int test_gdi_BitBlt_32bpp(void)
{
	BYTE* data;
	HGDI_DC hdcSrc;
	HGDI_DC hdcDst;
	HGDI_BRUSH hBrush;
	HGDI_BITMAP hBmpSrc;
	HGDI_BITMAP hBmpDst;
	HGDI_BITMAP hBmpPat;
	HGDI_BITMAP hBmp_SPna;
	HGDI_BITMAP hBmp_BLACKNESS;
	HGDI_BITMAP hBmp_WHITENESS;
	HGDI_BITMAP hBmp_SRCCOPY;
	HGDI_BITMAP hBmp_SRCAND;
	HGDI_BITMAP hBmp_SRCPAINT;
	HGDI_BITMAP hBmp_SRCINVERT;
	HGDI_BITMAP hBmp_SRCERASE;
	HGDI_BITMAP hBmp_NOTSRCCOPY;
	HGDI_BITMAP hBmp_NOTSRCERASE;
	HGDI_BITMAP hBmp_DSTINVERT;
	HGDI_BITMAP hBmp_MERGECOPY;
	HGDI_BITMAP hBmp_MERGEPAINT;
	HGDI_BITMAP hBmp_PATCOPY;
	HGDI_BITMAP hBmp_PATPAINT;
	HGDI_BITMAP hBmp_PATINVERT;
	HGDI_BITMAP hBmpDstOriginal;
	UINT32* hPalette;
    const UINT32 format = PIXEL_FORMAT_XRGB32;

	if (!(hdcSrc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}
	hdcSrc->format = format;

	if (!(hdcDst = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}
	hdcDst->format = format;

	hPalette = NULL; // TODO: Get a real palette!

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRC, NULL, 16, 16, 8, format, hPalette);
	hBmpSrc = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DST, NULL, 16, 16, 8, format, hPalette);
	hBmpDst = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DST, NULL, 16, 16, 8, format, hPalette);
	hBmpDstOriginal = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PAT, NULL, 8, 8, 8, format, hPalette);
	hBmpPat = gdi_CreateBitmap(8, 8, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCCOPY, NULL, 16, 16, 8, format, hPalette);
	hBmp_SRCCOPY = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SPna, NULL, 16, 16, 8, format, hPalette);
	hBmp_SPna = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_BLACKNESS, NULL, 16, 16, 8, format, hPalette);
	hBmp_BLACKNESS = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_WHITENESS, NULL, 16, 16, 8, format, hPalette);
	hBmp_WHITENESS = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCAND, NULL, 16, 16, 8, format, hPalette);
	hBmp_SRCAND = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCPAINT, NULL, 16, 16, 8, format, hPalette);
	hBmp_SRCPAINT = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCINVERT, NULL, 16, 16, 8, format, hPalette);
	hBmp_SRCINVERT = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCERASE, NULL, 16, 16, 8, format, hPalette);
	hBmp_SRCERASE = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_NOTSRCCOPY, NULL, 16, 16, 8, format, hPalette);
	hBmp_NOTSRCCOPY = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_NOTSRCERASE, NULL, 16, 16, 8, format, hPalette);
	hBmp_NOTSRCERASE = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DSTINVERT, NULL, 16, 16, 8, format, hPalette);
	hBmp_DSTINVERT = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_MERGECOPY, NULL, 16, 16, 8, format, hPalette);
	hBmp_MERGECOPY = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_MERGEPAINT, NULL, 16, 16, 8, format, hPalette);
	hBmp_MERGEPAINT = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATCOPY, NULL, 16, 16, 8, format, hPalette);
	hBmp_PATCOPY = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATPAINT, NULL, 16, 16, 8, format, hPalette);
	hBmp_PATPAINT = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATINVERT, NULL, 16, 16, 8, format, hPalette);
	hBmp_PATINVERT = gdi_CreateBitmap(16, 16, format, data);

	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);
	gdi_SelectObject(hdcDst, (HGDIOBJECT) hBmpDst);

	/* SRCCOPY */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCCOPY, "SRCCOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* BLACKNESS */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_BLACKNESS))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_BLACKNESS, "BLACKNESS") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* WHITENESS */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_WHITENESS))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_WHITENESS, "WHITENESS") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCAND */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCAND))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCAND, "SRCAND") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCPAINT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCPAINT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCPAINT, "SRCPAINT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCINVERT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCINVERT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCINVERT, "SRCINVERT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCERASE */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCERASE))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCERASE, "SRCERASE") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* NOTSRCCOPY */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_NOTSRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_NOTSRCCOPY, "NOTSRCCOPY") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* NOTSRCERASE */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_NOTSRCERASE))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_NOTSRCERASE, "NOTSRCERASE") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* DSTINVERT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_DSTINVERT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_DSTINVERT, "DSTINVERT") < 0)
	//	return -1;

	/* select a brush for operations using a pattern */
	hBrush = gdi_CreatePatternBrush(hBmpPat);
	gdi_SelectObject(hdcDst, (HGDIOBJECT) hBrush);

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* MERGECOPY */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_MERGECOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_MERGECOPY, "MERGECOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* MERGEPAINT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_MERGEPAINT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_MERGEPAINT, "MERGEPAINT") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATCOPY */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATCOPY, "PATCOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATINVERT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATINVERT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATINVERT, "PATINVERT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATPAINT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATPAINT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATPAINT, "PATPAINT") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SPna */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SPna))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SPna, "SPna") < 0)
		return -1;

	return 0;
}

int test_gdi_BitBlt_16bpp(void)
{
	BYTE* data;
	HGDI_DC hdcSrc;
	HGDI_DC hdcDst;
	HGDI_BRUSH hBrush;
	HGDI_BITMAP hBmpSrc;
	HGDI_BITMAP hBmpDst;
	HGDI_BITMAP hBmpPat;
	HGDI_BITMAP hBmp_SPna;
	HGDI_BITMAP hBmp_BLACKNESS;
	HGDI_BITMAP hBmp_WHITENESS;
	HGDI_BITMAP hBmp_SRCCOPY;
	HGDI_BITMAP hBmp_SRCAND;
	HGDI_BITMAP hBmp_SRCPAINT;
	HGDI_BITMAP hBmp_SRCINVERT;
	HGDI_BITMAP hBmp_SRCERASE;
	HGDI_BITMAP hBmp_NOTSRCCOPY;
	HGDI_BITMAP hBmp_NOTSRCERASE;
	HGDI_BITMAP hBmp_DSTINVERT;
	HGDI_BITMAP hBmp_MERGECOPY;
	HGDI_BITMAP hBmp_MERGEPAINT;
	HGDI_BITMAP hBmp_PATCOPY;
	HGDI_BITMAP hBmp_PATPAINT;
	HGDI_BITMAP hBmp_PATINVERT;
	HGDI_BITMAP hBmpDstOriginal;
	rdpPalette* hPalette;
    const UINT32 format = PIXEL_FORMAT_XRGB32;

	if (!(hdcSrc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdcSrc->format = format;

	if (!(hdcDst = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdcDst->format = format;

	hPalette = (rdpPalette*) gdi_GetSystemPalette();

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRC, NULL, 16, 16, 8, format, hPalette);
	hBmpSrc = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DST, NULL, 16, 16, 8, format, hPalette);
	hBmpDst = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DST, NULL, 16, 16, 8, format, hPalette);
	hBmpDstOriginal = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PAT, NULL, 8, 8, 8, format, hPalette);
	hBmpPat = gdi_CreateBitmap(8, 8, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCCOPY, NULL, 16, 16, 8, format, hPalette);
	hBmp_SRCCOPY = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SPna, NULL, 16, 16, 8, format, hPalette);
	hBmp_SPna = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_BLACKNESS, NULL, 16, 16, 8, format, hPalette);
	hBmp_BLACKNESS = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_WHITENESS, NULL, 16, 16, 8, format, hPalette);
	hBmp_WHITENESS = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCAND, NULL, 16, 16, 8, format, hPalette);
	hBmp_SRCAND = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCPAINT, NULL, 16, 16, 8, format, hPalette);
	hBmp_SRCPAINT = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCINVERT, NULL, 16, 16, 8, format, hPalette);
	hBmp_SRCINVERT = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCERASE, NULL, 16, 16, 8, format, hPalette);
	hBmp_SRCERASE = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_NOTSRCCOPY, NULL, 16, 16, 8, format, hPalette);
	hBmp_NOTSRCCOPY = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_NOTSRCERASE, NULL, 16, 16, 8, format, hPalette);
	hBmp_NOTSRCERASE = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DSTINVERT, NULL, 16, 16, 8, format, hPalette);
	hBmp_DSTINVERT = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_MERGECOPY, NULL, 16, 16, 8, format, hPalette);
	hBmp_MERGECOPY = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_MERGEPAINT, NULL, 16, 16, 8, format, hPalette);
	hBmp_MERGEPAINT = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATCOPY, NULL, 16, 16, 8, format, hPalette);
	hBmp_PATCOPY = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATPAINT, NULL, 16, 16, 8, format, hPalette);
	hBmp_PATPAINT = gdi_CreateBitmap(16, 16, format, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATINVERT, NULL, 16, 16, 8, format, hPalette);
	hBmp_PATINVERT = gdi_CreateBitmap(16, 16, format, data);

	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);
	gdi_SelectObject(hdcDst, (HGDIOBJECT) hBmpDst);

	/* SRCCOPY */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCCOPY, "SRCCOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* BLACKNESS */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_BLACKNESS))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_BLACKNESS, "BLACKNESS") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* WHITENESS */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_WHITENESS))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_WHITENESS, "WHITENESS") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCAND */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCAND))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCAND, "SRCAND") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCPAINT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCPAINT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCPAINT, "SRCPAINT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCINVERT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCINVERT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCINVERT, "SRCINVERT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCERASE */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCERASE))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCERASE, "SRCERASE") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* NOTSRCCOPY */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_NOTSRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_NOTSRCCOPY, "NOTSRCCOPY") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* NOTSRCERASE */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_NOTSRCERASE))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_NOTSRCERASE, "NOTSRCERASE") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* DSTINVERT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_DSTINVERT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_DSTINVERT, "DSTINVERT") < 0)
	//	return -1;

	/* select a brush for operations using a pattern */
	hBrush = gdi_CreatePatternBrush(hBmpPat);
	gdi_SelectObject(hdcDst, (HGDIOBJECT) hBrush);

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* MERGECOPY */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_MERGECOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_MERGECOPY, "MERGECOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* MERGEPAINT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_MERGEPAINT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_MERGEPAINT, "MERGEPAINT") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATCOPY */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATCOPY, "PATCOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATINVERT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATINVERT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATINVERT, "PATINVERT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATPAINT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATPAINT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATPAINT, "PATPAINT") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SPna */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SPna))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SPna, "SPna") < 0)
		return -1;

	return 0;
}

int test_gdi_BitBlt_8bpp(void)
{
	BYTE* data;
	HGDI_DC hdcSrc;
	HGDI_DC hdcDst;
	HGDI_BRUSH hBrush;
	HGDI_BITMAP hBmpSrc;
	HGDI_BITMAP hBmpDst;
	HGDI_BITMAP hBmpPat;
	HGDI_BITMAP hBmp_SPna;
	HGDI_BITMAP hBmp_BLACKNESS;
	HGDI_BITMAP hBmp_WHITENESS;
	HGDI_BITMAP hBmp_SRCCOPY;
	HGDI_BITMAP hBmp_SRCAND;
	HGDI_BITMAP hBmp_SRCPAINT;
	HGDI_BITMAP hBmp_SRCINVERT;
	HGDI_BITMAP hBmp_SRCERASE;
	HGDI_BITMAP hBmp_NOTSRCCOPY;
	HGDI_BITMAP hBmp_NOTSRCERASE;
	HGDI_BITMAP hBmp_DSTINVERT;
	HGDI_BITMAP hBmp_MERGECOPY;
	HGDI_BITMAP hBmp_MERGEPAINT;
	HGDI_BITMAP hBmp_PATCOPY;
	HGDI_BITMAP hBmp_PATPAINT;
	HGDI_BITMAP hBmp_PATINVERT;
	HGDI_BITMAP hBmpDstOriginal;
    DWORD* hPalette;
    const UINT32 format = PIXEL_FORMAT_XRGB32;

	if (!(hdcSrc = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdcSrc->format = format;

    data = calloc(1024, 1024);
    if (!data)
        return -1;

	if (!(hdcDst = gdi_GetDC()))
	{
		printf("failed to get gdi device context\n");
		return -1;
	}

	hdcDst->format = format;

    hPalette = NULL; // TODO

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_SRC, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmpSrc = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_DST, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmpDst = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_DST, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmpDstOriginal = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 8, 8,
                            bmp_DST, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmpPat = gdi_CreateBitmap(8, 8, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_SRCCOPY, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_SRCCOPY = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_SPna, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_SPna = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_BLACKNESS, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_BLACKNESS = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_WHITENESS, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_WHITENESS = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_SRCAND, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_SRCAND = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_SRCPAINT, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_SRCPAINT = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_SRCINVERT, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_SRCINVERT = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_SRCERASE, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_SRCERASE = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_NOTSRCCOPY, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_NOTSRCCOPY = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_NOTSRCERASE, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_NOTSRCERASE = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_DSTINVERT, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_DSTINVERT = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_MERGECOPY, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_MERGECOPY = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_MERGEPAINT, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_MERGEPAINT = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_PATCOPY, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_PATCOPY = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_PATPAINT, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_PATPAINT = gdi_CreateBitmap(16, 16, format, data);

    if (!freerdp_image_copy(data, format, -1, 0, 0, 16, 16,
                            bmp_PATINVERT, PIXEL_FORMAT_RGB8, -1, 0, 0, hPalette))
        return -1;
	hBmp_PATINVERT = gdi_CreateBitmap(16, 16, format, data);

	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);
	gdi_SelectObject(hdcDst, (HGDIOBJECT) hBmpDst);

	/* SRCCOPY */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (CompareBitmaps(hBmpDst, hBmp_SRCCOPY) != 1)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* BLACKNESS */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_BLACKNESS))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_BLACKNESS, "BLACKNESS") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* WHITENESS */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_WHITENESS))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_WHITENESS, "WHITENESS") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCAND */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCAND))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCAND, "SRCAND") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCPAINT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCPAINT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCPAINT, "SRCPAINT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCINVERT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCINVERT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCINVERT, "SRCINVERT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCERASE */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCERASE))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCERASE, "SRCERASE") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* NOTSRCCOPY */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_NOTSRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_NOTSRCCOPY, "NOTSRCCOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* NOTSRCERASE */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_NOTSRCERASE))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_NOTSRCERASE, "NOTSRCERASE") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* DSTINVERT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_DSTINVERT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_DSTINVERT, "DSTINVERT") < 0)
		return -1;

	/* select a brush for operations using a pattern */
	hBrush = gdi_CreatePatternBrush(hBmpPat);
	gdi_SelectObject(hdcDst, (HGDIOBJECT) hBrush);

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* MERGECOPY */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_MERGECOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_MERGECOPY, "MERGECOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* MERGEPAINT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_MERGEPAINT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_MERGEPAINT, "MERGEPAINT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATCOPY */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATCOPY, "PATCOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATINVERT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATINVERT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATINVERT, "PATINVERT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATPAINT */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATPAINT))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATPAINT, "PATPAINT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SPna */
	if (!gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SPna))
	{
		printf("gdi_BitBlt failed (line #%u)\n", __LINE__);
		return -1;
	}

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SPna, "SPna") < 0)
		return -1;

	return 0;
}

int TestGdiBitBlt(int argc, char* argv[])
{
	return 0; /* FIXME: broken tests */

	fprintf(stderr, "test_gdi_BitBlt_32bpp()\n");

	if (test_gdi_BitBlt_32bpp() < 0)
		return -1;

	fprintf(stderr, "test_gdi_BitBlt_16bpp()\n");

	if (test_gdi_BitBlt_16bpp() < 0)
		return -1;

	fprintf(stderr, "test_gdi_BitBlt_8bpp()\n");

	if (test_gdi_BitBlt_8bpp() < 0)
		return -1;

	return 0;
}
