#ifndef MFREERDP_H
#define MFREERDP_H

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

#ifdef __cplusplus
}
#endif



// System menu constants
#define SYSCOMMAND_ID_SMARTSIZING 1000

typedef struct mf_info mfInfo;

struct mf_context
{
    rdpContext _p;

    mfInfo* mfi;
};
typedef struct mf_context mfContext;

typedef void (CALLBACK * callbackFunc)(mfInfo* mfi, int callback_type, DWORD param1, DWORD param2);

struct mf_info
{
    rdpClient* client;

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
    freerdp* instance;

    DWORD mainThreadId;
    DWORD keyboardThreadId;
    RFX_CONTEXT* rfx_context;
    NSC_CONTEXT* nsc_context;

    BOOL sw_gdi;
    callbackFunc client_callback_func;

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

/**
 * Client Interface
 */

#define cfInfo mfInfo

void mf_on_param_change(freerdp* instance, int id);



//FREERDP_API int freerdp_client_global_init();
//FREERDP_API int freerdp_client_global_uninit();

//FREERDP_API int freerdp_client_start(mfInfo* cfi);
//FREERDP_API int freerdp_client_stop(mfInfo* cfi);

//FREERDP_API HANDLE freerdp_client_get_thread(mfInfo* cfi);
//FREERDP_API freerdp* freerdp_client_get_instance(mfInfo* cfi);
//FREERDP_API rdpClient* freerdp_client_get_interface(mfInfo* cfi);

//FREERDP_API int freerdp_client_focus_in(mfInfo* cfi);
//FREERDP_API int freerdp_client_focus_out(mfInfo* cfi);

//FREERDP_API int freerdp_client_set_window_size(mfInfo* cfi, int width, int height);

//FREERDP_API cfInfo* freerdp_client_new(int argc, char** argv);
//FREERDP_API void freerdp_client_free(mfInfo* cfi);

//FREERDP_API int freerdp_client_set_client_callback_function(mfInfo* cfi, callbackFunc callbackFunc);

//FREERDP_API rdpSettings* freerdp_client_get_settings(mfInfo* wfi);

//FREERDP_API int freerdp_client_load_settings_from_rdp_file(mfInfo* cfi, char* filename);
//FREERDP_API int freerdp_client_save_settings_to_rdp_file(mfInfo* cfi, char* filename);


mfInfo* mf_mfi_new();

void mf_mfi_free(mfInfo* mfi);

void mf_context_new(freerdp* instance, rdpContext* context);

void mf_context_free(freerdp* instance, rdpContext* context);

void mf_on_param_change(freerdp* instance, int id);

#endif // MFREERDP_H
