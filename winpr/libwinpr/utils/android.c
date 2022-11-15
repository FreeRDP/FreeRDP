/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Winpr android helpers
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
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

#include "android.h"
#include <jni.h>

#include <winpr/winpr.h>
#include <winpr/assert.h>

#include "../log.h"

#define TAG WINPR_TAG("android")

JavaVM* jniVm = NULL;

WINPR_API jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	WLog_INFO(TAG, "Setting up JNI environement...");

	jniVm = vm;
	return JNI_VERSION_1_6;
}

WINPR_API void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved)
{
	JNIEnv* env = NULL;
	WLog_INFO(TAG, "Tearing down JNI environement...");

	if ((*jniVm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK)
	{
		WLog_FATAL(TAG, "Failed to get the environment");
		return;
	}
}

jboolean winpr_jni_attach_thread(JNIEnv** env)
{
	WINPR_ASSERT(jniVm);

	if ((*jniVm)->GetEnv(jniVm, (void**)env, JNI_VERSION_1_4) != JNI_OK)
	{
		WLog_INFO(TAG, "android_java_callback: attaching current thread");
		(*jniVm)->AttachCurrentThread(jniVm, env, NULL);

		if ((*jniVm)->GetEnv(jniVm, (void**)env, JNI_VERSION_1_4) != JNI_OK)
		{
			WLog_ERR(TAG, "android_java_callback: failed to obtain current JNI environment");
		}

		return JNI_TRUE;
	}

	return JNI_FALSE;
}

/* attach current thread to JVM */
void winpr_jni_detach_thread(void)
{
	WINPR_ASSERT(jniVm);
	(*jniVm)->DetachCurrentThread(jniVm);
}
