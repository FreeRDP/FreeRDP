/*
   Android JNI Client Layer

   Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz
   Copyright 2013 Thincast Technologies GmbH, Author: Armin Novak

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>

#include <freerdp/graphics.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/channels/channels.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/constants.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/primitives.h>
#include <freerdp/version.h>

#include <android/bitmap.h>

#include "android_freerdp.h"
#include "android_jni_callback.h"
#include "android_jni_utils.h"
#include "android_debug.h"
#include "android_cliprdr.h"

#if defined(WITH_GPROF)
#include "jni/prof.h"
#endif

int android_context_new(freerdp* instance, rdpContext* context)
{
	context->channels = freerdp_channels_new();
	android_event_queue_init(instance);
	return 0;
}

void android_context_free(freerdp* instance, rdpContext* context)
{
	freerdp_channels_close(instance->context->channels, instance);
	freerdp_channels_free(context->channels);
	android_event_queue_uninit(instance);
}

void android_OnChannelConnectedEventHandler(rdpContext* context, ChannelConnectedEventArgs* e)
{
	rdpSettings* settings = context->settings;
	androidContext* afc = (androidContext*) context;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{

	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
			gdi_graphics_pipeline_init(context->gdi, (RdpgfxClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		android_cliprdr_init(afc, (CliprdrClientContext*) e->pInterface);
	}
}

void android_OnChannelDisconnectedEventHandler(rdpContext* context, ChannelDisconnectedEventArgs* e)
{
	rdpSettings* settings = context->settings;
	androidContext* afc = (androidContext*) context;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{

	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
			gdi_graphics_pipeline_uninit(context->gdi, (RdpgfxClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		android_cliprdr_uninit(afc, (CliprdrClientContext*) e->pInterface);
	}
}

void android_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
	gdi->primary->hdc->hwnd->ninvalid = 0;
}

void android_end_paint(rdpContext* context)
{
	androidContext *ctx = (androidContext*)context;
	rdpSettings* settings = context->instance->settings;
	
	DEBUG_ANDROID("ui_update");

	assert(ctx);
	assert(settings);
	assert(context->instance);

	DEBUG_ANDROID("width=%d, height=%d, bpp=%d", settings->DesktopWidth,
			settings->DesktopHeight, settings->ColorDepth);
	
	freerdp_callback("OnGraphicsUpdate", "(IIIII)V", context->instance,
		0, 0, settings->DesktopWidth, settings->DesktopHeight);
}

void android_desktop_resize(rdpContext* context)
{
	DEBUG_ANDROID("ui_desktop_resize");

	assert(context);
	assert(context->settings);
	assert(context->instance);

	freerdp_callback("OnGraphicsResize", "(IIII)V",
			context->instance, context->settings->DesktopWidth,
			context->settings->DesktopHeight, context->settings->ColorDepth);
}

BOOL android_pre_connect(freerdp* instance)
{
	DEBUG_ANDROID("android_pre_connect");

	rdpSettings* settings = instance->settings;
	BOOL bitmap_cache = settings->BitmapCacheEnabled;
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
	settings->OrderSupport[NEG_MEMBLT_INDEX] = bitmap_cache;
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
	settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;

	settings->FrameAcknowledge = 10;

	PubSub_SubscribeChannelConnected(instance->context->pubSub,
			(pChannelConnectedEventHandler) android_OnChannelConnectedEventHandler);

	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
			(pChannelDisconnectedEventHandler) android_OnChannelDisconnectedEventHandler);

	freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);
	freerdp_client_load_addins(instance->context->channels, instance->settings);

	freerdp_channels_pre_connect(instance->context->channels, instance);

	return TRUE;
}

static BOOL android_post_connect(freerdp* instance)
{
	UINT32 gdi_flags;
	rdpSettings *settings = instance->settings;

	DEBUG_ANDROID("android_post_connect");

	assert(instance);
	assert(settings);

	freerdp_callback("OnSettingsChanged", "(IIII)V", instance,
			settings->DesktopWidth, settings->DesktopHeight,
			settings->ColorDepth);

	instance->context->cache = cache_new(settings);

	if (instance->settings->ColorDepth > 16)
		gdi_flags = CLRBUF_32BPP | CLRCONV_ALPHA | CLRCONV_INVERT;
	else
		gdi_flags = CLRBUF_16BPP;

	gdi_init(instance, gdi_flags, NULL);

	instance->update->BeginPaint = android_begin_paint;
	instance->update->EndPaint = android_end_paint;
	instance->update->DesktopResize = android_desktop_resize;

	freerdp_channels_post_connect(instance->context->channels, instance);

	freerdp_callback("OnConnectionSuccess", "(I)V", instance);

	return TRUE;
}

static void android_post_disconnect(freerdp* instance)
{
	gdi_free(instance);
	cache_free(instance->context->cache);
}

BOOL android_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	DEBUG_ANDROID("Authenticate user:");
	DEBUG_ANDROID("  Username: %s", *username);
	DEBUG_ANDROID("  Domain: %s", *domain);

	JNIEnv* env;
	jboolean attached = jni_attach_thread(&env);
	jobject jstr1 = create_string_builder(env, *username);
	jobject jstr2 = create_string_builder(env, *domain);
	jobject jstr3 = create_string_builder(env, *password);

	jboolean res = freerdp_callback_bool_result("OnAuthenticate", "(ILjava/lang/StringBuilder;Ljava/lang/StringBuilder;Ljava/lang/StringBuilder;)Z", instance, jstr1, jstr2, jstr3);

	if (res == JNI_TRUE)
	{
		// read back string values
		if (*username != NULL)
			free(*username);

		*username = get_string_from_string_builder(env, jstr1);

		if (*domain != NULL)
			free(*domain);

		*domain = get_string_from_string_builder(env, jstr2);

		if (*password == NULL)
			free(*password);
		
		*password = get_string_from_string_builder(env, jstr3);
	}

	if (attached == JNI_TRUE)
		jni_detach_thread();

	return ((res == JNI_TRUE) ? TRUE : FALSE);
}

BOOL android_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint)
{
	DEBUG_ANDROID("Certificate details:");
	DEBUG_ANDROID("\tSubject: %s", subject);
	DEBUG_ANDROID("\tIssuer: %s", issuer);
	DEBUG_ANDROID("\tThumbprint: %s", fingerprint);
	DEBUG_ANDROID("The above X.509 certificate could not be verified, possibly because you do not have "
		"the CA certificate in your certificate store, or the certificate has expired."
		"Please look at the documentation on how to create local certificate store for a private CA.\n");

	JNIEnv* env;
	jboolean attached = jni_attach_thread(&env);
	jstring jstr1 = (*env)->NewStringUTF(env, subject);
	jstring jstr2 = (*env)->NewStringUTF(env, issuer);
	jstring jstr3 = (*env)->NewStringUTF(env, fingerprint);

	jboolean res = freerdp_callback_bool_result("OnVerifyCertificate", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z", instance, jstr1, jstr2, jstr3);

	if (attached == JNI_TRUE)
		jni_detach_thread();

	return ((res == JNI_TRUE) ? TRUE : FALSE);
}

BOOL android_verify_changed_certificate(freerdp* instance, char* subject, char* issuer, char* new_fingerprint, char* old_fingerprint)
{
	return android_verify_certificate(instance, subject, issuer, new_fingerprint);
}

static void* jni_input_thread(void* arg)
{
	HANDLE event[3];
	wMessageQueue* queue;
	freerdp* instance = (freerdp*) arg;
	androidContext *aCtx = (androidContext*)instance->context;
	
	assert(NULL != instance);
	assert(NULL != aCtx);
														  
	DEBUG_ANDROID("Start.");

	queue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE);
	event[0] = CreateFileDescriptorEvent(NULL, FALSE, FALSE, aCtx->event_queue->pipe_fd[0]);
	event[1] = CreateFileDescriptorEvent(NULL, FALSE, FALSE, aCtx->event_queue->pipe_fd[1]);
	event[2] = freerdp_get_message_queue_event_handle(instance, FREERDP_INPUT_MESSAGE_QUEUE);
			
	do
	{
		DWORD rc = WaitForMultipleObjects(3, event, FALSE, INFINITE);
		if ((rc < WAIT_OBJECT_0) || (rc > WAIT_OBJECT_0 + 2))
			continue;
	
		if (rc == WAIT_OBJECT_0 + 2)
		{
			wMessage msg;

			MessageQueue_Peek(queue, &msg, FALSE);
			if (msg.id == WMQ_QUIT)
				break;
		}
		if (android_check_fds(instance) != TRUE)
			break;
	}
	while(1);

	DEBUG_ANDROID("Quit.");
	
	MessageQueue_PostQuit(queue, 0);
	ExitThread(0);
	return NULL;
}

static void* jni_channels_thread(void* arg)
{     
	int status;
	HANDLE event;
	rdpChannels* channels;
	freerdp* instance = (freerdp*) arg;
	
	assert(NULL != instance);
											  
	DEBUG_ANDROID("Start.");

	channels = instance->context->channels;
	event = freerdp_channels_get_event_handle(instance);

	while (WaitForSingleObject(event, INFINITE) == WAIT_OBJECT_0)
	{
		status = freerdp_channels_process_pending_messages(instance);

		if (!status)
			break;
	}
	
	DEBUG_ANDROID("Quit.");

	ExitThread(0);
	return NULL;
} 

static int android_freerdp_run(freerdp* instance)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	int wcount;
	int fd_input_event;
	HANDLE input_event = NULL;
	void* rfds[32];
	void* wfds[32];
	fd_set rfds_set;
	fd_set wfds_set;
	int select_status;
	struct timeval timeout;

	const rdpSettings* settings = instance->context->settings;

	HANDLE input_thread;
	HANDLE channels_thread;
	
	BOOL async_input = settings->AsyncInput;
	BOOL async_channels = settings->AsyncChannels;
	BOOL async_transport = settings->AsyncTransport;

	DEBUG_ANDROID("AsyncUpdate=%d", settings->AsyncUpdate);
	DEBUG_ANDROID("AsyncInput=%d", settings->AsyncInput);
	DEBUG_ANDROID("AsyncChannels=%d", settings->AsyncChannels);
	DEBUG_ANDROID("AsyncTransport=%d", settings->AsyncTransport);

	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));

	if (!freerdp_connect(instance))
	{
		freerdp_callback("OnConnectionFailure", "(I)V", instance);
		return 0;
	}

	if (async_input)
	{
		input_thread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE) jni_input_thread, instance, 0, NULL);
	}
	      
	if (async_channels)
	{
		channels_thread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE) jni_channels_thread, instance, 0, NULL);
	}

	((androidContext*)instance->context)->is_connected = TRUE;
	while (!freerdp_shall_disconnect(instance))
	{
		rcount = 0;
		wcount = 0;

		if (!async_transport)
		{
			if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
			{
				DEBUG_ANDROID("Failed to get FreeRDP file descriptor\n");
				break;
			}
		}

		if (!async_channels)
		{
			if (freerdp_channels_get_fds(instance->context->channels, instance, rfds, &rcount, wfds, &wcount) != TRUE)
			{
				DEBUG_ANDROID("Failed to get channel manager file descriptor\n");
				break;
			}
		}

		if (!async_input)
		{
			if (android_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
			{
				DEBUG_ANDROID("Failed to get android file descriptor\n");
				break;
			}
		}
		else
		{
			input_event = freerdp_get_message_queue_event_handle(instance, FREERDP_INPUT_MESSAGE_QUEUE);
			fd_input_event = GetEventFileDescriptor(input_event);
			rfds[rcount++] = (void*) (long) fd_input_event;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);
		FD_ZERO(&wfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}

		if (max_fds == 0)
			break;

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		select_status = select(max_fds + 1, &rfds_set, NULL, NULL, &timeout);

		if (select_status == 0)
			continue;
		else if (select_status == -1)
		{
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR))) /* signal occurred */
			{
				DEBUG_ANDROID("android_run: select failed\n");
				break;
			}
		}
		
		if (freerdp_shall_disconnect(instance))
			break;

		if (!async_transport)
		{
			if (freerdp_check_fds(instance) != TRUE)
			{
				DEBUG_ANDROID("Failed to check FreeRDP file descriptor\n");
				break;
			}
		}

		if (!async_input)
		{
			if (android_check_fds(instance) != TRUE)
			{
				DEBUG_ANDROID("Failed to check android file descriptor\n");
				break;
			}
		}
		else if (input_event)
		{
			if (WaitForSingleObject(input_event, 0) == WAIT_OBJECT_0)
			{
				if (!freerdp_message_queue_process_pending_messages(instance,
							FREERDP_INPUT_MESSAGE_QUEUE))
				{
					DEBUG_ANDROID("User Disconnect");
					break;
				}
			}
		}

		if (!async_channels)
		{
			if (freerdp_channels_check_fds(instance->context->channels, instance) != TRUE)
			{
				DEBUG_ANDROID("Failed to check channel manager file descriptor\n");
				break;
			}
		}
	}

	DEBUG_ANDROID("Prepare shutdown...");

	// issue another OnDisconnecting here in case the disconnect was initiated by the server and not our client
	freerdp_callback("OnDisconnecting", "(I)V", instance);
	
	DEBUG_ANDROID("Close channels...");
	freerdp_channels_disconnect(instance->context->channels, instance);

	DEBUG_ANDROID("Cleanup threads...");

	if (async_channels)
	{
		WaitForSingleObject(channels_thread, INFINITE);
		CloseHandle(channels_thread);
	}
 
	if (async_input)
	{
		wMessageQueue* input_queue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE);
		MessageQueue_PostQuit(input_queue, 0);
		WaitForSingleObject(input_thread, INFINITE);
		CloseHandle(input_thread);
	}

	DEBUG_ANDROID("Disconnecting...");
	freerdp_disconnect(instance);
	freerdp_callback("OnDisconnected", "(I)V", instance);

	DEBUG_ANDROID("Quit.");

	return 0;
}

static void* android_thread_func(void* param)
{
	freerdp* instance = param;
	
	DEBUG_ANDROID("Start.");

	assert(instance);

	android_freerdp_run(instance);

	DEBUG_ANDROID("Quit.");

	ExitThread(0);
	return NULL;
}

JNIEXPORT jint JNICALL jni_freerdp_new(JNIEnv *env, jclass cls)
{
	freerdp* instance;

#if defined(WITH_GPROF)
	setenv("CPUPROFILE_FREQUENCY", "200", 1);
	monstartup("libfreerdp-android.so");
#endif

	// create instance
	instance = freerdp_new();
	instance->PreConnect = android_pre_connect;
	instance->PostConnect = android_post_connect;
	instance->PostDisconnect = android_post_disconnect;
	instance->Authenticate = android_authenticate;
	instance->VerifyCertificate = android_verify_certificate;
	instance->VerifyChangedCertificate = android_verify_changed_certificate;

	// create context
	instance->ContextSize = sizeof(androidContext);
	instance->ContextNew = android_context_new;
	instance->ContextFree = android_context_free;
	freerdp_context_new(instance);

	return (jint) instance;
}

JNIEXPORT void JNICALL jni_freerdp_free(JNIEnv *env, jclass cls, jint instance)
{
	freerdp* inst = (freerdp*)instance;

	freerdp_context_free(inst);
	freerdp_free(inst);

#if defined(WITH_GPROF)
	moncleanup();
#endif
}

JNIEXPORT jboolean JNICALL jni_freerdp_connect(JNIEnv *env, jclass cls, jint instance)
{
	freerdp* inst = (freerdp*)instance;
	androidContext* ctx = (androidContext*)inst->context;

	assert(inst);
	assert(ctx);

	ctx->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE)android_thread_func, inst, 0, NULL);

	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL jni_freerdp_disconnect(JNIEnv *env, jclass cls, jint instance)
{
	freerdp* inst = (freerdp*)instance;
	androidContext* ctx = (androidContext*)inst->context;
	ANDROID_EVENT* event = (ANDROID_EVENT*)android_event_disconnect_new();

	assert(inst);
	assert(ctx);
	assert(event);

	android_push_event(inst, event);

	WaitForSingleObject(ctx->thread, INFINITE);
	CloseHandle(ctx->thread);
	ctx->thread = NULL;

	freerdp_callback("OnDisconnecting", "(I)V", instance);

	return (jboolean) JNI_TRUE;
}

JNIEXPORT void JNICALL jni_freerdp_cancel_connection(JNIEnv *env, jclass cls, jint instance)
{
	jni_freerdp_disconnect(env, cls, instance);
}

JNIEXPORT void JNICALL jni_freerdp_set_data_directory(JNIEnv *env, jclass cls, jint instance, jstring jdirectory)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;

	const jbyte *directory = (*env)->GetStringUTFChars(env, jdirectory, NULL);

	free(settings->HomePath);
	free(settings->ConfigPath);

	int config_dir_len = strlen(directory) + 10; /* +9 chars for /.freerdp and +1 for \0 */
	char* config_dir_buf = (char*)malloc(config_dir_len);
	strcpy(config_dir_buf, directory);
	strcat(config_dir_buf, "/.freerdp");
	settings->HomePath = strdup(directory);
	settings->ConfigPath = config_dir_buf;	/* will be freed by freerdp library */

	(*env)->ReleaseStringUTFChars(env, jdirectory, directory);
}

JNIEXPORT void JNICALL jni_freerdp_set_connection_info(JNIEnv *env, jclass cls, jint instance,
						   jstring jhostname, jstring jusername, jstring jpassword, jstring jdomain, jint width, jint height,
						   jint color_depth, jint port, jboolean console, jint security, jstring jcertname)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;

	const jbyte *hostname = (*env)->GetStringUTFChars(env, jhostname, NULL);
	const jbyte *username = (*env)->GetStringUTFChars(env, jusername, NULL);
	const jbyte *password = (*env)->GetStringUTFChars(env, jpassword, NULL);
	const jbyte *domain = (*env)->GetStringUTFChars(env, jdomain, NULL);
	const jbyte *certname = (*env)->GetStringUTFChars(env, jcertname, NULL);

	DEBUG_ANDROID("hostname: %s", (char*) hostname);
	DEBUG_ANDROID("username: %s", (char*) username);
	DEBUG_ANDROID("password: %s", (char*) password);
	DEBUG_ANDROID("domain: %s", (char*) domain);
	DEBUG_ANDROID("width: %d", width);
	DEBUG_ANDROID("height: %d", height);
	DEBUG_ANDROID("color depth: %d", color_depth);
	DEBUG_ANDROID("port: %d", port);
	DEBUG_ANDROID("security: %d", security);

	settings->DesktopWidth = width;
	settings->DesktopHeight = height;
	settings->ColorDepth = color_depth;
	settings->ServerPort = port;

	// Hack for 16 bit RDVH connections:
	//   In this case we get screen corruptions when we have an odd screen resolution width ... need to investigate what is causing this...
	if (color_depth <= 16)
		settings->DesktopWidth &= (~1);

	settings->ServerHostname = strdup(hostname);

	if (username && strlen(username) > 0)
		settings->Username = strdup(username);

	if (password && strlen(password) > 0)
	{
		settings->Password = strdup(password);
		settings->AutoLogonEnabled = TRUE;
	}

	settings->Domain = strdup(domain);

	if (certname && strlen(certname) > 0)
		settings->CertificateName = strdup(certname);

	settings->ConsoleSession = (console == JNI_TRUE) ? TRUE : FALSE;

	settings->SoftwareGdi = TRUE;
	settings->BitmapCacheV3Enabled = TRUE;

	switch ((int) security)
	{
		case 1:
			/* Standard RDP */
			settings->RdpSecurity = TRUE;
			settings->TlsSecurity = FALSE;
			settings->NlaSecurity = FALSE;
			settings->ExtSecurity = FALSE;
			settings->UseRdpSecurityLayer = TRUE;
			break;

		case 2:
			/* TLS */
			settings->NlaSecurity = FALSE;
			settings->TlsSecurity = TRUE;
			settings->RdpSecurity = FALSE;
			settings->ExtSecurity = FALSE;
			break;

		case 3:
			/* NLA */
			settings->NlaSecurity = TRUE;
			settings->TlsSecurity = FALSE;
			settings->RdpSecurity = FALSE;
			settings->ExtSecurity = FALSE;
			break;

		default:
			break;
	}

	// set US keyboard layout
	settings->KeyboardLayout = 0x0409;

	(*env)->ReleaseStringUTFChars(env, jhostname, hostname);
	(*env)->ReleaseStringUTFChars(env, jusername, username);
	(*env)->ReleaseStringUTFChars(env, jpassword, password);
	(*env)->ReleaseStringUTFChars(env, jdomain, domain);
	(*env)->ReleaseStringUTFChars(env, jcertname, certname);

	return;
}

JNIEXPORT void JNICALL jni_freerdp_set_performance_flags(
	JNIEnv *env, jclass cls, jint instance, jboolean remotefx,
	jboolean disableWallpaper, jboolean disableFullWindowDrag,
	jboolean disableMenuAnimations, jboolean disableTheming,
	jboolean enableFontSmoothing, jboolean enableDesktopComposition)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;
	
	DEBUG_ANDROID("remotefx: %d", (remotefx == JNI_TRUE) ? 1 : 0);
	if (remotefx == JNI_TRUE)
	{
		settings->RemoteFxCodec = TRUE;
		settings->FastPathOutput = TRUE;
		settings->ColorDepth = 32;
		settings->LargePointerFlag = TRUE;
		settings->FrameMarkerCommandEnabled = TRUE;
	}
	else
	{
		/* enable NSCodec if we don't use remotefx */
		settings->NSCodec = TRUE;
	}

	/* store performance settings */
	settings->DisableWallpaper = (disableWallpaper == JNI_TRUE) ? TRUE : FALSE;
	settings->DisableFullWindowDrag = (disableFullWindowDrag == JNI_TRUE) ? TRUE : FALSE;
	settings->DisableMenuAnims = (disableMenuAnimations == JNI_TRUE) ? TRUE : FALSE;
	settings->DisableThemes = (disableTheming == JNI_TRUE) ? TRUE : FALSE;
	settings->AllowFontSmoothing = (enableFontSmoothing == JNI_TRUE) ? TRUE : FALSE;
	settings->AllowDesktopComposition = (enableDesktopComposition == JNI_TRUE) ? TRUE : FALSE;

	/* Create performance flags from settings */
	freerdp_performance_flags_make(settings);

	DEBUG_ANDROID("performance_flags: %04X", settings->PerformanceFlags);
}

JNIEXPORT void JNICALL jni_freerdp_set_advanced_settings(JNIEnv *env, jclass cls,
		jint instance, jstring jRemoteProgram, jstring jWorkDir,
		jboolean async_channel, jboolean async_transport, jboolean async_input,
		jboolean async_update)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;

	const jbyte *remote_program = (*env)->GetStringUTFChars(env, jRemoteProgram, NULL);
	const jbyte *work_dir = (*env)->GetStringUTFChars(env, jWorkDir, NULL);

	DEBUG_ANDROID("Remote Program: %s", (char*) remote_program);
	DEBUG_ANDROID("Work Dir: %s", (char*) work_dir);

	/* Enable async mode. */
	settings->AsyncUpdate = async_update;
	settings->AsyncChannels = async_channel;
	settings->AsyncTransport = async_transport;
	settings->AsyncInput = async_input;

	if (remote_program && strlen(remote_program) > 0)
		settings->AlternateShell = strdup(remote_program);

	if (work_dir && strlen(work_dir) > 0)
		settings->ShellWorkingDirectory = strdup(work_dir);

	(*env)->ReleaseStringUTFChars(env, jRemoteProgram, remote_program);
	(*env)->ReleaseStringUTFChars(env, jWorkDir, work_dir);
}

JNIEXPORT void JNICALL jni_freerdp_set_drive_redirection(JNIEnv *env, jclass cls, jint instance, jstring jpath)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;
	char* args[] = {"drive", "Android", ""};

	const jbyte *path = (*env)->GetStringUTFChars(env, jpath, NULL);
	DEBUG_ANDROID("drive redirect: %s", (char*)path);

	args[2] = (char*)path;
	freerdp_client_add_device_channel(settings, 3, args);
	settings->DeviceRedirection = TRUE;

	(*env)->ReleaseStringUTFChars(env, jpath, path);
}

JNIEXPORT void JNICALL jni_freerdp_set_sound_redirection(JNIEnv *env,
		jclass cls, jint instance, jint redirect)
{
	char** p;
	int count = 1;
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;

	DEBUG_ANDROID("sound: %s",
			redirect ? ((redirect == 1) ? "Server" : "Redirect") : "None");

	settings->AudioPlayback = (redirect == 2) ? TRUE : FALSE;
	settings->RemoteConsoleAudio = (redirect == 1) ? TRUE : FALSE;
	if (settings->AudioPlayback)
	{
		p = malloc(sizeof(char*));
		p[0] = "rdpsnd";

		freerdp_client_add_static_channel(settings, count, p);

		free(p);
	}
}

JNIEXPORT void JNICALL jni_freerdp_set_microphone_redirection(JNIEnv *env,
		jclass cls, jint instance, jboolean enable)
{
	char** p;
	int count = 1;
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;

	DEBUG_ANDROID("microphone redirect: %s", enable ? "TRUE" : "FALSE");

	settings->AudioCapture = enable;
	if (enable)
	{
		p = malloc(sizeof(char*));
		p[0] = "audin";

		freerdp_client_add_dynamic_channel(settings, count, p);

		free(p);
	}
}

JNIEXPORT void JNICALL jni_freerdp_set_clipboard_redirection(JNIEnv *env, jclass cls, jint instance, jboolean enable)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;

	DEBUG_ANDROID("clipboard redirect: %s", enable ? "TRUE" : "FALSE");

	settings->RedirectClipboard = enable ? TRUE : FALSE;
}

JNIEXPORT void JNICALL jni_freerdp_set_gateway_info(JNIEnv *env, jclass cls, jint instance, jstring jgatewayhostname, jint port, 
													jstring jgatewayusername, jstring jgatewaypassword, jstring jgatewaydomain)
{
	freerdp* inst = (freerdp*)instance;
	rdpSettings * settings = inst->settings;

	const jbyte *gatewayhostname = (*env)->GetStringUTFChars(env, jgatewayhostname, NULL);
	const jbyte *gatewayusername = (*env)->GetStringUTFChars(env, jgatewayusername, NULL);
	const jbyte *gatewaypassword = (*env)->GetStringUTFChars(env, jgatewaypassword, NULL);
	const jbyte *gatewaydomain = (*env)->GetStringUTFChars(env, jgatewaydomain, NULL);

	DEBUG_ANDROID("gatewayhostname: %s", (char*) gatewayhostname);
	DEBUG_ANDROID("gatewayport: %d", port);
	DEBUG_ANDROID("gatewayusername: %s", (char*) gatewayusername);
	DEBUG_ANDROID("gatewaypassword: %s", (char*) gatewaypassword);
	DEBUG_ANDROID("gatewaydomain: %s", (char*) gatewaydomain);

	settings->GatewayHostname = strdup(gatewayhostname);
	settings->GatewayPort     = port;
	settings->GatewayUsername = strdup(gatewayusername);
	settings->GatewayPassword = strdup(gatewaypassword);
	settings->GatewayDomain = strdup(gatewaydomain);
	settings->GatewayUsageMethod = TSC_PROXY_MODE_DIRECT;
	settings->GatewayEnabled = TRUE;
	settings->GatewayUseSameCredentials = FALSE;

	(*env)->ReleaseStringUTFChars(env, jgatewayhostname, gatewayhostname);
	(*env)->ReleaseStringUTFChars(env, jgatewayusername, gatewayusername);
	(*env)->ReleaseStringUTFChars(env, jgatewaypassword, gatewaypassword);
	(*env)->ReleaseStringUTFChars(env, jgatewaydomain, gatewaydomain);
}

static void copy_pixel_buffer(UINT8* dstBuf, UINT8* srcBuf, int x, int y, int width, int height, int wBuf, int hBuf, int bpp)
{
	int i;
	int length;
	int scanline;
	UINT8 *dstp, *srcp;
	
	length = width * bpp;
	scanline = wBuf * bpp;

	srcp = (UINT8*) &srcBuf[(scanline * y) + (x * bpp)];
	dstp = (UINT8*) &dstBuf[(scanline * y) + (x * bpp)];

	for (i = 0; i < height; i++)
	{
		memcpy(dstp, srcp, length);
		srcp += scanline;
		dstp += scanline;
	}
}

JNIEXPORT jboolean JNICALL jni_freerdp_update_graphics(
	JNIEnv *env, jclass cls, jint instance, jobject bitmap, jint x, jint y, jint width, jint height)
{

	int ret;
	void* pixels;
	AndroidBitmapInfo info;
	freerdp* inst = (freerdp*)instance;
	rdpGdi *gdi = inst->context->gdi;

	if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0)
	{
		DEBUG_ANDROID("AndroidBitmap_getInfo() failed ! error=%d", ret);
		return JNI_FALSE;
	}

	if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0)
	{
		DEBUG_ANDROID("AndroidBitmap_lockPixels() failed ! error=%d", ret);
		return JNI_FALSE;
	}

	copy_pixel_buffer(pixels, gdi->primary_buffer, x, y, width, height, gdi->width, gdi->height, gdi->bytesPerPixel);

	AndroidBitmap_unlockPixels(env, bitmap);

	return JNI_TRUE;
}

JNIEXPORT void JNICALL jni_freerdp_send_key_event(
	JNIEnv *env, jclass cls, jint instance, jint keycode, jboolean down)
{
	DWORD scancode;
	ANDROID_EVENT* event;

	freerdp* inst = (freerdp*)instance;

	scancode = GetVirtualScanCodeFromVirtualKeyCode(keycode, 4);
	int flags = (down == JNI_TRUE) ? KBD_FLAGS_DOWN : KBD_FLAGS_RELEASE;
	flags |= (scancode & KBDEXT) ? KBD_FLAGS_EXTENDED : 0;
	event = (ANDROID_EVENT*) android_event_key_new(flags, scancode & 0xFF);

	android_push_event(inst, event);

	DEBUG_ANDROID("send_key_event: %d, %d", (int)scancode, flags);
}

JNIEXPORT void JNICALL jni_freerdp_send_unicodekey_event(
	JNIEnv *env, jclass cls, jint instance, jint keycode)
{
	ANDROID_EVENT* event;

	freerdp* inst = (freerdp*)instance;
	event = (ANDROID_EVENT*) android_event_unicodekey_new(keycode);
	android_push_event(inst, event);

	DEBUG_ANDROID("send_unicodekey_event: %d", keycode);
}

JNIEXPORT void JNICALL jni_freerdp_send_cursor_event(
	JNIEnv *env, jclass cls, jint instance, jint x, jint y, jint flags)
{
	ANDROID_EVENT* event;

	freerdp* inst = (freerdp*)instance;
	event = (ANDROID_EVENT*) android_event_cursor_new(flags, x, y);
	android_push_event(inst, event);

	DEBUG_ANDROID("send_cursor_event: (%d, %d), %d", x, y, flags);
}

JNIEXPORT void JNICALL jni_freerdp_send_clipboard_data(JNIEnv *env, jclass cls, jint instance, jstring jdata)
{
	ANDROID_EVENT* event;
	freerdp* inst = (freerdp*)instance;
	const jbyte *data = jdata != NULL ? (*env)->GetStringUTFChars(env, jdata, NULL) : NULL;
	int data_length = data ? strlen(data) : 0;      

	event = (ANDROID_EVENT*) android_event_clipboard_new((void*)data, data_length);
	android_push_event(inst, event);

	DEBUG_ANDROID("send_clipboard_data: (%s)", data);

	if (data)
		(*env)->ReleaseStringUTFChars(env, jdata, data);
}

JNIEXPORT jstring JNICALL jni_freerdp_get_version(JNIEnv *env, jclass cls)
{
	return (*env)->NewStringUTF(env, GIT_REVISION);
}

