/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/utils/signal.h>
#include <freerdp/client/cmdline.h>
#include "mf_client.h"

/**
 * Client Interface
 */

int freerdp_client_global_init()
{
//    setlocale(LC_ALL, "");
    freerdp_handle_signals();
    freerdp_channels_global_init();

    return 0;
}

int freerdp_client_global_uninit()
{
    freerdp_channels_global_uninit();

    return 0;
}

int freerdp_client_start(rdpContext* cfc)
{
    mfContext* mfc = (mfContext*) cfc;
    rdpSettings* settings = mfc->settings;

    if (!settings->ServerHostname)
    {
        fprintf(stderr, "error: server hostname was not specified with /v:<server>[:port]\n");
        return -1;
    }

    //mfi->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) mf_thread, (void*) mfi->instance, 0, NULL);

    return 0;
}

int freerdp_client_stop(rdpContext* cfc)
{
    mfContext* mfc = (mfContext*) cfc;

//    if (mfi->instance->settings->AsyncInput)
//	{
//		wMessageQueue* queue;
//        queue = freerdp_get_message_queue(mfi->instance, FREERDP_INPUT_MESSAGE_QUEUE);
//		MessageQueue_PostQuit(queue, 0);
//	}
//	else
//	{
//        mfi->disconnect = TRUE;
//	}

    return 0;
}

HANDLE freerdp_client_get_thread(rdpContext* cfc)
{
    mfContext* mfc = (mfContext*) cfc;

    return mfc->thread;
}

rdpClient* freerdp_client_get_interface(rdpContext* cfc)
{
    mfContext* mfc = (mfContext*) cfc;

    return mfc->client;
}

rdpContext* freerdp_client_new(int argc, char** argv)
{
    int index;
    int status;
    mfContext* mfc;
    rdpFile* file;
    freerdp* instance;
    rdpSettings* settings;

    instance = freerdp_new();
//    instance->PreConnect = mf_pre_connect;
//    instance->PostConnect = mf_post_connect;
//    instance->Authenticate = mf_authenticate;
//    instance->VerifyCertificate = mf_verify_certificate;
//    instance->LogonErrorInfo = mf_logon_error_info;
//    instance->ReceiveChannelData = mf_receive_channel_data;

    instance->ContextSize = sizeof(mfContext);
    instance->ContextNew = (pContextNew) mf_context_new;
    instance->ContextFree = (pContextFree) mf_context_free;
    freerdp_context_new(instance);

    instance->context->argc = argc;
    instance->context->argv = (char**) malloc(sizeof(char*) * argc);

    for (index = 0; index < argc; index++)
        instance->context->argv[index] = _strdup(argv[index]);

    mfc = (mfContext*) instance->context;

    mfc->instance = instance;
    settings = instance->settings;
    mfc->client = instance->context->client;
    mfc->settings = instance->context->settings;

    settings->OsMajorType = OSMAJORTYPE_UNIX;
    settings->OsMinorType = OSMINORTYPE_NATIVE_XSERVER;

    settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
    settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
    settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
    settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
    settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
    settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
    settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
    settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
    settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
    settings->OrderSupport[NEG_MEMBLT_INDEX] = settings->BitmapCacheEnabled;

    settings->OrderSupport[NEG_MEM3BLT_INDEX] = (settings->SoftwareGdi) ? TRUE : FALSE;

    settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = settings->BitmapCacheEnabled;
    settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
    settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
    settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
    settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
    settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;

    settings->OrderSupport[NEG_POLYGON_SC_INDEX] = (settings->SoftwareGdi) ? FALSE : TRUE;
    settings->OrderSupport[NEG_POLYGON_CB_INDEX] = (settings->SoftwareGdi) ? FALSE : TRUE;

    settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
    settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;

    return (rdpContext*) mfc;
}

void freerdp_client_free(rdpContext* cfc)
{
    mfContext* mfc = (mfContext*) cfc;

    if (mfc)
    {
        int index;
        freerdp* instance;
        rdpContext* context;

        context = (rdpContext*) mfc;
        instance = context->instance;

        for (index = 0; index < context->argc; index++)
            free(context->argv[index]);

        free(context->argv);

        freerdp_context_free(instance);
        freerdp_free(instance);
    }
}


void freerdp_client_mouse_event(rdpContext* cfc, DWORD flags, int x, int y)
{
    int width, height;
    mfContext* mfc = (mfContext*) cfc;
    rdpInput* input = mfc->instance->input;
    rdpSettings* settings = mfc->instance->settings;

    width = settings->DesktopWidth;
    height = settings->DesktopHeight;

    if (x < 0)
        x = 0;

    if (x >= width)
        x = width - 1;

    if (y < 0)
        y = 0;

    if (y >= height)
        y = height - 1;

    input->MouseEvent(input, flags, x, y);
}

int mf_context_new(freerdp* instance, cfContext* cfc)
{
    cfc->instance = instance;
    cfc->settings = instance->settings;
    return 0;
}

void mf_context_free(freerdp* instance, cfContext* cfc)
{
}
