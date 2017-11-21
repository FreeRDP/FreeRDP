#ifndef FREERDP_CLIENT_MAC_FREERDP_H
#define FREERDP_CLIENT_MAC_FREERDP_H

typedef struct mf_context mfContext;

#include <freerdp/freerdp.h>
#include <freerdp/client/file.h>
#include <freerdp/api.h>
#include <freerdp/freerdp.h>

#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/gdi/region.h>
#include <freerdp/cache/cache.h>
#include <freerdp/channels/channels.h>

#include <freerdp/client/channels.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/encomsp.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/clipboard.h>

#include "MRDPView.h"
#include "Keyboard.h"
#include <AppKit/NSView.h>

struct mf_context
{
	rdpContext context;
	DEFINE_RDP_CLIENT_COMMON();

	void* view;
	BOOL view_ownership;

	int width;
	int height;
	int offset_x;
	int offset_y;
	int fs_toggle;
	int fullscreen;
	int percentscreen;
	char window_title[64];
	int client_x;
	int client_y;
	int client_width;
	int client_height;

	HANDLE stopEvent;
	HANDLE keyboardThread;
	enum APPLE_KEYBOARD_TYPE appleKeyboardType;

	DWORD mainThreadId;
	DWORD keyboardThreadId;

	BOOL clipboardSync;
	wClipboard* clipboard;
	UINT32 numServerFormats;
	UINT32 requestedFormatId;
	HANDLE clipboardRequestEvent;
	CLIPRDR_FORMAT* serverFormats;
	CliprdrClientContext* cliprdr;
	UINT32 clipboardCapabilities;

	rdpFile* connectionRdpFile;

	// Keep track of window size and position, disable when in fullscreen mode.
	BOOL disablewindowtracking;

	// These variables are required for horizontal scrolling.
	BOOL updating_scrollbars;
	BOOL xScrollVisible;
	int xMinScroll;       // minimum horizontal scroll value
	int xCurrentScroll;   // current horizontal scroll value
	int xMaxScroll;       // maximum horizontal scroll value

	// These variables are required for vertical scrolling.
	BOOL yScrollVisible;
	int yMinScroll;       // minimum vertical scroll value
	int yCurrentScroll;   // current vertical scroll value
	int yMaxScroll;       // maximum vertical scroll value

	CGEventFlags kbdFlags;
};

#endif /* FREERDP_CLIENT_MAC_FREERDP_H */
