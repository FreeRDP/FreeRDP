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
#ifndef _ANDROID_JNI_UTILS_H_
#define _ANDROID_JNI_UTILS_H_

#include <jni.h>

#ifdef __cplusplus
extern "C"
{
#endif

JNIEnv* getJNIEnv();
JavaVM* getJavaVM();

char* get_string_from_string_builder(JNIEnv* env, jobject strBuilder);
jobject create_string_builder(JNIEnv *env, char* initialStr);
jstring jniNewStringUTF(JNIEnv* env, const char* in, int len);

extern JavaVM *g_JavaVm;

#ifdef __cplusplus
}
#endif

#endif /* _ANDROID_JNI_UTILS_H_ */
