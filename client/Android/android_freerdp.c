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

static void android_OnChannelConnectedEventHandler(
    rdpContext* context,
    ChannelConnectedEventArgs* e)
{
	rdpSettings* settings;
	androidContext* afc;

	if (!context || !e)
	{
		WLog_FATAL(TAG, "%s(context=%p, EventArgs=%p",
		           __FUNCTION__, (void*) context, (void*) e);
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
}

static void android_OnChannelDisconnectedEventHandler(
    rdpContext* context, ChannelDisconnectedEventArgs* e)
{
	rdpSettings* settings;
	androidContext* afc;

	if (!context || !e)
	{
		WLog_FATAL(TAG, "%s(context=%p, EventArgs=%p",
		           __FUNCTION__, (void*) context, (void*) e);
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

	hwnd->invalid->null = TRUE;
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
	androidContext* ctx = (androidContext*)context;
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

	freerdp_callback("OnGraphicsUpdate", "(JIIII)V", (jlong)context->instance,
	                 x1, y1, x2 - x1, y2 - y1);
	return TRUE;
}

static BOOL android_desktop_resize(rdpContext* context)
{
	if (!context || !context->instance || !context->settings)
		return FALSE;

	freerdp_callback("OnGraphicsResize", "(JIII)V",
	                 (jlong)context->instance, context->settings->DesktopWidth,
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

	if (!freerdp_client_load_addins(instance->context->channels,
	                                instance->settings))
	{
		WLog_ERR(TAG, "Failed to load addins [%l08X]", GetLastError());
		return FALSE;
	}

	freerdp_callback("OnPreConnect", "(J)V", (jlong)instance);
	return TRUE;
}

static BOOL android_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	if (!context || !pointer || !context->gdi)
		return FALSE;

	return TRUE;
}

static void android_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	if (!context || !pointer)
		return;
}

static BOOL android_Pointer_Set(rdpContext* context,
                                const rdpPointer* pointer)
{
	if (!context)
		return FALSE;

	return TRUE;
}

static BOOL android_Pointer_SetPosition(rdpContext* context,
                                        UINT32 x, UINT32 y)
{
	if (!context)
		return FALSE;

	return TRUE;
}

static BOOL android_Pointer_SetNull(rdpContext* context)
{
	if (!context)
		return FALSE;

	return TRUE;
}

static BOOL android_Pointer_SetDefault(rdpContext* context)
{
	if (!context)
		return FALSE;

	return TRUE;
}

static BOOL android_register_pointer(rdpGraphics* graphics)
{
	rdpPointer pointer;

	if (!graphics)
		return FALSE;

	pointer.size = sizeof(pointer);
	pointer.New = android_Pointer_New;
	pointer.Free = android_Pointer_Free;
	pointer.Set = android_Pointer_Set;
	pointer.SetNull = android_Pointer_SetNull;
	pointer.SetDefault = android_Pointer_SetDefault;
	pointer.SetPosition = android_Pointer_SetPosition;
	graphics_register_pointer(graphics, &pointer);
	return TRUE;
}

static BOOL android_post_connect(freerdp* instance)
{
	rdpSettings* settings;
	rdpUpdate* update;

	if (!instance || !instance->settings || !instance->context || !instance->update)
		return FALSE;

	update = instance->update;
	settings = instance->settings;

	if (!gdi_init(instance, PIXEL_FORMAT_RGBA32))
		return FALSE;

	if (!android_register_pointer(instance->context->graphics))
		return FALSE;

	instance->update->BeginPaint = android_begin_paint;
	instance->update->EndPaint = android_end_paint;
	instance->update->DesktopResize = android_desktop_resize;
	pointer_cache_register_callbacks(update);
	freerdp_callback("OnSettingsChanged", "(JIII)V", (jlong)instance,
	                 settings->DesktopWidth, settings->DesktopHeight,
	                 settings->ColorDepth);
	freerdp_callback("OnConnectionSuccess", "(J)V", (jlong)instance);
	return TRUE;
}

static void android_post_disconnect(freerdp* instance)
{
	freerdp_callback("OnDisconnecting", "(J)V", (jlong)instance);
	gdi_free(instance);
}

static BOOL android_authenticate_int(freerdp* instance, char** username,
                                     char** password, char** domain, const char* cb_name)
{
	JNIEnv* env;
	jboolean attached = jni_attach_thread(&env);
	jobject jstr1 = create_string_builder(env, *username);
	jobject jstr2 = create_string_builder(env, *domain);
	jobject jstr3 = create_string_builder(env, *password);
	jboolean res;
	res = freerdp_callback_bool_result(
	          cb_name,
	          "(JLjava/lang/StringBuilder;"
	          "Ljava/lang/StringBuilder;"
	          "Ljava/lang/StringBuilder;)Z",
	          (jlong)instance, jstr1, jstr2, jstr3);

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

static BOOL android_authenticate(freerdp* instance, char** username,
                                 char** password, char** domain)
{
	return android_authenticate_int(instance, username, password, domain,
	                                "OnAuthenticate");
}

static BOOL android_gw_authenticate(freerdp* instance, char** username,
                                    char** password, char** domain)
{
	return android_authenticate_int(instance, username, password, domain,
	                                "OnGatewayAuthenticate");
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
	WLog_DBG(TAG,
	         "The above X.509 certificate could not be verified, possibly because you do not have "
	         "the CA certificate in your certificate store, or the certificate has expired."
	         "Please look at the OpenSSL documentation on how to add a private CA to the store.\n");
	JNIEnv* env;
	jboolean attached = jni_attach_thread(&env);
	jstring jstr0 = (*env)->NewStringUTF(env, common_name);
	jstring jstr1 = (*env)->NewStringUTF(env, subject);
	jstring jstr2 = (*env)->NewStringUTF(env, issuer);
	jstring jstr3 = (*env)->NewStringUTF(env, fingerprint);
	jint res = freerdp_callback_int_result("OnVerifyCertificate",
	                                       "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)I",
	                                       (jlong)instance, jstr0, jstr1, jstr2, jstr3, host_mismatch);

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
	                                       "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
	                                       "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I",
	                                       (jlong)instance, jstr0, jstr1, jstr2, jstr3, jstr4, jstr5, jstr6);

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

	if (!(event[1] = freerdp_get_message_queue_event_handle(instance,
	                 FREERDP_INPUT_MESSAGE_QUEUE)))
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
	while (1);

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
	WLog_DBG(TAG, "AsyncInput=%"PRIu8"", settings->AsyncInput);

	if (async_input)
	{
		if (!(inputThread = CreateThread(NULL, 0,
		                                 (LPTHREAD_START_ROUTINE) jni_input_thread, instance, 0, NULL)))
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

		if (inputThread)
			handles[count++] = inputThread;
		else
			handles[count++] = inputEvent;

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
			WLog_ERR(TAG, "WaitForMultipleObjects failed with %"PRIu32" [%08lX]", status, GetLastError());
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
	}

disconnect:
	WLog_INFO(TAG, "Prepare shutdown...");

	if (async_input && inputThread)
	{
		WaitForSingleObject(inputThread, INFINITE);
		CloseHandle(inputThread);
	}

	return status;
}

static void* android_thread_func(void* param)
{
	DWORD status = ERROR_BAD_ARGUMENTS;
	freerdp* instance = param;
	WLog_DBG(TAG, "Start...");

	if (!instance || !instance->context)
		goto fail;

	if (freerdp_client_start(instance->context) != CHANNEL_RC_OK)
		goto fail;

	WLog_DBG(TAG, "Connect...");

	if (!freerdp_connect(instance))
		status = GetLastError();
	else
	{
		status = android_freerdp_run(instance);
		WLog_DBG(TAG, "Disonnect...");

		if (!freerdp_disconnect(instance))
			status = GetLastError();
	}

	WLog_DBG(TAG, "Stop...");

	if (freerdp_client_stop(instance->context) != CHANNEL_RC_OK)
		goto fail;

fail:
	WLog_DBG(TAG, "Session ended with %08"PRIX32"", status);

	if (status == CHANNEL_RC_OK)
		freerdp_callback("OnDisconnected", "(J)V", (jlong)instance);
	else
		freerdp_callback("OnConnectionFailure", "(J)V", (jlong)instance);

	WLog_DBG(TAG, "Quit.");
	ExitThread(status);
	return NULL;
}

static BOOL android_client_new(freerdp* instance, rdpContext* context)
{
	if (!instance || !context)
		return FALSE;

	if (!android_event_queue_init(instance))
		return FALSE;

	instance->PreConnect = android_pre_connect;
	instance->PostConnect = android_post_connect;
	instance->PostDisconnect = android_post_disconnect;
	instance->Authenticate = android_authenticate;
	instance->GatewayAuthenticate = android_gw_authenticate;
	instance->VerifyCertificate = android_verify_certificate;
	instance->VerifyChangedCertificate = android_verify_changed_certificate;
	instance->LogonErrorInfo = NULL;
	return TRUE;
}


static void android_client_free(freerdp* instance, rdpContext* context)
{
	if (!context)
		return;

	android_event_queue_uninit(instance);
}

static int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	ZeroMemory(pEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	pEntryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->GlobalInit = NULL;
	pEntryPoints->GlobalUninit = NULL;
	pEntryPoints->ContextSize = sizeof(androidContext);
	pEntryPoints->ClientNew = android_client_new;
	pEntryPoints->ClientFree = android_client_free;
	pEntryPoints->ClientStart = NULL;
	pEntryPoints->ClientStop = NULL;
	return 0;
}

static jlong JNICALL jni_freerdp_new(JNIEnv* env, jclass cls, jobject context)
{
	jclass contextClass;
	jclass fileClass;
	jobject filesDirObj;
	jmethodID getFilesDirID;
	jmethodID getAbsolutePathID;
	jstring path;
	const char* raw;
	char* envStr;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	rdpContext* ctx;
#if defined(WITH_GPROF)
	setenv("CPUPROFILE_FREQUENCY", "200", 1);
	monstartup("libfreerdp-android.so");
#endif
	contextClass = (*env)->FindClass(env, JAVA_CONTEXT_CLASS);
	fileClass = (*env)->FindClass(env, JAVA_FILE_CLASS);

	if (!contextClass || !fileClass)
	{
		WLog_FATAL(TAG, "Failed to load class references %s=%p, %s=%p",
		           JAVA_CONTEXT_CLASS, (void*) contextClass, JAVA_FILE_CLASS, (void*) fileClass);
		return (jlong)NULL;
	}

	getFilesDirID = (*env)->GetMethodID(env, contextClass, "getFilesDir",
	                                    "()L"JAVA_FILE_CLASS";");

	if (!getFilesDirID)
	{
		WLog_FATAL(TAG, "Failed to find method ID getFilesDir ()L"JAVA_FILE_CLASS";");
		return (jlong)NULL;
	}

	getAbsolutePathID = (*env)->GetMethodID(env, fileClass, "getAbsolutePath",
	                                        "()Ljava/lang/String;");

	if (!getAbsolutePathID)
	{
		WLog_FATAL(TAG,
		           "Failed to find method ID getAbsolutePath ()Ljava/lang/String;");
		return (jlong)NULL;
	}

	filesDirObj = (*env)->CallObjectMethod(env, context, getFilesDirID);

	if (!filesDirObj)
	{
		WLog_FATAL(TAG, "Failed to call getFilesDir");
		return (jlong)NULL;
	}

	path = (*env)->CallObjectMethod(env, filesDirObj, getAbsolutePathID);

	if (!path)
	{
		WLog_FATAL(TAG, "Failed to call getAbsolutePath");
		return (jlong)NULL;
	}

	raw = (*env)->GetStringUTFChars(env, path, 0);

	if (!raw)
	{
		WLog_FATAL(TAG, "Failed to get C string from java string");
		return (jlong)NULL;
	}

	envStr = _strdup(raw);
	(*env)->ReleaseStringUTFChars(env, path, raw);

	if (!envStr)
	{
		WLog_FATAL(TAG, "_strdup(%s) failed", raw);
		return (jlong)NULL;
	}

	if (setenv("HOME", _strdup(envStr), 1) != 0)
	{
		WLog_FATAL(TAG, "Failed to set environemnt HOME=%s %s [%d]",
		           env, strerror(errno), errno);
		return (jlong)NULL;
	}

	RdpClientEntry(&clientEntryPoints);
	ctx = freerdp_client_context_new(&clientEntryPoints);

	if (!ctx)
		return (jlong)NULL;

	return (jlong) ctx->instance;
}

static void JNICALL jni_freerdp_free(JNIEnv* env, jclass cls, jlong instance)
{
	freerdp* inst = (freerdp*)instance;

	if (inst)
		freerdp_client_context_free(inst->context);

#if defined(WITH_GPROF)
	moncleanup();
#endif
}

static jboolean JNICALL jni_freerdp_parse_arguments(
    JNIEnv* env, jclass cls, jlong instance, jobjectArray arguments)
{
	freerdp* inst = (freerdp*)instance;
	int i, count;
	char** argv;
	DWORD status;

	if (!inst || !inst->context)
		return JNI_FALSE;

	count = (*env)->GetArrayLength(env, arguments);
	argv = calloc(count, sizeof(char*));

	if (!argv)
		return JNI_TRUE;

	for (i = 0; i < count; i++)
	{
		jstring str = (jstring)(*env)->GetObjectArrayElement(env, arguments, i);
		const char* raw = (*env)->GetStringUTFChars(env, str, 0);
		argv[i] = _strdup(raw);
		(*env)->ReleaseStringUTFChars(env, str, raw);
	}

	status = freerdp_client_settings_parse_command_line(inst->settings, count, argv,
	         FALSE);

	for (i = 0; i < count; i++)
		free(argv[i]);

	free(argv);
	return (status == 0) ? JNI_TRUE : JNI_FALSE;
}

static jboolean JNICALL jni_freerdp_connect(JNIEnv* env, jclass cls,
        jlong instance)
{
	freerdp* inst = (freerdp*)instance;
	androidContext* ctx;

	if (!inst || !inst->context)
	{
		WLog_FATAL(TAG, "%s(env=%p, cls=%p, instance=%d", __FUNCTION__,
		           (void*) env, (void*) cls, instance);
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

static jboolean JNICALL jni_freerdp_disconnect(JNIEnv* env, jclass cls,
        jlong instance)
{
	freerdp* inst = (freerdp*)instance;
	androidContext* ctx;
	ANDROID_EVENT* event;

	if (!inst || !inst->context || !cls || !env)
	{
		WLog_FATAL(TAG, "%s(env=%p, cls=%p, instance=%d", __FUNCTION__,
		           (void*) env, (void*) cls, instance);
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

static jboolean JNICALL jni_freerdp_update_graphics(
    JNIEnv* env, jclass cls, jlong instance, jobject bitmap,
    jint x, jint y, jint width, jint height)
{
	UINT32 DstFormat;
	jboolean rc;
	int ret;
	void* pixels;
	AndroidBitmapInfo info;
	freerdp* inst = (freerdp*)instance;
	rdpGdi* gdi;

	if (!env || !cls || !inst)
	{
		WLog_FATAL(TAG, "%s(env=%p, cls=%p, instance=%d", __FUNCTION__,
		           (void*) env, (void*) cls, instance);
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

	rc = JNI_TRUE;

	switch (info.format)
	{
		case ANDROID_BITMAP_FORMAT_RGBA_8888:
			DstFormat = PIXEL_FORMAT_RGBA32;
			break;

		case ANDROID_BITMAP_FORMAT_RGB_565:
			DstFormat = PIXEL_FORMAT_RGB16;
			break;

		case ANDROID_BITMAP_FORMAT_RGBA_4444:
		case ANDROID_BITMAP_FORMAT_A_8:
		case ANDROID_BITMAP_FORMAT_NONE:
		default:
			rc = JNI_FALSE;
			break;
	}

	if (rc)
	{
		rc = freerdp_image_copy(pixels, DstFormat, info.stride, x, y, width, height,
		                        gdi->primary_buffer, gdi->dstFormat, gdi->stride, x, y,
		                        &gdi->palette, FREERDP_FLIP_NONE);
	}

	if ((ret = AndroidBitmap_unlockPixels(env, bitmap)) < 0)
	{
		WLog_FATAL(TAG, "AndroidBitmap_unlockPixels() failed ! error=%d", ret);
		return JNI_FALSE;
	}

	return rc;
}

static jboolean JNICALL jni_freerdp_send_key_event(
    JNIEnv* env, jclass cls, jlong instance,
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

	WLog_DBG(TAG, "send_key_event: %"PRIu32", %d", scancode, flags);
	return JNI_TRUE;
}

static jboolean JNICALL jni_freerdp_send_unicodekey_event(
    JNIEnv* env, jclass cls, jlong instance, jint keycode)
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
    JNIEnv* env, jclass cls, jlong instance, jint x, jint y, jint flags)
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
    JNIEnv* env, jclass cls,
    jlong instance, jstring jdata)
{
	ANDROID_EVENT* event;
	freerdp* inst = (freerdp*)instance;
	const jbyte* data = jdata != NULL ? (*env)->GetStringUTFChars(env, jdata,
	                    NULL) : NULL;
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

static jstring JNICALL jni_freerdp_get_jni_version(JNIEnv* env, jclass cls)
{
	return (*env)->NewStringUTF(env, FREERDP_JNI_VERSION);
}

static jstring JNICALL jni_freerdp_get_version(JNIEnv* env, jclass cls)
{
	return (*env)->NewStringUTF(env, freerdp_get_version_string());
}

static jstring JNICALL jni_freerdp_get_build_date(JNIEnv* env, jclass cls)
{
	return (*env)->NewStringUTF(env, freerdp_get_build_date());
}

static jstring JNICALL jni_freerdp_get_build_revision(JNIEnv* env, jclass cls)
{
	return (*env)->NewStringUTF(env, freerdp_get_build_revision());
}

static jstring JNICALL jni_freerdp_get_build_config(JNIEnv* env, jclass cls)
{
	return (*env)->NewStringUTF(env, freerdp_get_build_config());
}

static JNINativeMethod methods[] =
{
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
		"(Landroid/content/Context;)J",
		&jni_freerdp_new
	},
	{
		"freerdp_free",
		"(J)V",
		&jni_freerdp_free
	},
	{
		"freerdp_parse_arguments",
		"(J[Ljava/lang/String;)Z",
		&jni_freerdp_parse_arguments
	},
	{
		"freerdp_connect",
		"(J)Z",
		&jni_freerdp_connect
	},
	{
		"freerdp_disconnect",
		"(J)Z",
		&jni_freerdp_disconnect
	},
	{
		"freerdp_update_graphics",
		"(JLandroid/graphics/Bitmap;IIII)Z",
		&jni_freerdp_update_graphics
	},
	{
		"freerdp_send_cursor_event",
		"(JIII)Z",
		&jni_freerdp_send_cursor_event
	},
	{
		"freerdp_send_key_event",
		"(JIZ)Z",
		&jni_freerdp_send_key_event
	},
	{
		"freerdp_send_unicodekey_event",
		"(JI)Z",
		&jni_freerdp_send_unicodekey_event
	},
	{
		"freerdp_send_clipboard_data",
		"(JLjava/lang/String;)Z",
		&jni_freerdp_send_clipboard_data
	}
};

static jclass gJavaActivityClass = NULL;

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv* env;
	setlocale(LC_ALL, "");
	WLog_DBG(TAG, "Setting up JNI environement...");

	/*
		if (freerdp_handle_signals() != 0)
		{
			WLog_FATAL(TAG, "Failed to register signal handler");
			return -1;
		}
	*/
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
	WLog_DBG(TAG, "Tearing down JNI environement...");

	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK)
	{
		WLog_FATAL(TAG, "Failed to get the environment");
		return;
	}

	(*env)->UnregisterNatives(env, gJavaActivityClass);

	if (gJavaActivityClass)
		(*env)->DeleteGlobalRef(env, gJavaActivityClass);
}
