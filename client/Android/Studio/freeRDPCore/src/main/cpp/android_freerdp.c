/*
   Android JNI Client Layer

   Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz
   Copyright 2013 Thincast Technologies GmbH, Author: Armin Novak
   Copyright 2015 Bernhard Miklautz <bernhard.miklautz@thincast.com>
   Copyright 2016 Thincast Technologies GmbH
   Copyright 2016 Armin Novak <armin.novak@thincast.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

#include <freerdp/config.h>

#include <locale.h>

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <winpr/assert.h>

#include <freerdp/graphics.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/codec/h264.h>
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
#define FREERDP_JNI_VERSION "3.0.0-dev"

static void android_OnChannelConnectedEventHandler(void* context,
                                                   const ChannelConnectedEventArgs* e)
{
	rdpSettings* settings;
	androidContext* afc;

	if (!context || !e)
	{
		WLog_FATAL(TAG, "%s(context=%p, EventArgs=%p", __FUNCTION__, context, (void*)e);
		return;
	}

	afc = (androidContext*)context;
	settings = afc->common.context.settings;

	if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		android_cliprdr_init(afc, (CliprdrClientContext*)e->pInterface);
	}
	else
		freerdp_client_OnChannelConnectedEventHandler(context, e);
}

static void android_OnChannelDisconnectedEventHandler(void* context,
                                                      const ChannelDisconnectedEventArgs* e)
{
	rdpSettings* settings;
	androidContext* afc;

	if (!context || !e)
	{
		WLog_FATAL(TAG, "%s(context=%p, EventArgs=%p", __FUNCTION__, context, (void*)e);
		return;
	}

	afc = (androidContext*)context;
	settings = afc->common.context.settings;

	if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		android_cliprdr_uninit(afc, (CliprdrClientContext*)e->pInterface);
	}
	else
		freerdp_client_OnChannelDisconnectedEventHandler(context, e);
}

static BOOL android_begin_paint(rdpContext* context)
{
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

	settings = context->settings;

	if (!settings)
		return FALSE;

	gdi = context->gdi;

	if (!gdi || !gdi->primary || !gdi->primary->hdc)
		return FALSE;

	hwnd = ctx->common.context.gdi->primary->hdc->hwnd;

	if (!hwnd)
		return FALSE;

	ninvalid = hwnd->ninvalid;

	if (ninvalid < 1)
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

	freerdp_callback("OnGraphicsUpdate", "(JIIII)V", (jlong)context->instance, x1, y1, x2 - x1,
	                 y2 - y1);

	hwnd->invalid->null = TRUE;
	hwnd->ninvalid = 0;
	return TRUE;
}

static BOOL android_desktop_resize(rdpContext* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->settings);
	WINPR_ASSERT(context->instance);

	freerdp_callback("OnGraphicsResize", "(JIII)V", (jlong)context->instance,
	                 context->settings->DesktopWidth, context->settings->DesktopHeight,
	                 freerdp_settings_get_uint32(context->settings, FreeRDP_ColorDepth));
	return TRUE;
}

static BOOL android_pre_connect(freerdp* instance)
{
	int rc;
	rdpSettings* settings;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	settings = instance->context->settings;

	if (!settings)
		return FALSE;

	rc = PubSub_SubscribeChannelConnected(instance->context->pubSub,
	                                      android_OnChannelConnectedEventHandler);

	if (rc != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "Could not subscribe to connect event handler [%l08X]", rc);
		return FALSE;
	}

	rc = PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                         android_OnChannelDisconnectedEventHandler);

	if (rc != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "Could not subscribe to disconnect event handler [%l08X]", rc);
		return FALSE;
	}

	freerdp_callback("OnPreConnect", "(J)V", (jlong)instance);
	return TRUE;
}

static BOOL android_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(pointer);
	WINPR_ASSERT(context->gdi);

	return TRUE;
}

static void android_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	WINPR_ASSERT(context);
}

static BOOL android_Pointer_Set(rdpContext* context, rdpPointer* pointer)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(pointer);

	return TRUE;
}

static BOOL android_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
	WINPR_ASSERT(context);

	return TRUE;
}

static BOOL android_Pointer_SetNull(rdpContext* context)
{
	WINPR_ASSERT(context);

	return TRUE;
}

static BOOL android_Pointer_SetDefault(rdpContext* context)
{
	WINPR_ASSERT(context);

	return TRUE;
}

static BOOL android_register_pointer(rdpGraphics* graphics)
{
	rdpPointer pointer = { 0 };

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

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	update = instance->context->update;
	WINPR_ASSERT(update);

	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	if (!gdi_init(instance, PIXEL_FORMAT_RGBX32))
		return FALSE;

	if (!android_register_pointer(instance->context->graphics))
		return FALSE;

	update->BeginPaint = android_begin_paint;
	update->EndPaint = android_end_paint;
	update->DesktopResize = android_desktop_resize;
	freerdp_callback("OnSettingsChanged", "(JIII)V", (jlong)instance, settings->DesktopWidth,
	                 settings->DesktopHeight,
	                 freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth));
	freerdp_callback("OnConnectionSuccess", "(J)V", (jlong)instance);
	return TRUE;
}

static void android_post_disconnect(freerdp* instance)
{
	freerdp_callback("OnDisconnecting", "(J)V", (jlong)instance);
	gdi_free(instance);
}

static BOOL android_authenticate_int(freerdp* instance, char** username, char** password,
                                     char** domain, const char* cb_name)
{
	JNIEnv* env;
	jboolean attached = jni_attach_thread(&env);
	jobject jstr1 = create_string_builder(env, *username);
	jobject jstr2 = create_string_builder(env, *domain);
	jobject jstr3 = create_string_builder(env, *password);
	jboolean res;
	res = freerdp_callback_bool_result(cb_name,
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

static BOOL android_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	return android_authenticate_int(instance, username, password, domain, "OnAuthenticate");
}

static BOOL android_gw_authenticate(freerdp* instance, char** username, char** password,
                                    char** domain)
{
	return android_authenticate_int(instance, username, password, domain, "OnGatewayAuthenticate");
}

static DWORD android_verify_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                           const char* common_name, const char* subject,
                                           const char* issuer, const char* fingerprint, DWORD flags)
{
	WLog_DBG(TAG, "Certificate details [%s:%" PRIu16 ":", host, port);
	WLog_DBG(TAG, "\tSubject: %s", subject);
	WLog_DBG(TAG, "\tIssuer: %s", issuer);
	WLog_DBG(TAG, "\tThumbprint: %s", fingerprint);
	WLog_DBG(TAG,
	         "The above X.509 certificate could not be verified, possibly because you do not have "
	         "the CA certificate in your certificate store, or the certificate has expired."
	         "Please look at the OpenSSL documentation on how to add a private CA to the store.\n");
	JNIEnv* env;
	jboolean attached = jni_attach_thread(&env);
	jstring jstr0 = (*env)->NewStringUTF(env, host);
	jstring jstr1 = (*env)->NewStringUTF(env, common_name);
	jstring jstr2 = (*env)->NewStringUTF(env, subject);
	jstring jstr3 = (*env)->NewStringUTF(env, issuer);
	jstring jstr4 = (*env)->NewStringUTF(env, fingerprint);
	jint res = freerdp_callback_int_result("OnVerifyCertificateEx",
	                                       "(JLjava/lang/String;JLjava/lang/String;Ljava/lang/"
	                                       "String;Ljava/lang/String;Ljava/lang/String;J)I",
	                                       (jlong)instance, jstr0, (jlong)port, jstr1, jstr2, jstr3,
	                                       jstr4, (jlong)flags);

	if (attached == JNI_TRUE)
		jni_detach_thread();

	return res;
}

static DWORD android_verify_changed_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                                   const char* common_name, const char* subject,
                                                   const char* issuer, const char* new_fingerprint,
                                                   const char* old_subject, const char* old_issuer,
                                                   const char* old_fingerprint, DWORD flags)
{
	JNIEnv* env;
	jboolean attached = jni_attach_thread(&env);
	jstring jhost = (*env)->NewStringUTF(env, host);
	jstring jstr0 = (*env)->NewStringUTF(env, common_name);
	jstring jstr1 = (*env)->NewStringUTF(env, subject);
	jstring jstr2 = (*env)->NewStringUTF(env, issuer);
	jstring jstr3 = (*env)->NewStringUTF(env, new_fingerprint);
	jstring jstr4 = (*env)->NewStringUTF(env, old_subject);
	jstring jstr5 = (*env)->NewStringUTF(env, old_issuer);
	jstring jstr6 = (*env)->NewStringUTF(env, old_fingerprint);
	jint res =
	    freerdp_callback_int_result("OnVerifyChangedCertificateEx",
	                                "(JLjava/lang/String;JLjava/lang/String;Ljava/lang/"
	                                "String;Ljava/lang/String;Ljava/lang/String;"
	                                "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;J)I",
	                                (jlong)instance, jhost, (jlong)port, jstr0, jstr1, jstr2, jstr3,
	                                jstr4, jstr5, jstr6, (jlong)flags);

	if (attached == JNI_TRUE)
		jni_detach_thread();

	return res;
}

static int android_freerdp_run(freerdp* instance)
{
	DWORD count;
	DWORD status = WAIT_FAILED;
	HANDLE handles[MAXIMUM_WAIT_OBJECTS];
	HANDLE inputEvent = NULL;
	const rdpSettings* settings = instance->context->settings;
	rdpContext* context = instance->context;

	inputEvent = android_get_handle(instance);

	while (!freerdp_shall_disconnect_context(instance->context))
	{
		DWORD tmp;
		count = 0;

		handles[count++] = inputEvent;

		tmp = freerdp_get_event_handles(context, &handles[count], 64 - count);

		if (tmp == 0)
		{
			WLog_ERR(TAG, "freerdp_get_event_handles failed");
			break;
		}

		count += tmp;
		status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForMultipleObjects failed with %" PRIu32 " [%08lX]", status,
			         GetLastError());
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

		if (freerdp_shall_disconnect_context(instance->context))
			break;

		if (android_check_handle(instance) != TRUE)
		{
			WLog_ERR(TAG, "Failed to check android file descriptor");
			status = GetLastError();
			break;
		}
	}

disconnect:
	WLog_INFO(TAG, "Prepare shutdown...");

	return status;
}

static DWORD WINAPI android_thread_func(LPVOID param)
{
	DWORD status = ERROR_BAD_ARGUMENTS;
	freerdp* instance = param;
	WLog_DBG(TAG, "Start...");

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

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
	WLog_DBG(TAG, "Session ended with %08" PRIX32 "", status);

	if (status == CHANNEL_RC_OK)
		freerdp_callback("OnDisconnected", "(J)V", (jlong)instance);
	else
		freerdp_callback("OnConnectionFailure", "(J)V", (jlong)instance);

	WLog_DBG(TAG, "Quit.");
	ExitThread(status);
	return status;
}

static BOOL android_client_new(freerdp* instance, rdpContext* context)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(context);

	if (!android_event_queue_init(instance))
		return FALSE;

	instance->PreConnect = android_pre_connect;
	instance->PostConnect = android_post_connect;
	instance->PostDisconnect = android_post_disconnect;
	instance->Authenticate = android_authenticate;
	instance->GatewayAuthenticate = android_gw_authenticate;
	instance->VerifyCertificateEx = android_verify_certificate_ex;
	instance->VerifyChangedCertificateEx = android_verify_changed_certificate_ex;
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

JNIEXPORT jlong JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1new(
    JNIEnv* env, jclass cls, jobject context)
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
		WLog_FATAL(TAG, "Failed to load class references %s=%p, %s=%p", JAVA_CONTEXT_CLASS,
		           (void*)contextClass, JAVA_FILE_CLASS, (void*)fileClass);
		return (jlong)NULL;
	}

	getFilesDirID =
	    (*env)->GetMethodID(env, contextClass, "getFilesDir", "()L" JAVA_FILE_CLASS ";");

	if (!getFilesDirID)
	{
		WLog_FATAL(TAG, "Failed to find method ID getFilesDir ()L" JAVA_FILE_CLASS ";");
		return (jlong)NULL;
	}

	getAbsolutePathID =
	    (*env)->GetMethodID(env, fileClass, "getAbsolutePath", "()Ljava/lang/String;");

	if (!getAbsolutePathID)
	{
		WLog_FATAL(TAG, "Failed to find method ID getAbsolutePath ()Ljava/lang/String;");
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
		WLog_FATAL(TAG, "Failed to set environemnt HOME=%s %s [%d]", env, strerror(errno), errno);
		return (jlong)NULL;
	}

	RdpClientEntry(&clientEntryPoints);
	ctx = freerdp_client_context_new(&clientEntryPoints);

	if (!ctx)
		return (jlong)NULL;

	return (jlong)ctx->instance;
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1free(
    JNIEnv* env, jclass cls, jlong instance)
{
	freerdp* inst = (freerdp*)instance;

	if (inst)
		freerdp_client_context_free(inst->context);

#if defined(WITH_GPROF)
	moncleanup();
#endif
}

JNIEXPORT jstring JNICALL
Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1get_1last_1error_1string(JNIEnv* env,
                                                                                   jclass cls,
                                                                                   jlong instance)
{
	freerdp* inst = (freerdp*)instance;

	if (!inst || !inst->context)
		return (*env)->NewStringUTF(env, "");

	return (*env)->NewStringUTF(
	    env, freerdp_get_last_error_string(freerdp_get_last_error(inst->context)));
}

JNIEXPORT jboolean JNICALL
Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1parse_1arguments(JNIEnv* env, jclass cls,
                                                                           jlong instance,
                                                                           jobjectArray arguments)
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

	status =
	    freerdp_client_settings_parse_command_line(inst->context->settings, count, argv, FALSE);

	for (i = 0; i < count; i++)
		free(argv[i]);

	free(argv);
	return (status == 0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1connect(
    JNIEnv* env, jclass cls, jlong instance)
{
	freerdp* inst = (freerdp*)instance;
	androidContext* ctx;

	if (!inst || !inst->context)
	{
		WLog_FATAL(TAG, "%s(env=%p, cls=%p, instance=%d", __FUNCTION__, (void*)env, (void*)cls,
		           instance);
		return JNI_FALSE;
	}

	ctx = (androidContext*)inst->context;

	if (!(ctx->thread = CreateThread(NULL, 0, android_thread_func, inst, 0, NULL)))
	{
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1disconnect(
    JNIEnv* env, jclass cls, jlong instance)
{
	freerdp* inst = (freerdp*)instance;
	androidContext* ctx;
	ANDROID_EVENT* event;

	if (!inst || !inst->context || !cls || !env)
	{
		WLog_FATAL(TAG, "%s(env=%p, cls=%p, instance=%d", __FUNCTION__, (void*)env, (void*)cls,
		           instance);
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

	if (!freerdp_abort_connect_context(inst->context))
		return JNI_FALSE;

	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1update_1graphics(JNIEnv* env, jclass cls,
                                                                           jlong instance,
                                                                           jobject bitmap, jint x,
                                                                           jint y, jint width,
                                                                           jint height)
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
		WLog_FATAL(TAG, "%s(env=%p, cls=%p, instance=%d", __FUNCTION__, (void*)env, (void*)cls,
		           instance);
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
			DstFormat = PIXEL_FORMAT_RGBX32;
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

JNIEXPORT jboolean JNICALL
Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1send_1key_1event(JNIEnv* env, jclass cls,
                                                                           jlong instance,
                                                                           jint keycode,
                                                                           jboolean down)
{
	DWORD scancode;
	ANDROID_EVENT* event;
	freerdp* inst = (freerdp*)instance;
	scancode = GetVirtualScanCodeFromVirtualKeyCode(keycode, 4);
	int flags = (down == JNI_TRUE) ? KBD_FLAGS_DOWN : KBD_FLAGS_RELEASE;
	flags |= (scancode & KBDEXT) ? KBD_FLAGS_EXTENDED : 0;
	event = (ANDROID_EVENT*)android_event_key_new(flags, scancode & 0xFF);

	if (!event)
		return JNI_FALSE;

	if (!android_push_event(inst, event))
	{
		android_event_free(event);
		return JNI_FALSE;
	}

	WLog_DBG(TAG, "send_key_event: %" PRIu32 ", %d", scancode, flags);
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1send_1unicodekey_1event(
    JNIEnv* env, jclass cls, jlong instance, jint keycode, jboolean down)
{
	ANDROID_EVENT* event;
	freerdp* inst = (freerdp*)instance;
	UINT16 flags = (down == JNI_TRUE) ? 0 : KBD_FLAGS_RELEASE;
	event = (ANDROID_EVENT*)android_event_unicodekey_new(flags, keycode);

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

JNIEXPORT jboolean JNICALL
Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1send_1cursor_1event(
    JNIEnv* env, jclass cls, jlong instance, jint x, jint y, jint flags)
{
	ANDROID_EVENT* event;
	freerdp* inst = (freerdp*)instance;
	event = (ANDROID_EVENT*)android_event_cursor_new(flags, x, y);

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

JNIEXPORT jboolean JNICALL
Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1send_1clipboard_1data(JNIEnv* env,
                                                                                jclass cls,
                                                                                jlong instance,
                                                                                jstring jdata)
{
	ANDROID_EVENT* event;
	freerdp* inst = (freerdp*)instance;
	const char* data = jdata != NULL ? (*env)->GetStringUTFChars(env, jdata, NULL) : NULL;
	const size_t data_length = data ? (*env)->GetStringUTFLength(env, jdata) : 0;
	jboolean ret = JNI_FALSE;
	event = (ANDROID_EVENT*)android_event_clipboard_new((void*)data, data_length);

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

JNIEXPORT jstring JNICALL
Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1get_1jni_1version(JNIEnv* env, jclass cls)
{
	return (*env)->NewStringUTF(env, FREERDP_JNI_VERSION);
}

JNIEXPORT jboolean JNICALL
Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1has_1h264(JNIEnv* env, jclass cls)
{
	H264_CONTEXT* ctx = h264_context_new(FALSE);
	if (!ctx)
		return JNI_FALSE;
	h264_context_free(ctx);
	return JNI_TRUE;
}

JNIEXPORT jstring JNICALL
Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1get_1version(JNIEnv* env, jclass cls)
{
	return (*env)->NewStringUTF(env, freerdp_get_version_string());
}

JNIEXPORT jstring JNICALL
Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1get_1build_1revision(JNIEnv* env,
                                                                               jclass cls)
{
	return (*env)->NewStringUTF(env, freerdp_get_build_revision());
}

JNIEXPORT jstring JNICALL
Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1get_1build_1config(JNIEnv* env,
                                                                             jclass cls)
{
	return (*env)->NewStringUTF(env, freerdp_get_build_config());
}

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

	if (gJavaActivityClass)
		(*env)->DeleteGlobalRef(env, gJavaActivityClass);
}
