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

#include "mfreerdp.h"
#include <freerdp/utils/signal.h>
#include <freerdp/client/cmdline.h>

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

int freerdp_client_start(void* cfi)
{
    mfInfo* mfi = (mfInfo*) cfi;

    rdpSettings* settings = mfi->instance->settings;

	if (!settings->ServerHostname)
	{
		fprintf(stderr, "error: server hostname was not specified with /v:<server>[:port]\n");
		return -1;
	}

    //mfi->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) mf_thread, (void*) mfi->instance, 0, NULL);

	return 0;
}

int freerdp_client_stop(void* cfi)
{
    mfInfo* mfi = (mfInfo*) cfi;

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

void* freerdp_client_get_instance(void* cfi)
{
    mfInfo* mfi = (mfInfo*) cfi;
    return mfi->instance;
}

HANDLE freerdp_client_get_thread(void* cfi)
{
    mfInfo* mfi = (mfInfo*) cfi;

    return mfi->thread;
}

void* freerdp_client_get_interface(void* cfi)
{
    mfInfo* mfi = (mfInfo*) cfi;

    return mfi->client;
}

cfInfo* freerdp_client_new(int argc, char** argv)
{
	int index;
	int status;
    mfInfo* mfi;
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

    instance->context_size = sizeof(mfContext);
    instance->ContextNew = (pContextNew) mf_context_new;
    instance->ContextFree = (pContextFree) mf_context_free;
	freerdp_context_new(instance);

	instance->context->argc = argc;
	instance->context->argv = (char**) malloc(sizeof(char*) * argc);

	for (index = 0; index < argc; index++)
		instance->context->argv[index] = _strdup(argv[index]);

    mfi = (mfInfo*) malloc(sizeof(mfInfo));
    ZeroMemory(mfi, sizeof(mfInfo));

    ((mfContext*) instance->context)->mfi = mfi;

    mfi->instance = instance;
	settings = instance->settings;
    mfi->client = instance->context->client;

	status = freerdp_client_parse_command_line_arguments(instance->context->argc,
				instance->context->argv, settings);
	if (status < 0)
	{
        freerdp_context_free(mfi->instance);
        freerdp_free(mfi->instance);
        free(mfi);
		return NULL;
	}

	if (settings->ConnectionFile)
	{
		file = freerdp_client_rdp_file_new();

		fprintf(stderr, "Using connection file: %s\n", settings->ConnectionFile);

		freerdp_client_parse_rdp_file(file, settings->ConnectionFile);
		freerdp_client_populate_settings_from_rdp_file(file, settings);
	}

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

//	if (settings->ListMonitors)
//	{
//		mf_list_monitors(mfi);
//	}

    return mfi;
}

void freerdp_client_free(void* cfi)
{
    mfInfo* mfi = (mfInfo*) cfi;

    if (mfi)
	{
		int index;
		rdpContext* context;

//        mf_window_free(mfi);

//        free(mfi->bmp_codec_none);

//        XCloseDisplay(mfi->display);

//        context = (rdpContext*) mfi->context;

		for (index = 0; index < context->argc; index++)
			free(context->argv[index]);

		free(context->argv);

        freerdp_context_free(mfi->instance);
        freerdp_free(mfi->instance);

        free(mfi);
	}
}


BOOL freerdp_client_get_param_bool(void* cfi, int id)
{
    mfInfo* mfi = (mfInfo*) cfi;
    rdpSettings* settings = mfi->instance->settings;

    return freerdp_get_param_bool(settings, id);
}

int freerdp_client_set_param_bool(void* cfi, int id, BOOL param)
{
    mfInfo* mfi = (mfInfo*) cfi;
    rdpSettings* settings = mfi->instance->settings;

    return freerdp_set_param_bool(settings, id, param);
}

UINT32 freerdp_client_get_param_uint32(void* cfi, int id)
{
    mfInfo* mfi = (mfInfo*) cfi;
    rdpSettings* settings = mfi->instance->settings;

    return freerdp_get_param_uint32(settings, id);
}

int freerdp_client_set_param_uint32(void* cfi, int id, UINT32 param)
{
    mfInfo* mfi = (mfInfo*) cfi;
    rdpSettings* settings = mfi->instance->settings;

    return freerdp_set_param_uint32(settings, id, param);
}

UINT64 freerdp_client_get_param_uint64(void* cfi, int id)
{
    mfInfo* mfi = (mfInfo*) cfi;
    rdpSettings* settings = mfi->instance->settings;

    return freerdp_get_param_uint64(settings, id);
}

int freerdp_client_set_param_uint64(void* cfi, int id, UINT64 param)
{
    mfInfo* mfi = (mfInfo*) cfi;
    rdpSettings* settings = mfi->instance->settings;

    return freerdp_set_param_uint64(settings, id, param);
}

char* freerdp_client_get_param_string(void* cfi, int id)
{
    mfInfo* mfi = (mfInfo*) cfi;
    rdpSettings* settings = mfi->instance->settings;

    return freerdp_get_param_string(settings, id);
}

int freerdp_client_set_param_string(void* cfi, int id, char* param)
{
    mfInfo* mfi = (mfInfo*) cfi;
    rdpSettings* settings = mfi->instance->settings;

    return freerdp_set_param_string(settings, id, param);
}


void freerdp_client_mouse_event(void* cfi, DWORD flags, int x, int y)
{
    int width, height;
    mfInfo* mfi = (mfInfo*) cfi;
    rdpInput* input = mfi->instance->input;
    rdpSettings* settings = mfi->instance->settings;

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
