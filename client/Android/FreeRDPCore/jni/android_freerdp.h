/*
   Android JNI Client Layer

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef __ANDROID_FREERDP_H
#define __ANDROID_FREERDP_H

#include <jni.h>

#include <winpr/crt.h>
#include <winpr/clipboard.h>

#include <freerdp/freerdp.h>
#include <freerdp/client/cliprdr.h>

#include "android_event.h"

struct android_context
{
	rdpContext rdpCtx;

	ANDROID_EVENT_QUEUE* event_queue;
	HANDLE thread;

	BOOL is_connected;

	BOOL clipboardSync;
	wClipboard* clipboard;
	UINT32 numServerFormats;
	UINT32 requestedFormatId;
	HANDLE clipboardRequestEvent;
	CLIPRDR_FORMAT* serverFormats;
	CliprdrClientContext* cliprdr;
	UINT32 clipboardCapabilities;
};
typedef struct android_context androidContext;

JNIEXPORT jint JNICALL jni_freerdp_new(JNIEnv *env, jclass cls);
JNIEXPORT void JNICALL jni_freerdp_free(JNIEnv *env, jclass cls, jint instance);
JNIEXPORT jboolean JNICALL jni_freerdp_connect(JNIEnv *env, jclass cls, jint instance);
JNIEXPORT jboolean JNICALL jni_freerdp_disconnect(JNIEnv *env, jclass cls, jint instance);
JNIEXPORT jboolean JNICALL jni_freerdp_cancel_connection(JNIEnv *env, jclass cls, jint instance);
JNIEXPORT jboolean JNICALL jni_freerdp_set_connection_info(JNIEnv *env, jclass cls, jint instance,
	jstring jhostname, jstring jusername, jstring jpassword, jstring jdomain, jint width,
	jint height, jint color_depth, jint port, jboolean console, jint security, jstring jcertname);
JNIEXPORT void JNICALL jni_freerdp_set_performance_flags(JNIEnv *env, jclass cls, jint instance, jboolean remotefx, jboolean disableWallpaper, jboolean disableFullWindowDrag,
	jboolean disableMenuAnimations, jboolean disableTheming, jboolean enableFontSmoothing, jboolean enableDesktopComposition);
JNIEXPORT jboolean JNICALL jni_freerdp_set_advanced_settings(JNIEnv *env, jclass cls,
		jint instance, jstring jRemoteProgram, jstring jWorkDir,
		jboolean async_channel, jboolean async_transport, jboolean async_input,
		jboolean async_update);
JNIEXPORT jboolean JNICALL jni_freerdp_set_drive_redirection(JNIEnv *env, jclass cls, jint instance, jstring jpath);
JNIEXPORT jboolean JNICALL jni_freerdp_set_sound_redirection(JNIEnv *env, jclass cls, jint instance, jint redirect);
JNIEXPORT jboolean JNICALL jni_freerdp_set_microphone_redirection(JNIEnv *env, jclass cls, jint instance, jboolean enable);
JNIEXPORT void JNICALL jni_freerdp_set_clipboard_redirection(JNIEnv *env, jclass cls, jint instance, jboolean enable);
JNIEXPORT jboolean JNICALL jni_freerdp_set_gateway_info(JNIEnv *env, jclass cls, jint instance, jstring jgatewayhostname, jint port, jstring jgatewayusername, jstring jgatewaypassword, jstring jgatewaydomain);
JNIEXPORT jboolean JNICALL jni_freerdp_set_data_directory(JNIEnv *env, jclass cls, jint instance, jstring jdirectory);
JNIEXPORT jboolean JNICALL jni_freerdp_update_graphics(JNIEnv *env, jclass cls, jint instance, jobject bitmap, jint x, jint y, jint width, jint height);
JNIEXPORT jboolean JNICALL jni_freerdp_send_cursor_event(JNIEnv *env, jclass cls, jint instance, jint x, jint y, jint flags);
JNIEXPORT jboolean JNICALL jni_freerdp_send_key_event(JNIEnv *env, jclass cls, jint instance, jint keycode, jboolean down);
JNIEXPORT jboolean JNICALL jni_freerdp_send_unicodekey_event(JNIEnv *env, jclass cls, jint instance, jint keycode);
JNIEXPORT jboolean JNICALL jni_freerdp_send_clipboard_data(JNIEnv *env, jclass cls, jint instance, jstring jdata);
JNIEXPORT jstring JNICALL jni_freerdp_get_version(JNIEnv *env, jclass cls);

#endif /* __ANDROID_FREERDP_H */

