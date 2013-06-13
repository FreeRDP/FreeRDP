#ifndef MF_CLIENT_H
#define MF_CLIENT_H


#ifdef __cplusplus
extern "C" {
#endif

#include <winpr/windows.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/region.h>
#include <freerdp/cache/cache.h>
#include <freerdp/codec/color.h>
#include <freerdp/utils/debug.h>
#include <freerdp/channels/channels.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/client/file.h>

#include "mf_event.h"

// System menu constants
#define SYSCOMMAND_ID_SMARTSIZING 1000

struct mf_context
{
    rdpContext context;

    freerdp* instance;
    rdpClient* client;
    rdpSettings* settings;

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

    HANDLE thread;
    HANDLE keyboardThread;

    HGDI_DC hdc;
    UINT16 srcBpp;
    UINT16 dstBpp;

    DWORD mainThreadId;
    DWORD keyboardThreadId;
    RFX_CONTEXT* rfx_context;
    NSC_CONTEXT* nsc_context;

    BOOL sw_gdi;
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
};
typedef struct mf_context mfContext;

/**
 * Client Interface
 */

typedef struct mf_context cfContext;

void mf_context_new(freerdp* instance, cfContext* context);

void mf_context_free(freerdp* instance, cfContext* context);

#ifdef __cplusplus
}
#endif

#endif // MF_CLIENT_H
