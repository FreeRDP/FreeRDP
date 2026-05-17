/*
   Android Display Update Virtual Channel

*/

#ifndef FREERDP_CLIENT_ANDROID_DISP_H
#define FREERDP_CLIENT_ANDROID_DISP_H

#include <freerdp/client/disp.h>
#include <freerdp/api.h>

#include "android_freerdp.h"

FREERDP_LOCAL BOOL android_disp_init(androidContext* afc, DispClientContext* disp);
FREERDP_LOCAL BOOL android_disp_uninit(androidContext* afc, DispClientContext* disp);
FREERDP_LOCAL BOOL android_disp_send_monitor_layout(androidContext* afc, UINT32 width,
                                                    UINT32 height);

#endif /* FREERDP_CLIENT_ANDROID_DISP_H */
