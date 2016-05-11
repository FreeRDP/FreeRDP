/*
   Android JNI Client Layer

   Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz
   Copyright 2013 Thincast Technologies GmbH, Author: Armin Novak
   Copyright 2015 Bernhard Miklautz <bernhard.miklautz@thincast.com>
   Copyright 2016 Thincast Technologies GmbH
   Copyright 2016 Armin Novak <armin.novak@thincast.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <locale.h>

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

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
#include <freerdp/settings.h>
#include <freerdp/utils/signal.h>

#include <android/bitmap.h>

#include "android_jni_callback.h"
#include "android_jni_utils.h"
#include "android_cliprdr.h"
#include "android_freerdp_jni.h"

#if defined(WITH_GPROF)
#include "jni/prof.h"
#endif

#define TAG CLIENT_TAG("android")

/* Defines the JNI version supported by this library. */
#define FREERDP_JNI_VERSION "2.0.0"

static BOOL android_context_new(freerdp* instance, rdpContext* context)
{
	if (!instance || !context)
	{
		WLog_FATAL(TAG, "%s(instance=%p, context=%p) invalid arguments!",
			   __FUNCTION__, instance, context);
		return FALSE;
	}

	if (!(context->channels = freerdp_channels_new()))
		return FALSE;

	if (!android_event_queue_init(instance))
	{
		freerdp_channels_free(context->channels);
		return FALSE;
	}
	return TRUE;
}

static void android_context_free(freerdp* instance, rdpContext* context)
{
	if (context && context->channels)
	{
		freerdp_channels_close(context->channels, instance);
		freerdp_channels_free(context->channels);
		context->channels = NULL;
	}
	android_event_queue_uninit(instance);
}

static void android_OnChannelConnectedEventHandler(
		rdpContext* context,
		ChannelConnectedEventArgs* e)
{
	rdpSettings* settings;
	androidContext* afc;

	if (!context || !e)
	{
		WLog_FATAL(TAG, "%s(context=%p, EventArgs=%p",
			   __FUNCTION__, context, e);
		return;
	}
	afc = (androidContext*) context;
	settings = context->settings;

	if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
		{
			gdi_graphics_pipeline_init(context->gdi,
						   (RdpgfxClientContext*) e->pInterface);
		}
		else
		{
			WLog_WARN(TAG, "GFX without software GDI requested. "
					   " This is not supported, add /gdi:sw");
		}
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		android_cliprdr_init(afc, (CliprdrClientContext*) e->pInterface);
	}
	else
		WLog_WARN(TAG, "Trying to load unsupported channel %s", e->name);
}

static void android_OnChannelDisconnectedEventHandler(
		rdpContext* context, ChannelDisconnectedEventArgs* e)
{
	rdpSettings* settings;
	androidContext* afc;

	if (!context || !e)
	{
		WLog_FATAL(TAG, "%s(context=%p, EventArgs=%p",
			   __FUNCTION__, context, e);
		return;
	}
	afc = (androidContext*) context;
	settings = context->settings;

	if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
		{
			gdi_graphics_pipeline_uninit(context->gdi,
							 (RdpgfxClientContext*) e->pInterface);
		}
		else
		{
			WLog_WARN(TAG, "GFX without software GDI requested. "
					   " This is not supported, add /gdi:sw");
		}
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		android_cliprdr_uninit(afc, (CliprdrClientContext*) e->pInterface);
	}
	else
		WLog_WARN(TAG, "Trying to unload unsupported channel %s", e->name);
}

static BOOL android_begin_paint(rdpContext* context)
{
	rdpGdi* gdi;
	HGDI_WND hwnd;

	if (!context)
		return FALSE;

	gdi = context->gdi;
	if (!gdi || !gdi->primary || !gdi->primary->hdc)
		return FALSE;

	hwnd = gdi->primary->hdc->hwnd;
	if (!hwnd || !hwnd->invalid)
		return FALSE;

	hwnd->invalid->null = 1;
	hwnd->ninvalid = 0;
	return TRUE;
}

static BOOL android_end_paint(rdpContext* context)
{
	int i;
	HGDI_WND hwnd;
	int ninvalid;
	rdpGdi* gdi;
	HGDI_RGN cinvalid;
	int x1, y1, x2, y2;
	androidContext *ctx = (androidContext*)context;
	rdpSettings* settings;

	if (!ctx || !context->instance)
		return FALSE;

	settings = context->instance->settings;
	if (!settings)
		return FALSE;

	gdi = context->gdi;
	if (!gdi || !gdi->primary || !gdi->primary->hdc)
		return FALSE;

	hwnd = ctx->rdpCtx.gdi->primary->hdc->hwnd;
	if (!hwnd)
		return FALSE;

	ninvalid = hwnd->ninvalid;
	if (ninvalid == 0)
		return TRUE;

	cinvalid = hwnd->cinvalid;
	if (!cinvalid)
		return FALSE;

	x1 = cinvalid[0].x;
	y1 = cinvalid[0].y;
	x2 = cinvalid[0].x + cinvalid[0].w;
	y2 = cinvalid[0].y + cinvalid[0].h;

	for (i = 0; i < ninvalid; i++)
	{
		x1 = MIN(x1, cinvalid[i].x);
		y1 = MIN(y1, cinvalid[i].y);
		x2 = MAX(x2, cinvalid[i].x + cinvalid[i].w);
		y2 = MAX(y2, cinvalid[i].y + cinvalid[i].h);
	}

	freerdp_callback("OnGraphicsUpdate", "(IIIII)V", context->instance,
			 x1, y1, x2 - x1, y2 - y1);
	return TRUE;
}

static BOOL android_desktop_resize(rdpContext* context)
{
	if (!context || !context->instance || !context->settings)
		return FALSE;

	freerdp_callback("OnGraphicsResize", "(IIII)V",
			 context->instance, context->settings->DesktopWidth,
			 context->settings->DesktopHeight, context->settings->ColorDepth);
	return TRUE;
}

static BOOL android_pre_connect(freerdp* instance)
{
	int rc;
	rdpSettings* settings;
	BOOL bitmap_cache;

	if (!instance)
		return FALSE;

	settings = instance->settings;
	if (!settings || !settings->OrderSupport)
		return FALSE;

	bitmap_cache = settings->BitmapCacheEnabled;

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

	rc = PubSub_SubscribeChannelConnected(
			 instance->context->pubSub,
			 (pChannelConnectedEventHandler)
			 android_OnChannelConnectedEventHandler);
	if (rc != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "Could not subscribe to connect event handler [%l08X]", rc);
		return FALSE;
	}

	rc = PubSub_SubscribeChannelDisconnected(
			 instance->context->pubSub,
			 (pChannelDisconnectedEventHandler)
			 android_OnChannelDisconnectedEventHandler);
	if (rc != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "Could not subscribe to disconnect event handler [%l08X]", rc);
		return FALSE;
	}

	rc = freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);
	if (rc != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "Failed to register addin provider [%l08X]", rc);
		return FALSE;
	}

	if (!freerdp_client_load_addins(instance->context->channels, instance->settings))
	{
		WLog_ERR(TAG, "Failed to load addins [%l08X]", GetLastError());
		return FALSE;
	}

	rc = freerdp_channels_pre_connect(instance->context->channels, instance);
	if (rc != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "freerdp_channels_pre_connect failed with %l08X", rc);
		return FALSE;
	}

	freerdp_callback("OnPreConnect", "(I)V", instance);

	return TRUE;
}

static BOOL android_post_connect(freerdp* instance)
{
	UINT32 gdi_flags;
	rdpSettings *settings;

	if (!instance || !instance->settings || !instance->context || !instance->update)
		return FALSE;

	settings = instance->settings;

	if (!(instance->context->cache = cache_new(settings)))
		return FALSE;

	if (instance->settings->ColorDepth > 16)
		gdi_flags = CLRBUF_32BPP | CLRCONV_ALPHA | CLRCONV_INVERT;
	else
		gdi_flags = CLRBUF_16BPP;

	if (!gdi_init(instance, gdi_flags, NULL))
		return FALSE;

	instance->update->BeginPaint = android_begin_paint;
	instance->update->EndPaint = android_end_paint;
	instance->update->DesktopResize = android_desktop_resize;

	if (freerdp_channels_post_connect(instance->context->channels, instance) != CHANNEL_RC_OK)
		return FALSE;

	freerdp_callback("OnSettingsChanged", "(IIII)V", instance,
			 settings->DesktopWidth, settings->DesktopHeight,
			 settings->ColorDepth);
	freerdp_callback("OnConnectionSuccess", "(I)V", instance);

	return TRUE;
}

static void android_post_disconnect(freerdp* instance)
{
	freerdp_callback("OnDisconnecting", "(I)V", instance);

	if (instance && instance->context)
		freerdp_channels_disconnect(instance->context->channels, instance);

	gdi_free(instance);

	if (instance && instance->context)
		cache_free(instance->context->cache);
}

static BOOL android_authenticate(freerdp* instance, char** username,
				 char** password, char** domain)
{
	JNIEnv* env;
	jboolean attached = jni_attach_thread(&env);
	jobject jstr1 = create_string_builder(env, *username);
	jobject jstr2 = create_string_builder(env, *domain);
	jobject jstr3 = create_string_builder(env, *password);
	jboolean res;

	res = freerdp_callback_bool_result(
			  "OnAuthenticate",
			  "(ILjava/lang/StringBuilder;"
			  "Ljava/lang/StringBuilder;"
			  "Ljava/lang/StringBuilder;)Z",
			  instance, jstr1, jstr2, jstr3);

	if (res == JNI_TRUE)
	{
		// read back string values
		free(*username);
		*username = get_string_from_string_builder(env, jstr1);

		free(*domain);
		*domain = get_string_from_string_builder(env, jstr2);

		free(*password);
		*password = get_string_from_string_builder(env, jstr3);
	}

	if (attached == JNI_TRUE)
		jni_detach_thread();

	return ((res == JNI_TRUE) ? TRUE : FALSE);
}

static DWORD android_verify_certificate(
        freerdp* instance, const char* common_name,
        const char* subject, const char* issuer,
        const char* fingerprint, BOOL host_mismatch)
{
    WLog_DBG(TAG, "Certificate details:");
    WLog_DBG(TAG, "\tSubject: %s", subject);
    WLog_DBG(TAG, "\tIssuer: %s", issuer);
    WLog_DBG(TAG, "\tThumbprint: %s", fingerprint);
    WLog_DBG(TAG, "The above X.509 certificate could not be verified, possibly because you do not have "
            "the CA certificate in your certificate store, or the certificate has expired."
            "Please look at the documentation on how to create local certificate store for a private CA.\n");

    JNIEnv* env;
    jboolean attached = jni_attach_thread(&env);
    jstring jstr0 = (*env)->NewStringUTF(env, common_name);
    jstring jstr1 = (*env)->NewStringUTF(env, subject);
    jstring jstr2 = (*env)->NewStringUTF(env, issuer);
    jstring jstr3 = (*env)->NewStringUTF(env, fingerprint);

    jint res = freerdp_callback_int_result("OnVerifyCertificate",
            "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)I",
            instance, jstr0, jstr1, jstr2, jstr3, host_mismatch);

    if (attached == JNI_TRUE)
        jni_detach_thread();
    
    return res;
}

static DWORD android_verify_changed_certificate(freerdp* instance,
					const char* common_name,
					const char* subject,
					const char* issuer,
					const char* new_fingerprint,
					const char* old_subject,
					const char* old_issuer,
					const char* old_fingerprint)
{
    JNIEnv* env;
    jboolean attached = jni_attach_thread(&env);
    jstring jstr0 = (*env)->NewStringUTF(env, common_name);
    jstring jstr1 = (*env)->NewStringUTF(env, subject);
    jstring jstr2 = (*env)->NewStringUTF(env, issuer);
    jstring jstr3 = (*env)->NewStringUTF(env, new_fingerprint);
    jstring jstr4 = (*env)->NewStringUTF(env, old_subject);
    jstring jstr5 = (*env)->NewStringUTF(env, old_issuer);
    jstring jstr6 = (*env)->NewStringUTF(env, old_fingerprint);

    jint res = freerdp_callback_int_result("OnVerifyChangedCertificate",
            "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
            "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I",
            instance, jstr0, jstr1, jstr2, jstr3, jstr4, jstr5, jstr6);

    if (attached == JNI_TRUE)
        jni_detach_thread();
    
    return res;
}

static void* jni_input_thread(void* arg)
{
	HANDLE event[2];
	wMessageQueue* queue;
	freerdp* instance = (freerdp*) arg;

	WLog_DBG(TAG, "input_thread Start.");

	if (!(queue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE)))
		goto disconnect;

	if (!(event[0] = android_get_handle(instance)))
		goto disconnect;

	if (!(event[1] = freerdp_get_message_queue_event_handle(instance, FREERDP_INPUT_MESSAGE_QUEUE)))
		goto disconnect;

	do
	{
		DWORD rc = WaitForMultipleObjects(2, event, FALSE, INFINITE);
		if ((rc < WAIT_OBJECT_0) || (rc > WAIT_OBJECT_0 + 1))
			continue;

		if (rc == WAIT_OBJECT_0 + 1)
		{
			wMessage msg;

			MessageQueue_Peek(queue, &msg, FALSE);
			if (msg.id == WMQ_QUIT)
				break;
		}
		if (android_check_handle(instance) != TRUE)
			break;
	}
	while(1);

	WLog_DBG(TAG, "input_thread Quit.");

disconnect:
	MessageQueue_PostQuit(queue, 0);

	ExitThread(0);
	return NULL;
}

static int android_freerdp_run(freerdp* instance)
{
	DWORD count;
	DWORD status = WAIT_FAILED;
	HANDLE handles[64];
	HANDLE inputEvent = NULL;
	HANDLE inputThread = NULL;

	const rdpSettings* settings = instance->context->settings;
	rdpContext* context = instance->context;

	BOOL async_input = settings->AsyncInput;

	WLog_DBG(TAG, "AsyncInput=%d", settings->AsyncInput);

	if (async_input)
	{
		if (!(inputEvent = freerdp_get_message_queue_event_handle(instance, FREERDP_INPUT_MESSAGE_QUEUE)))
		{
			WLog_ERR(TAG, "async input: failed to get input event handle");
			goto disconnect;
		}
		if (!(inputThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) jni_input_thread, instance, 0, NULL)))
		{
			WLog_ERR(TAG, "async input: failed to create input thread");
			goto disconnect;
		}
	}
	else
		inputEvent = android_get_handle(instance);

	while (!freerdp_shall_disconnect(instance))
	{
		DWORD tmp;

		count = 0;
		handles[count++] = inputEvent;
		if (inputThread)
			handles[count++] = inputThread;

		tmp = freerdp_get_event_handles(context, &handles[count], 64 - count);

		if (tmp == 0)
		{
			WLog_ERR(TAG, "freerdp_get_event_handles failed");
			break;
		}

		count += tmp;

		status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
		if ((status == WAIT_FAILED))
		{
			WLog_ERR(TAG, "WaitForMultipleObjects failed with %lu [%08lX]",
				 status, GetLastError());
			break;
		}

		if (!freerdp_check_event_handles(context))
		{
			/* TODO: Auto reconnect
			if (xf_auto_reconnect(instance))
				continue;
				*/

			WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
			status = GetLastError();
			break;
		}

		if (freerdp_shall_disconnect(instance))
			break;

		if (!async_input)
		{
			if (android_check_handle(instance) != TRUE)
			{
				WLog_ERR(TAG, "Failed to check android file descriptor");
				status = GetLastError();
				break;
			}
		}
		else if (inputEvent)
		{
			if (WaitForSingleObject(inputEvent, 0) == WAIT_OBJECT_0)
			{
				if (!freerdp_message_queue_process_pending_messages(instance,
											FREERDP_INPUT_MESSAGE_QUEUE))
				{
					WLog_INFO(TAG, "User Disconnect");
					break;
				}
			}
		}
	}

disconnect:
	WLog_INFO(TAG, "Prepare shutdown...");

	if (async_input && inputThread)
	{
		wMessageQueue* input_queue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE);
		if (input_queue)
		{
			if (MessageQueue_PostQuit(input_queue, 0))
				WaitForSingleObject(inputThread, INFINITE);
		}
		CloseHandle(inputThread);
	}

	return status;
}

static void* android_thread_func(void* param)
{
	DWORD status;
	freerdp* instance = param;

	WLog_DBG(TAG, "Start...");

	if (!freerdp_connect(instance))
	{
		status = GetLastError();
		freerdp_callback("OnConnectionFailure", "(I)V", instance);
	}
	else
	{
		status = android_freerdp_run(instance);
		if (!freerdp_disconnect(instance))
			status = GetLastError();
		freerdp_callback("OnDisconnected", "(I)V", instance);
	}

	WLog_DBG(TAG, "Quit.");

	ExitThread(status);
	return NULL;
}

static jint JNICALL jni_freerdp_new(JNIEnv *env, jclass cls, jobject context)
{
	jclass contextClass;
	jclass fileClass;
	jobject filesDirObj;
	jmethodID getFilesDirID;
	jmethodID getAbsolutePathID;
	jstring path;
	freerdp* instance;
	const char* raw;
	char* envStr;

#if defined(WITH_GPROF)
	setenv("CPUPROFILE_FREQUENCY", "200", 1);
	monstartup("libfreerdp-android.so");
#endif

	contextClass = (*env)->FindClass(env, JAVA_CONTEXT_CLASS);
	fileClass = (*env)->FindClass(env, JAVA_FILE_CLASS);
	if (!contextClass || !fileClass)
	{
		WLog_FATAL(TAG, "Failed to load class references %s=%p, %s=%p",
			JAVA_CONTEXT_CLASS, contextClass, JAVA_FILE_CLASS, fileClass);
		return (jint)NULL;
	}

	getFilesDirID = (*env)->GetMethodID(env, contextClass, "getFilesDir", "()L"JAVA_FILE_CLASS";");
	if (!getFilesDirID)
	{
		WLog_FATAL(TAG, "Failed to find method ID getFilesDir ()L"JAVA_FILE_CLASS";");
		return (jint)NULL;
	}

	getAbsolutePathID = (*env)->GetMethodID(env, fileClass, "getAbsolutePath", "()Ljava/lang/String;");
	if (!getAbsolutePathID)
	{
		WLog_FATAL(TAG, "Failed to find method ID getAbsolutePath ()Ljava/lang/String;");
		return (jint)NULL;
	}

	filesDirObj = (*env)->CallObjectMethod(env, context, getFilesDirID);
	if (!filesDirObj)
	{
		WLog_FATAL(TAG, "Failed to call getFilesDir");
		return (jint)NULL;
	}

	path = (*env)->CallObjectMethod(env, filesDirObj, getAbsolutePathID);
	if (!path)
	{
		WLog_FATAL(TAG, "Failed to call getAbsolutePath");
		return (jint)NULL;
	}

	raw = (*env)->GetStringUTFChars(env, path, 0);
	if (!raw)
	{
		WLog_FATAL(TAG, "Failed to get C string from java string");
		return (jint)NULL;
	}

	envStr = _strdup(raw);
	(*env)->ReleaseStringUTFChars(env, path, raw);

	if (!envStr)
	{
		WLog_FATAL(TAG, "_strdup(%s) failed", raw);
		return (jint)NULL;
	}

	if (setenv("HOME", _strdup(envStr), 1) != 0)
	{
		WLog_FATAL(TAG, "Failed to set environemnt HOME=%s %s [%d]",
			env, strerror(errno), errno);
		return (jint)NULL;
	}

	// create instance
	if (!(instance = freerdp_new()))
		return (jint)NULL;

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

	if (!freerdp_context_new(instance))
	{
		freerdp_free(instance);
		instance = NULL;
	}

	return (jint) instance;
}

static void JNICALL jni_freerdp_free(JNIEnv *env, jclass cls, jint instance)
{
	freerdp* inst = (freerdp*)instance;

	freerdp_context_free(inst);
	freerdp_free(inst);

#if defined(WITH_GPROF)
	moncleanup();
#endif
}

static jboolean JNICALL jni_freerdp_parse_arguments(
		JNIEnv *env, jclass cls, jint instance, jobjectArray arguments)
{
	freerdp* inst = (freerdp*)instance;
	int i, count;
	char **argv;
	DWORD status;

	if (!inst || !inst->context)
		return JNI_FALSE;

	count = (*env)->GetArrayLength(env, arguments);
	argv = calloc(count, sizeof(char*));
	if (!argv)
		return JNI_TRUE;

	for (i=0; i<count; i++)
	{
		jstring str = (jstring)(*env)->GetObjectArrayElement(env, arguments, i);
		const char *raw = (*env)->GetStringUTFChars(env, str, 0);
		argv[i] = _strdup(raw);
		(*env)->ReleaseStringUTFChars(env, str, raw);
	}

	status = freerdp_client_settings_parse_command_line(inst->settings, count, argv, FALSE);

	for (i=0; i<count; i++)
		free(argv[i]);
	free(argv);

	return (status == 0) ? JNI_TRUE : JNI_FALSE;
}

static jboolean JNICALL jni_freerdp_connect(JNIEnv *env, jclass cls, jint instance)
{
	freerdp* inst = (freerdp*)instance;
	androidContext* ctx;

	if (!inst || !inst->context)
	{
		WLog_FATAL(TAG, "%s(env=%p, cls=%p, instance=%d", __FUNCTION__,
			   env, cls, instance);
		return JNI_FALSE;
	}

	ctx = (androidContext*)inst->context;
	if (!(ctx->thread = CreateThread(
			  NULL, 0, (LPTHREAD_START_ROUTINE)android_thread_func,
			  inst, 0, NULL)))
	{
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

static jboolean JNICALL jni_freerdp_disconnect(JNIEnv *env, jclass cls, jint instance)
{
	freerdp* inst = (freerdp*)instance;
	androidContext* ctx;
	ANDROID_EVENT* event;

	if (!inst || !inst->context || !cls || !env)
	{
		WLog_FATAL(TAG, "%s(env=%p, cls=%p, instance=%d", __FUNCTION__,
			   env, cls, instance);
		return JNI_FALSE;
	}

	ctx = (androidContext*)inst->context;
	event = (ANDROID_EVENT*)android_event_disconnect_new();
	if (!event)
		return JNI_FALSE;

	if (!android_push_event(inst, event))
	{
		android_event_free((ANDROID_EVENT*)event);
		return JNI_FALSE;
	}

	if (!freerdp_abort_connect(inst))
		return JNI_FALSE;

	return JNI_TRUE;
}

static void copy_pixel_buffer(UINT8* dstBuf, UINT8* srcBuf, int x, int y,
				  int width, int height, int wBuf, int hBuf, int bpp)
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

static jboolean JNICALL jni_freerdp_update_graphics(
		JNIEnv *env, jclass cls, jint instance, jobject bitmap,
		jint x, jint y, jint width, jint height)
{

	int ret;
	void* pixels;
	AndroidBitmapInfo info;
	freerdp* inst = (freerdp*)instance;
	rdpGdi *gdi;

	if (!env || !cls || !inst)
	{
		WLog_FATAL(TAG, "%s(env=%p, cls=%p, instance=%d", __FUNCTION__,
			   env, cls, instance);
		return JNI_FALSE;
	}

	gdi = inst->context->gdi;

	if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0)
	{
		WLog_FATAL(TAG, "AndroidBitmap_getInfo() failed ! error=%d", ret);
		return JNI_FALSE;
	}

	if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0)
	{
		WLog_FATAL(TAG, "AndroidBitmap_lockPixels() failed ! error=%d", ret);
		return JNI_FALSE;
	}

	copy_pixel_buffer(pixels, gdi->primary_buffer, x, y, width, height,
			  gdi->width, gdi->height, gdi->bytesPerPixel);

	if ((ret = AndroidBitmap_unlockPixels(env, bitmap)) < 0)
	{
		WLog_FATAL(TAG, "AndroidBitmap_unlockPixels() failed ! error=%d", ret);
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

static jboolean JNICALL jni_freerdp_send_key_event(
		JNIEnv *env, jclass cls, jint instance,
		jint keycode, jboolean down)
{
	DWORD scancode;
	ANDROID_EVENT* event;

	freerdp* inst = (freerdp*)instance;

	scancode = GetVirtualScanCodeFromVirtualKeyCode(keycode, 4);
	int flags = (down == JNI_TRUE) ? KBD_FLAGS_DOWN : KBD_FLAGS_RELEASE;
	flags |= (scancode & KBDEXT) ? KBD_FLAGS_EXTENDED : 0;
	event = (ANDROID_EVENT*) android_event_key_new(flags, scancode & 0xFF);
	if (!event)
		return JNI_FALSE;

	if (!android_push_event(inst, event))
	{
		android_event_free(event);
		return JNI_FALSE;
	}

	WLog_DBG(TAG, "send_key_event: %d, %d", (int)scancode, flags);
	return JNI_TRUE;
}

static jboolean JNICALL jni_freerdp_send_unicodekey_event(
		JNIEnv *env, jclass cls, jint instance, jint keycode)
{
	ANDROID_EVENT* event;

	freerdp* inst = (freerdp*)instance;
	event = (ANDROID_EVENT*) android_event_unicodekey_new(keycode);
	if (!event)
		return JNI_FALSE;
	if (!android_push_event(inst, event))
	{
		android_event_free(event);
		return JNI_FALSE;
	}

	WLog_DBG(TAG, "send_unicodekey_event: %d", keycode);
	return JNI_TRUE;
}

static jboolean JNICALL jni_freerdp_send_cursor_event(
		JNIEnv *env, jclass cls, jint instance, jint x, jint y, jint flags)
{
	ANDROID_EVENT* event;

	freerdp* inst = (freerdp*)instance;
	event = (ANDROID_EVENT*) android_event_cursor_new(flags, x, y);
	if (!event)
		return JNI_FALSE;

	if (!android_push_event(inst, event))
	{
		android_event_free(event);
		return JNI_FALSE;
	}

	WLog_DBG(TAG, "send_cursor_event: (%d, %d), %d", x, y, flags);
	return JNI_TRUE;
}

static jboolean JNICALL jni_freerdp_send_clipboard_data(
		JNIEnv *env, jclass cls,
		jint instance, jstring jdata)
{
	ANDROID_EVENT* event;
	freerdp* inst = (freerdp*)instance;
	const jbyte *data = jdata != NULL ? (*env)->GetStringUTFChars(env, jdata, NULL) : NULL;
	int data_length = data ? strlen(data) : 0;
	jboolean ret = JNI_FALSE;;

	event = (ANDROID_EVENT*) android_event_clipboard_new((void*)data, data_length);
	if (!event)
		goto out_fail;

	if (!android_push_event(inst, event))
	{
		android_event_free(event);
		goto out_fail;
	}

	WLog_DBG(TAG, "send_clipboard_data: (%s)", data);

	ret = JNI_TRUE;
out_fail:
	if (data)
		(*env)->ReleaseStringUTFChars(env, jdata, data);
	return ret;
}

static jstring JNICALL jni_freerdp_get_jni_version(JNIEnv *env, jclass cls)
{
	return (*env)->NewStringUTF(env, FREERDP_JNI_VERSION);
}

static jstring JNICALL jni_freerdp_get_version(JNIEnv *env, jclass cls)
{
	return (*env)->NewStringUTF(env, freerdp_get_version_string());
}

static jstring JNICALL jni_freerdp_get_build_date(JNIEnv *env, jclass cls)
{
	return (*env)->NewStringUTF(env, freerdp_get_build_date());
}

static jstring JNICALL jni_freerdp_get_build_revision(JNIEnv *env, jclass cls)
{
	return (*env)->NewStringUTF(env, freerdp_get_build_revision());
}

static jstring JNICALL jni_freerdp_get_build_config(JNIEnv *env, jclass cls)
{
	return (*env)->NewStringUTF(env, freerdp_get_build_config());
}

static JNINativeMethod methods[] = {
	{
		"freerdp_get_jni_version",
		"()Ljava/lang/String;",
		&jni_freerdp_get_jni_version
	},
	{
		"freerdp_get_version",
		"()Ljava/lang/String;",
		&jni_freerdp_get_version
	},
	{
		"freerdp_get_build_date",
		"()Ljava/lang/String;",
		&jni_freerdp_get_build_date
	},
	{
		"freerdp_get_build_revision",
		"()Ljava/lang/String;",
		&jni_freerdp_get_build_revision
	},
	{
		"freerdp_get_build_config",
		"()Ljava/lang/String;",
		&jni_freerdp_get_build_config
	},
	{
		"freerdp_new",
		"(Landroid/content/Context;)I",
		&jni_freerdp_new
	},
	{
		"freerdp_free",
		"(I)V",
		&jni_freerdp_free
	},
	{
		"freerdp_parse_arguments",
		"(I[Ljava/lang/String;)Z",
		&jni_freerdp_parse_arguments
	},
	{
		"freerdp_connect",
		"(I)Z",
		&jni_freerdp_connect
	},
	{
		"freerdp_disconnect",
		"(I)Z",
		&jni_freerdp_disconnect
	},
	{
		"freerdp_update_graphics",
		"(ILandroid/graphics/Bitmap;IIII)Z",
		&jni_freerdp_update_graphics
	},
	{
		"freerdp_send_cursor_event",
		"(IIII)Z",
		&jni_freerdp_send_cursor_event
	},
	{
		"freerdp_send_key_event",
		"(IIZ)Z",
		&jni_freerdp_send_key_event
	},
	{
		"freerdp_send_unicodekey_event",
		"(II)Z",
		&jni_freerdp_send_unicodekey_event
	},
	{
		"freerdp_send_clipboard_data",
		"(ILjava/lang/String;)Z",
		&jni_freerdp_send_clipboard_data
	}
};

static jclass gJavaActivityClass = NULL;

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
	JNIEnv* env;

	setlocale(LC_ALL, "");

	freerdp_handle_signals();

	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK)
	{
		WLog_FATAL(TAG, "Failed to get the environment");
		return -1;
	}

	// Get SBCEngine activity class
	jclass activityClass = (*env)->FindClass(env, JAVA_LIBFREERDP_CLASS);
	if (!activityClass)
	{
		WLog_FATAL(TAG, "failed to get %s class reference", JAVA_LIBFREERDP_CLASS);
		return -1;
	}

	// Register methods with env->RegisterNatives.
	(*env)->RegisterNatives(env, activityClass, methods,
				sizeof(methods) / sizeof(methods[0]));

	/* create global reference for class */
	gJavaActivityClass = (*env)->NewGlobalRef(env, activityClass);

	g_JavaVm = vm;
	return init_callback_environment(vm, env);
}

void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved)
{
	JNIEnv* env;
	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK)
	{
		WLog_FATAL(TAG, "Failed to get the environment");
		return;
	}

	(*env)->UnregisterNatives(env, gJavaActivityClass);

	if (gJavaActivityClass)
		(*env)->DeleteGlobalRef(env, gJavaActivityClass);
}
