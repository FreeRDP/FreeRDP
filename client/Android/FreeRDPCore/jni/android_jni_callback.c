/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Android JNI Callback Helpers
 *
 * Copyright 2011-2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "android_jni_callback.h"
#include "android_debug.h"
#include "android_freerdp_jni.h"

JavaVM *jVM;
jobject jLibFreeRDPObject;

const char *jLibFreeRDPPath = JAVA_LIBFREERDP_CLASS;

void jni_load_class(JNIEnv *env, const char *path, jobject *objptr)
{
	jclass class;
	jmethodID method;
	jobject object;

	DEBUG_ANDROID("jni_load_class: %s", path);

	class = (*env)->FindClass(env, path);

	if (!class)
	{
		DEBUG_WARN("jni_load_class: failed to find class %s", path);
		goto finish;
	}

	method = (*env)->GetMethodID(env, class, "<init>", "()V");

	if (!method)
	{
		DEBUG_WARN("jni_load_class: failed to find class constructor of %s", path);
		goto finish;
	}

	object = (*env)->NewObject(env, class, method);

	if (!object)
	{
		DEBUG_WARN("jni_load_class: failed create new object of %s", path);
		goto finish;
	}

	(*objptr) = (*env)->NewGlobalRef(env, object);

finish:
	while(0);
}

jint init_callback_environment(JavaVM* vm)
{
	JNIEnv* env;
	if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK)
	{
		DEBUG_WARN("JNI_OnLoad: failed to obtain current JNI environment");
		return -1;
	}

	jVM = vm;

	jni_load_class(env, jLibFreeRDPPath, &jLibFreeRDPObject);

	return JNI_VERSION_1_4;
}

/* attach current thread to jvm */
jboolean jni_attach_thread(JNIEnv** env)
{
	if ((*jVM)->GetEnv(jVM, (void**) env, JNI_VERSION_1_4) != JNI_OK)
	{
		DEBUG_ANDROID("android_java_callback: attaching current thread");

		(*jVM)->AttachCurrentThread(jVM, env, NULL);

		if ((*jVM)->GetEnv(jVM, (void**) env, JNI_VERSION_1_4) != JNI_OK)
		{
			DEBUG_WARN("android_java_callback: failed to obtain current JNI environment");
		}

		return JNI_TRUE;
	}

	return JNI_FALSE;
}

/* attach current thread to JVM */
void jni_detach_thread()
{
	(*jVM)->DetachCurrentThread(jVM);
}

/* callback with void result */
void java_callback_void(jobject obj, const char * callback, const char* signature, va_list args)
{
	jclass jObjClass;
	jmethodID jCallback;
	jboolean attached;
	JNIEnv *env;

	DEBUG_ANDROID("java_callback: %s (%s)", callback, signature);

	attached = jni_attach_thread(&env);
	
	jObjClass = (*env)->GetObjectClass(env, obj);

	if (!jObjClass) {
		DEBUG_WARN("android_java_callback: failed to get class reference");
		goto finish;
	}

	jCallback = (*env)->GetStaticMethodID(env, jObjClass, callback, signature);

	if (!jCallback) {
		DEBUG_WARN("android_java_callback: failed to get method id");
		goto finish;
	}

	(*env)->CallStaticVoidMethodV(env, jObjClass, jCallback, args);

finish:
	if(attached == JNI_TRUE)
		jni_detach_thread();
}

/* callback with bool result */
jboolean java_callback_bool(jobject obj, const char * callback, const char* signature, va_list args)
{
	jclass jObjClass;
	jmethodID jCallback;
	jboolean attached;
	jboolean res = JNI_FALSE;
	JNIEnv *env;

	DEBUG_ANDROID("java_callback: %s (%s)", callback, signature);

	attached = jni_attach_thread(&env);
	
	jObjClass = (*env)->GetObjectClass(env, obj);

	if (!jObjClass) {
		DEBUG_WARN("android_java_callback: failed to get class reference");
		goto finish;
	}

	jCallback = (*env)->GetStaticMethodID(env, jObjClass, callback, signature);

	if (!jCallback) {
		DEBUG_WARN("android_java_callback: failed to get method id");
		goto finish;
	}

	res = (*env)->CallStaticBooleanMethodV(env, jObjClass, jCallback, args);

finish:
	if(attached == JNI_TRUE)
		jni_detach_thread();

	return res;
}

/* callback to freerdp class */
void freerdp_callback(const char * callback, const char * signature, ...)
{
	va_list vl;
  	va_start(vl, signature);
	java_callback_void(jLibFreeRDPObject, callback, signature, vl);
	va_end(vl);
}

jboolean freerdp_callback_bool_result(const char * callback, const char * signature, ...)
{
	va_list vl;
  	va_start(vl, signature);
	jboolean res = java_callback_bool(jLibFreeRDPObject, callback, signature, vl);
	va_end(vl);
	return res;
}
