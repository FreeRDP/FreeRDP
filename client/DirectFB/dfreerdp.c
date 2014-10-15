/**
 * FreeRDP: A Remote Desktop Protocol Client
 * DirectFB Client
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Killer{R} <support@killprog.com>
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

#include <errno.h>
#include <pthread.h>
#include <locale.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <freerdp/utils/args.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/semaphore.h>
#include <freerdp/utils/event.h>
#include <freerdp/peer.h>
#include <freerdp/constants.h>
#include <freerdp/plugins/cliprdr.h>
#include <freerdp/gdi/region.h>

#include "df_event.h"
#include "df_graphics.h"
#include "df_run.h"

#include "dfreerdp.h"

static freerdp_sem g_sem;
static int g_thread_count = 0;

struct thread_data
{
	freerdp* instance;
};

void df_context_new(freerdp* instance, rdpContext* context)
{
	context->channels = freerdp_channels_new();
}

void df_context_free(freerdp* instance, rdpContext* context)
{

}


boolean df_pre_connect(freerdp* instance)
{
	dfInfo* dfi;
	boolean bitmap_cache;
	dfContext* context;
	rdpSettings* settings;

	dfi = (dfInfo*) xzalloc(sizeof(dfInfo));
	context = ((dfContext*) instance->context);
	context->dfi = dfi;

	settings = instance->settings;
	bitmap_cache = settings->bitmap_cache;

	settings->order_support[NEG_DSTBLT_INDEX] = true;
	settings->order_support[NEG_PATBLT_INDEX] = true;
	settings->order_support[NEG_SCRBLT_INDEX] = true;
	settings->order_support[NEG_OPAQUE_RECT_INDEX] = true;
	settings->order_support[NEG_DRAWNINEGRID_INDEX] = false;
	settings->order_support[NEG_MULTIDSTBLT_INDEX] = false;
	settings->order_support[NEG_MULTIPATBLT_INDEX] = false;
	settings->order_support[NEG_MULTISCRBLT_INDEX] = false;
	settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = true;
	settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = false;
	settings->order_support[NEG_LINETO_INDEX] = true;
	settings->order_support[NEG_POLYLINE_INDEX] = true;
	settings->order_support[NEG_MEMBLT_INDEX] = bitmap_cache;
	settings->order_support[NEG_MEM3BLT_INDEX] = false;
	settings->order_support[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
	settings->order_support[NEG_MEM3BLT_V2_INDEX] = false;
	settings->order_support[NEG_SAVEBITMAP_INDEX] = false;
	settings->order_support[NEG_GLYPH_INDEX_INDEX] = false;
	settings->order_support[NEG_FAST_INDEX_INDEX] = false;
	settings->order_support[NEG_FAST_GLYPH_INDEX] = false;
	settings->order_support[NEG_POLYGON_SC_INDEX] = false;
	settings->order_support[NEG_POLYGON_CB_INDEX] = false;
	settings->order_support[NEG_ELLIPSE_SC_INDEX] = false;
	settings->order_support[NEG_ELLIPSE_CB_INDEX] = false;

	dfi->clrconv = xnew(CLRCONV);
	dfi->clrconv->alpha = 1;
	dfi->clrconv->invert = 0;
	dfi->clrconv->rgb555 = 0;
	dfi->clrconv->palette = xnew(rdpPalette);

	freerdp_channels_pre_connect(instance->context->channels, instance);

	return true;
}

static void df_init_cursor(dfContext* context)
{
	if (((rdpContext *)context)->instance->settings->fullscreen)
	{
		context->dfi->pointer_x = context->_p.gdi->width / 2;
		context->dfi->pointer_y = context->_p.gdi->height / 2;
	}
	else
	{
		context->dfi->dfb->GetDisplayLayer(context->dfi->dfb, 0, &(context->dfi->layer));
		context->dfi->layer->EnableCursor(context->dfi->layer, 1);
	}
}

boolean df_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	dfInfo* dfi;
	dfContext* context;
	int flags;

	context = ((dfContext*) instance->context);
	dfi = context->dfi;
	dfi->err = DirectFBCreate(&(dfi->dfb));
	if (dfi->err!=DFB_OK)
	{
		printf("DirectFB init failed! err=0x%x\n",  dfi->err);
		return false;
	}

	if (((rdpContext *)context)->instance->settings->fullscreen)
		dfi->dfb->SetCooperativeLevel(dfi->dfb, DFSCL_FULLSCREEN);

	dfi->dsc.flags = DSDESC_CAPS;
	dfi->dsc.caps = DSCAPS_PRIMARY;
	dfi->err = dfi->dfb->CreateSurface(dfi->dfb, &(dfi->dsc), &(dfi->primary));
	if (dfi->err!=DFB_OK)
	{
		printf("DirectFB surface failed! err=0x%x\n",  dfi->err);
		return false;
	}

	if (context->direct_surface)
	{
		if (!df_lock_fb(dfi, DF_LOCK_BIT_INIT))
			return false;

		gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_16BPP | CLRBUF_32BPP, dfi->primary_data);
		gdi = instance->context->gdi;
		df_run_register(instance);


		dfi->err = dfi->primary->GetSize(dfi->primary, &(gdi->width), &(gdi->height));
		if (dfi->err!=DFB_OK)
		{
			printf("DirectFB query surface size failed! err=0x%x\n",  dfi->err);
			return false;
		}
		df_unlock_fb(dfi, DF_LOCK_BIT_INIT);

		gdi->primary_buffer = 0;
		gdi->primary->bitmap->data = 0;

		dfi->err = dfi->dfb->SetVideoMode(dfi->dfb, gdi->width, gdi->height, gdi->dstBpp);
		printf("SetVideoMode %dx%dx%d 0x%x\n", gdi->width, gdi->height, gdi->dstBpp, dfi->err);


		dfi->dfb->CreateInputEventBuffer(dfi->dfb, DICAPS_ALL, DFB_TRUE, &(dfi->event_buffer));
		dfi->event_buffer->CreateFileDescriptor(dfi->event_buffer, &(dfi->read_fds));

		flags = fcntl(dfi->read_fds, F_GETFL, 0);
    	if ( flags == -1 || fcntl(dfi->read_fds, F_SETFL, flags | O_NONBLOCK) == -1 )
	    {
    	    perror("DirectFB non-blocking mode");
	        return false;
	    }
		dfi->read_len_pending = 0;

		df_init_cursor(context);
		if (!df_lock_fb(dfi, DF_LOCK_BIT_INIT))
			return false;

		df_keyboard_init();

		pointer_cache_register_callbacks(instance->update);
		df_register_graphics(instance->context->graphics);

		freerdp_channels_post_connect(instance->context->channels, instance);
		df_unlock_fb(dfi, DF_LOCK_BIT_INIT);
		printf("DirectFB client initialized in experimental --direct-surface option!\n");
		return true;
	}

	gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_16BPP | CLRBUF_32BPP, NULL);
	gdi = instance->context->gdi;
	df_run_register(instance);

	dfi->err = dfi->primary->GetSize(dfi->primary, &(gdi->width), &(gdi->height));
	if (dfi->err!=DFB_OK)
	{
		printf("DirectFB query surface size failed! err=0x%x\n",  dfi->err);
		return false;
	}

	dfi->err = dfi->dfb->SetVideoMode(dfi->dfb, gdi->width, gdi->height, gdi->dstBpp);

	printf("SetVideoMode %dx%dx%d 0x%x\n", gdi->width, gdi->height, gdi->dstBpp, dfi->err);

	dfi->dfb->CreateInputEventBuffer(dfi->dfb, DICAPS_ALL, DFB_TRUE, &(dfi->event_buffer));
	dfi->event_buffer->CreateFileDescriptor(dfi->event_buffer, &(dfi->read_fds));

	flags = fcntl(dfi->read_fds, F_GETFL, 0);
    if ( flags == -1 || fcntl(dfi->read_fds, F_SETFL, flags | O_NONBLOCK) == -1 )
    {
        perror("DirectFB non-blocking mode");
        return false;
    }
	dfi->read_len_pending = 0;

	df_init_cursor(context);

	dfi->dsc.flags = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PREALLOCATED | DSDESC_PIXELFORMAT;
	dfi->dsc.caps = DSCAPS_SYSTEMONLY;
	dfi->dsc.width = gdi->width;
	dfi->dsc.height = gdi->height;

	if (gdi->dstBpp == 32 || gdi->dstBpp == 24)
		dfi->dsc.pixelformat = DSPF_AiRGB;
	else if (gdi->dstBpp == 16 || gdi->dstBpp == 15)
		dfi->dsc.pixelformat = DSPF_RGB16;
	else if (gdi->dstBpp == 8)
		dfi->dsc.pixelformat = DSPF_RGB332;
	else
		dfi->dsc.pixelformat = DSPF_AiRGB;

	dfi->dsc.preallocated[0].data = gdi->primary_buffer;
	dfi->dsc.preallocated[0].pitch = gdi->width * gdi->bytesPerPixel;
	dfi->dfb->CreateSurface(dfi->dfb, &(dfi->dsc), &(dfi->secondary));

	df_keyboard_init();

	pointer_cache_register_callbacks(instance->update);
	df_register_graphics(instance->context->graphics);

	freerdp_channels_post_connect(instance->context->channels, instance);
	return true;
}

static int df_process_plugin_args(rdpSettings* settings, const char* name,
	RDP_PLUGIN_DATA* plugin_data, void* user_data)
{
	rdpChannels* channels = (rdpChannels*) user_data;

	printf("loading plugin %s\n", name);
	freerdp_channels_load_plugin(channels, settings, name, plugin_data);

	return 1;
}

boolean df_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint)
{
	printf("Certificate details:\n");
	printf("\tSubject: %s\n", subject);
	printf("\tIssuer: %s\n", issuer);
	printf("\tThumbprint: %s\n", fingerprint);
	printf("The above X.509 certificate could not be verified, possibly because you do not have "
		"the CA certificate in your certificate store, or the certificate has expired. "
		"Please look at the documentation on how to create local certificate store for a private CA.\n");

	char answer;
	while (1)
	{
		printf("Do you trust the above certificate? (Y/N) ");
		answer = fgetc(stdin);

		if (answer == 'y' || answer == 'Y')
		{
			return true;
		}
		else if (answer == 'n' || answer == 'N')
		{
			break;
		}
	}

	return false;
}

static int
df_receive_channel_data(freerdp* instance, int channelId, uint8* data, int size, int flags, int total_size)
{
	return freerdp_channels_data(instance, channelId, data, size, flags, total_size);
}


void* thread_func(void* param)
{
	struct thread_data* data;
	data = (struct thread_data*) param;

	df_run(data->instance);
	xfree(data);

	pthread_detach(pthread_self());

	g_thread_count--;

        if (g_thread_count < 1)
                freerdp_sem_signal(g_sem);

	return NULL;
}


int main(int argc, char* argv[])
{
	int i;
	pthread_t thread;
	freerdp* instance;
	dfContext* context;
	rdpChannels* channels;
	struct thread_data* data;

	setlocale(LC_ALL, "");

	freerdp_channels_global_init();

	g_sem = freerdp_sem_new(1);

	instance = freerdp_new();
	instance->PreConnect = df_pre_connect;
	instance->PostConnect = df_post_connect;
	instance->VerifyCertificate = df_verify_certificate;
	instance->ReceiveChannelData = df_receive_channel_data;

	instance->context_size = sizeof(dfContext);
	instance->ContextNew = df_context_new;
	instance->ContextFree = df_context_free;
	freerdp_context_new(instance);

	context = (dfContext*) instance->context;
	channels = instance->context->channels;
	
	DirectFBInit(&argc, &argv);
	if (freerdp_parse_args(instance->settings, argc, argv, df_process_plugin_args, channels, NULL, NULL)==FREERDP_ARGS_PARSE_HELP)
	{
		printf("  --direct-surface: Use only single DirectFB surface (faster, but repaints more 'visible').\n");
		printf("  --direct-flip: Can be neccessary with non-fullscreen mode if DirectFB setting 'autoflip-window' is not enabled. Don't use it if all works without it.\n");
		printf("\n");
		return 1;
	}

	for (i = 0; i<argc; ++i)
	{
		if (strcmp(argv[i], "--direct-surface")==0)
		{
			context->direct_surface = true;
		}
		else if (strcmp(argv[i], "--direct-flip")==0)
		{
			context->direct_flip = true;
			if (instance->settings->fullscreen)
				printf("WARNING: --direct-flip is not recommended to use with fullscreen mode. This probably will only defeat performance without any profit.\n");
		}
	}
	data = (struct thread_data*) xzalloc(sizeof(struct thread_data));
	data->instance = instance;

	g_thread_count++;
	pthread_create(&thread, 0, thread_func, data);

	while (g_thread_count > 0)
	{
		freerdp_sem_wait(g_sem);
	}

	freerdp_channels_global_uninit();

	return 0;
}
