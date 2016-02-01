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

#include "android_jni_utils.h"

#include <locale.h>
#include <freerdp/channels/channels.h>
#include <freerdp/log.h>

#include "android_jni_callback.h"

#define TAG CLIENT_TAG("android.utils")

JavaVM *g_JavaVm;

JavaVM* getJavaVM()
{
	return g_JavaVm;
}

JNIEnv* getJNIEnv()
{
	JNIEnv* env = NULL;
	if ((*g_JavaVm)->GetEnv(g_JavaVm, (void**) &env, JNI_VERSION_1_4) != JNI_OK)
	{
		WLog_FATAL(TAG, "Failed to obtain JNIEnv");
		return NULL;
	}
	return env;
}

jobject create_string_builder(JNIEnv *env, char* initialStr)
{
	jclass cls;
	jmethodID methodId;
	jobject obj;

	// get class
	cls = (*env)->FindClass(env, "java/lang/StringBuilder");
	if(!cls)
		return NULL;

	if(initialStr)
	{
		// get method id for constructor
		methodId = (*env)->GetMethodID(env, cls, "<init>", "(Ljava/lang/String;)V");
		if(!methodId)
			return NULL;

		// create string that holds our initial string
		jstring jstr = (*env)->NewStringUTF(env, initialStr);

		// construct new StringBuilder
		obj = (*env)->NewObject(env, cls, methodId, jstr);
	}
	else
	{
		// get method id for constructor
		methodId = (*env)->GetMethodID(env, cls, "<init>", "()V");
		if(!methodId)
			return NULL;

		// construct new StringBuilder
		obj = (*env)->NewObject(env, cls, methodId);
	}

	return obj;
}

char* get_string_from_string_builder(JNIEnv* env, jobject strBuilder)
{
	jclass cls;
	jmethodID methodId;
	jstring strObj;
	const jbyte* native_str;
	char* result;

	// get class
	cls = (*env)->FindClass(env, "java/lang/StringBuilder");
	if(!cls)
		return NULL;

	// get method id for constructor
	methodId = (*env)->GetMethodID(env, cls, "toString", "()Ljava/lang/String;");
	if(!methodId)
		return NULL;

	// get jstring representation of our buffer
	strObj = (*env)->CallObjectMethod(env, strBuilder, methodId);

	// read string
	native_str = (*env)->GetStringUTFChars(env, strObj, NULL);
	if (!native_str)
		return NULL;
	result = strdup(native_str);
	(*env)->ReleaseStringUTFChars(env, strObj, native_str);

	return result;
}

jstring jniNewStringUTF(JNIEnv* env, const char* in, int len)
{
	jstring out = NULL;
	jchar* unicode = NULL;
	jint result_size = 0;
	jint i;
	unsigned char* utf8 = (unsigned char*)in;

	if (!in)
	{
		return NULL;
	}
	if (len < 0)
		len = strlen(in);

	unicode = (jchar*)malloc(sizeof(jchar) * (len + 1));
	if (!unicode)
	{
		return NULL;
	}

	for(i = 0; i < len; i++)
	{
		unsigned char one = utf8[i];
		switch(one >> 4)
		{
			case 0x00:
			case 0x01:
			case 0x02:
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
				unicode[result_size++] = one;
				break;
			case 0x08:
			case 0x09:
			case 0x0a:
			case 0x0b:
			//case 0x0f:
				/*
		 * Bit pattern 10xx or 1111, which are illegal start bytes.
		 * Note: 1111 is valid for normal UTF-8, but not the
		 * modified UTF-8 used here.
		 */
				break;
			case 0x0f:
			case 0x0e:
				// Bit pattern 111x, so there are two additional bytes.
				if (i < (len - 2))
				{
					unsigned char two = utf8[i+1];
					unsigned char three = utf8[i+2];
					if ((two & 0xc0) == 0x80 && (three & 0xc0) == 0x80)
					{
						i += 2;
						unicode[result_size++] =
								((one & 0x0f) << 12)
							  | ((two & 0x3f) << 6)
							  | (three & 0x3f);
					}
				}
				break;
			case 0x0c:
			case 0x0d:
				// Bit pattern 110x, so there is one additional byte.
				if (i < (len - 1))
				{
					unsigned char two = utf8[i+1];
					if ((two & 0xc0) == 0x80)
					{
						i += 1;
						unicode[result_size++] =
								((one & 0x1f) << 6)
							  | (two & 0x3f);
					}
				}
				break;
		}
	}

	out = (*env)->NewString(env, unicode, result_size);
	free(unicode);

	return out;
}
