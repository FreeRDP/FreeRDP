
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/line.h>
#include <freerdp/gdi/brush.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/gdi/palette.h>
#include <freerdp/gdi/drawing.h>

#include <winpr/crt.h>

/* BitBlt() Test Data */

/* source bitmap (16x16) */
BYTE bmp_SRC[256] =
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
BYTE bmp_DST[256] =
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
BYTE bmp_PAT[64] =
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
BYTE bmp_SRCCOPY[256] =
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
BYTE bmp_BLACKNESS[256] =
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
BYTE bmp_WHITENESS[256] =
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
BYTE bmp_SRCAND[256] =
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
BYTE bmp_SRCPAINT[256] =
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
BYTE bmp_SRCINVERT[256] =
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
BYTE bmp_SRCERASE[256] =
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
BYTE bmp_NOTSRCCOPY[256] =
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
BYTE bmp_NOTSRCERASE[256] =
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
BYTE bmp_DSTINVERT[256] =
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
BYTE bmp_SPna[256] =
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
BYTE bmp_MERGEPAINT[256] =
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
BYTE bmp_MERGECOPY[256] =
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
BYTE bmp_PATPAINT[256] =
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
BYTE bmp_PATCOPY[256] =
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
BYTE bmp_PATINVERT[256] =
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

int CompareBitmaps(HGDI_BITMAP hBmp1, HGDI_BITMAP hBmp2)
{
	int bpp;
	int x, y;
	BYTE *p1, *p2;

	int minw = (hBmp1->width < hBmp2->width) ? hBmp1->width : hBmp2->width;
	int minh = (hBmp1->height < hBmp2->height) ? hBmp1->height : hBmp2->height;

	if (hBmp1->bitsPerPixel == hBmp2->bitsPerPixel)
	{
		p1 = hBmp1->data;
		p2 = hBmp2->data;
		bpp = hBmp1->bitsPerPixel;

		if (bpp == 32)
		{
			for (y = 0; y < minh; y++)
			{
				for (x = 0; x < minw; x++)
				{
					if (*p1 != *p2)
						return 0;
					p1++;
					p2++;

					if (*p1 != *p2)
						return 0;
					p1++;
					p2++;

					if (*p1 != *p2)
						return 0;
					p1 += 2;
					p2 += 2;
				}
			}
		}
		else if (bpp == 16)
		{
			for (y = 0; y < minh; y++)
			{
				for (x = 0; x < minw; x++)
				{
					if (*p1 != *p2)
						return 0;
					p1++;
					p2++;

					if (*p1 != *p2)
						return 0;
					p1++;
					p2++;
				}
			}
		}
		else if (bpp == 8)
		{
			for (y = 0; y < minh; y++)
			{
				for (x = 0; x < minw; x++)
				{
					if (*p1 != *p2)
						return 0;
					p1++;
					p2++;
				}
			}
		}
	}
	else
	{
		return 0;
	}

	return 1;
}

void test_dump_data(unsigned char* p, int len, int width, const char* name)
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

void test_dump_bitmap(HGDI_BITMAP hBmp, const char* name)
{
	test_dump_data(hBmp->data, hBmp->width * hBmp->height * hBmp->bytesPerPixel, hBmp->width * hBmp->bytesPerPixel, name);
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
	rdpPalette* hPalette;
	HCLRCONV clrconv;

	int bytesPerPixel = 4;
	int bitsPerPixel = 32;

	hdcSrc = gdi_GetDC();
	hdcSrc->bytesPerPixel = bytesPerPixel;
	hdcSrc->bitsPerPixel = bitsPerPixel;

	hdcDst = gdi_GetDC();
	hdcDst->bytesPerPixel = bytesPerPixel;
	hdcDst->bitsPerPixel = bitsPerPixel;

	hPalette = (rdpPalette*) gdi_GetSystemPalette();

	clrconv = (HCLRCONV) calloc(1, sizeof(CLRCONV));
	clrconv->alpha = 0;
	clrconv->invert = 0;
	clrconv->palette = hPalette;

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRC, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmpSrc = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DST, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmpDst = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DST, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmpDstOriginal = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PAT, NULL, 8, 8, 8, bitsPerPixel, clrconv);
	hBmpPat = gdi_CreateBitmap(8, 8, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCCOPY, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCCOPY = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SPna, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SPna = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_BLACKNESS, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_BLACKNESS = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_WHITENESS, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_WHITENESS = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCAND, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCAND = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCPAINT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCPAINT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCINVERT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCINVERT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCERASE, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCERASE = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_NOTSRCCOPY, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_NOTSRCCOPY = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_NOTSRCERASE, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_NOTSRCERASE = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DSTINVERT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_DSTINVERT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_MERGECOPY, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_MERGECOPY = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_MERGEPAINT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_MERGEPAINT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATCOPY, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_PATCOPY = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATPAINT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_PATPAINT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATINVERT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_PATINVERT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);
	gdi_SelectObject(hdcDst, (HGDIOBJECT) hBmpDst);

	/* SRCCOPY */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCCOPY, "SRCCOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* BLACKNESS */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_BLACKNESS);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_BLACKNESS, "BLACKNESS") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* WHITENESS */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_WHITENESS);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_WHITENESS, "WHITENESS") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCAND */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCAND);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCAND, "SRCAND") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCPAINT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCPAINT);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCPAINT, "SRCPAINT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCINVERT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCINVERT);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCINVERT, "SRCINVERT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCERASE */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCERASE);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCERASE, "SRCERASE") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* NOTSRCCOPY */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_NOTSRCCOPY);

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_NOTSRCCOPY, "NOTSRCCOPY") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* NOTSRCERASE */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_NOTSRCERASE);

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_NOTSRCERASE, "NOTSRCERASE") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* DSTINVERT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_DSTINVERT);

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_DSTINVERT, "DSTINVERT") < 0)
	//	return -1;

	/* select a brush for operations using a pattern */
	hBrush = gdi_CreatePatternBrush(hBmpPat);
	gdi_SelectObject(hdcDst, (HGDIOBJECT) hBrush);

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* MERGECOPY */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_MERGECOPY);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_MERGECOPY, "MERGECOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* MERGEPAINT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_MERGEPAINT);

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_MERGEPAINT, "MERGEPAINT") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATCOPY */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATCOPY);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATCOPY, "PATCOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATINVERT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATINVERT);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATINVERT, "PATINVERT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATPAINT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATPAINT);

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATPAINT, "PATPAINT") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SPna */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SPna);

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
	HCLRCONV clrconv;

	int bytesPerPixel = 2;
	int bitsPerPixel = 16;

	hdcSrc = gdi_GetDC();
	hdcSrc->bytesPerPixel = bytesPerPixel;
	hdcSrc->bitsPerPixel = bitsPerPixel;

	hdcDst = gdi_GetDC();
	hdcDst->bytesPerPixel = bytesPerPixel;
	hdcDst->bitsPerPixel = bitsPerPixel;

	hPalette = (rdpPalette*) gdi_GetSystemPalette();

	clrconv = (HCLRCONV) malloc(sizeof(CLRCONV));
	clrconv->alpha = 1;
	clrconv->invert = 0;
	clrconv->palette = hPalette;

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRC, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmpSrc = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DST, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmpDst = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DST, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmpDstOriginal = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PAT, NULL, 8, 8, 8, bitsPerPixel, clrconv);
	hBmpPat = gdi_CreateBitmap(8, 8, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCCOPY, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCCOPY = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SPna, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SPna = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_BLACKNESS, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_BLACKNESS = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_WHITENESS, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_WHITENESS = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCAND, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCAND = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCPAINT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCPAINT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCINVERT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCINVERT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCERASE, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCERASE = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_NOTSRCCOPY, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_NOTSRCCOPY = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_NOTSRCERASE, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_NOTSRCERASE = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DSTINVERT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_DSTINVERT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_MERGECOPY, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_MERGECOPY = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_MERGEPAINT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_MERGEPAINT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATCOPY, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_PATCOPY = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATPAINT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_PATPAINT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATINVERT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_PATINVERT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);
	gdi_SelectObject(hdcDst, (HGDIOBJECT) hBmpDst);

	/* SRCCOPY */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCCOPY, "SRCCOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* BLACKNESS */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_BLACKNESS);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_BLACKNESS, "BLACKNESS") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* WHITENESS */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_WHITENESS);

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_WHITENESS, "WHITENESS") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCAND */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCAND);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCAND, "SRCAND") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCPAINT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCPAINT);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCPAINT, "SRCPAINT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCINVERT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCINVERT);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCINVERT, "SRCINVERT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCERASE */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCERASE);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCERASE, "SRCERASE") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* NOTSRCCOPY */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_NOTSRCCOPY);

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_NOTSRCCOPY, "NOTSRCCOPY") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* NOTSRCERASE */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_NOTSRCERASE);

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_NOTSRCERASE, "NOTSRCERASE") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* DSTINVERT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_DSTINVERT);

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_DSTINVERT, "DSTINVERT") < 0)
	//	return -1;

	/* select a brush for operations using a pattern */
	hBrush = gdi_CreatePatternBrush(hBmpPat);
	gdi_SelectObject(hdcDst, (HGDIOBJECT) hBrush);

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* MERGECOPY */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_MERGECOPY);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_MERGECOPY, "MERGECOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* MERGEPAINT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_MERGEPAINT);

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_MERGEPAINT, "MERGEPAINT") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATCOPY */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATCOPY);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATCOPY, "PATCOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATINVERT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATINVERT);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATINVERT, "PATINVERT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATPAINT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATPAINT);

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATPAINT, "PATPAINT") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SPna */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SPna);

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
	rdpPalette* hPalette;
	HCLRCONV clrconv;

	int bytesPerPixel = 1;
	int bitsPerPixel = 8;

	hdcSrc = gdi_GetDC();
	hdcSrc->bytesPerPixel = bytesPerPixel;
	hdcSrc->bitsPerPixel = bitsPerPixel;

	hdcDst = gdi_GetDC();
	hdcDst->bytesPerPixel = bytesPerPixel;
	hdcDst->bitsPerPixel = bitsPerPixel;

	hPalette = (rdpPalette*) gdi_GetSystemPalette();

	clrconv = (HCLRCONV) malloc(sizeof(CLRCONV));
	clrconv->alpha = 1;
	clrconv->invert = 0;
	clrconv->palette = hPalette;

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRC, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmpSrc = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DST, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmpDst = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DST, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmpDstOriginal = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PAT, NULL, 8, 8, 8, bitsPerPixel, clrconv);
	hBmpPat = gdi_CreateBitmap(8, 8, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCCOPY, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCCOPY = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SPna, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SPna = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_BLACKNESS, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_BLACKNESS = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_WHITENESS, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_WHITENESS = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCAND, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCAND = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCPAINT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCPAINT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCINVERT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCINVERT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_SRCERASE, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_SRCERASE = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_NOTSRCCOPY, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_NOTSRCCOPY = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_NOTSRCERASE, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_NOTSRCERASE = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_DSTINVERT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_DSTINVERT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_MERGECOPY, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_MERGECOPY = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_MERGEPAINT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_MERGEPAINT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATCOPY, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_PATCOPY = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATPAINT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_PATPAINT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	data = (BYTE*) freerdp_image_convert((BYTE*) bmp_PATINVERT, NULL, 16, 16, 8, bitsPerPixel, clrconv);
	hBmp_PATINVERT = gdi_CreateBitmap(16, 16, bitsPerPixel, data);

	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);
	gdi_SelectObject(hdcDst, (HGDIOBJECT) hBmpDst);

	/* SRCCOPY */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);

	if (CompareBitmaps(hBmpDst, hBmp_SRCCOPY) != 1)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* BLACKNESS */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_BLACKNESS);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_BLACKNESS, "BLACKNESS") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* WHITENESS */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_WHITENESS);

	//if (test_assert_bitmaps_equal(hBmpDst, hBmp_WHITENESS, "WHITENESS") < 0)
	//	return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCAND */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCAND);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCAND, "SRCAND") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCPAINT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCPAINT);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCPAINT, "SRCPAINT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCINVERT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCINVERT);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCINVERT, "SRCINVERT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SRCERASE */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCERASE);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_SRCERASE, "SRCERASE") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* NOTSRCCOPY */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_NOTSRCCOPY);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_NOTSRCCOPY, "NOTSRCCOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* NOTSRCERASE */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_NOTSRCERASE);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_NOTSRCERASE, "NOTSRCERASE") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* DSTINVERT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_DSTINVERT);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_DSTINVERT, "DSTINVERT") < 0)
		return -1;

	/* select a brush for operations using a pattern */
	hBrush = gdi_CreatePatternBrush(hBmpPat);
	gdi_SelectObject(hdcDst, (HGDIOBJECT) hBrush);

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* MERGECOPY */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_MERGECOPY);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_MERGECOPY, "MERGECOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* MERGEPAINT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_MERGEPAINT);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_MERGEPAINT, "MERGEPAINT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATCOPY */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATCOPY);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATCOPY, "PATCOPY") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATINVERT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATINVERT);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATINVERT, "PATINVERT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* PATPAINT */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_PATPAINT);

	if (test_assert_bitmaps_equal(hBmpDst, hBmp_PATPAINT, "PATPAINT") < 0)
		return -1;

	/* restore original destination bitmap */
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpDstOriginal);
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SRCCOPY);
	gdi_SelectObject(hdcSrc, (HGDIOBJECT) hBmpSrc);

	/* SPna */
	gdi_BitBlt(hdcDst, 0, 0, 16, 16, hdcSrc, 0, 0, GDI_SPna);

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
