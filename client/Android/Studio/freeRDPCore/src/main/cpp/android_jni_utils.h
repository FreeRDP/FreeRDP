/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Android Event System
 *
 * Copyright 2013 Felix Long
 * Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef FREERDP_CLIENT_ANDROID_JNI_UTILS_H
#define FREERDP_CLIENT_ANDROID_JNI_UTILS_H

#include <jni.h>
#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL WINPR_ATTR_NODISCARD JNIEnv* getJNIEnv();
	FREERDP_LOCAL WINPR_ATTR_NODISCARD JavaVM* getJavaVM();

	FREERDP_LOCAL WINPR_ATTR_NODISCARD char* get_string_from_string_builder(JNIEnv* env,
	                                                                        jobject strBuilder);
	FREERDP_LOCAL WINPR_ATTR_NODISCARD jobject create_string_builder(JNIEnv* env, char* initialStr);
	FREERDP_LOCAL WINPR_ATTR_NODISCARD jstring jniNewStringUTF(JNIEnv* env, const char* in,
	                                                           int len);

	FREERDP_LOCAL extern JavaVM* g_JavaVm;

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_ANDROID_JNI_UTILS_H */
