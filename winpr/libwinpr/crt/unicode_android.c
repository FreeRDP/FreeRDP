/**
 * WinPR: Windows Portable Runtime
 * Unicode Conversion (CRT)
 *
 * Copyright 2022 Armin Novak <anovak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#include <winpr/config.h>
#include <winpr/assert.h>
#include <winpr/string.h>

#include "../utils/android.h"

#ifndef MIN
#define MIN(a, b) (a) < (b) ? (a) : (b)
#endif

#include "../log.h"
#define TAG WINPR_TAG("unicode")

static int convert_int(JNIEnv* env, const void* data, size_t size, void* buffer, size_t buffersize,
                       BOOL toUTF16)
{
	WINPR_ASSERT(env);
	WINPR_ASSERT(data || (size == 0));
	WINPR_ASSERT(buffer || (buffersize == 0));

	jstring utf8 = (*env)->NewStringUTF(env, "UTF-8");
	jstring utf16 = (*env)->NewStringUTF(env, "UTF-16LE");
	jclass stringClass = (*env)->FindClass(env, "java/lang/String");

	if (!utf8 || !utf16 || !stringClass)
	{
		WLog_ERR(TAG, "[%s] utf8-%p, utf16=%p, stringClass=%p", __func__, utf8, utf16, stringClass);
		return -1;
	}

	jmethodID constructorID =
	    (*env)->GetMethodID(env, stringClass, "<init>", "([BLjava/lang/String;)V");
	jmethodID getBytesID =
	    (*env)->GetMethodID(env, stringClass, "getBytes", "(Ljava/lang/String;)[B");
	if (!constructorID || !getBytesID)
	{
		WLog_ERR(TAG, "[%s] constructorID=%p, getBytesID=%p", __func__, constructorID, getBytesID);
		return -2;
	}

	jbyteArray ret = (*env)->NewByteArray(env, size);
	if (!ret)
	{
		WLog_ERR(TAG, "[%s] NewByteArray(%" PRIuz ") failed", __func__, size);
		return -3;
	}

	(*env)->SetByteArrayRegion(env, ret, 0, size, data);

	jobject obj = (*env)->NewObject(env, stringClass, constructorID, ret, toUTF16 ? utf8 : utf16);
	if (!obj)
	{
		WLog_ERR(TAG, "[%s] NewObject(String, byteArray, UTF-%d) failed", __func__,
		         toUTF16 ? 16 : 8);
		return -4;
	}

	jbyteArray res = (*env)->CallObjectMethod(env, obj, getBytesID, toUTF16 ? utf16 : utf8);
	if (!res)
	{
		WLog_ERR(TAG, "[%s] CallObjectMethod(String, getBytes, UTF-%d) failed", __func__,
		         toUTF16 ? 16 : 8);
		return -4;
	}

	jsize rlen = (*env)->GetArrayLength(env, res);
	if (buffersize > 0)
	{
		if (rlen > buffersize)
		{
			SetLastError(ERROR_INSUFFICIENT_BUFFER);
			return 0;
		}
		rlen = MIN(rlen, buffersize);
		(*env)->GetByteArrayRegion(env, res, 0, rlen, buffer);
	}

	if (toUTF16)
		rlen /= sizeof(WCHAR);

	return rlen;
}

static int convert(const void* data, size_t size, void* buffer, size_t buffersize, BOOL toUTF16)
{
	int rc;
	JNIEnv* env = NULL;
	jboolean attached = winpr_jni_attach_thread(&env);
	rc = convert_int(env, data, size, buffer, buffersize, toUTF16);
	if (attached)
		winpr_jni_detach_thread();
	return rc;
}

int int_MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte,
                            LPWSTR lpWideCharStr, int cchWideChar)
{
	size_t cbCharLen = (size_t)cbMultiByte;

	WINPR_UNUSED(dwFlags);

	/* If cbMultiByte is 0, the function fails */
	if ((cbMultiByte == 0) || (cbMultiByte < -1))
		return 0;

	if (cchWideChar < 0)
		return -1;

	if (cbMultiByte < 0)
	{
		const size_t len = strlen(lpMultiByteStr);
		if (len >= INT32_MAX)
			return 0;
		cbCharLen = (int)len + 1;
	}
	else
		cbCharLen = cbMultiByte;

	WINPR_ASSERT(lpMultiByteStr);
	switch (CodePage)
	{
		case CP_ACP:
		case CP_UTF8:
			break;

		default:
			WLog_ERR(TAG, "Unsupported encoding %u", CodePage);
			return 0;
	}

	return convert(lpMultiByteStr, cbCharLen, lpWideCharStr, cchWideChar * sizeof(WCHAR), TRUE);
}

int int_WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                            LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar,
                            LPBOOL lpUsedDefaultChar)
{
	size_t cbCharLen = (size_t)cchWideChar;

	WINPR_UNUSED(dwFlags);
	/* If cchWideChar is 0, the function fails */
	if ((cchWideChar == 0) || (cchWideChar < -1))
		return 0;

	if (cbMultiByte < 0)
		return -1;

	WINPR_ASSERT(lpWideCharStr);
	/* If cchWideChar is -1, the string is null-terminated */
	if (cchWideChar == -1)
	{
		const size_t len = _wcslen(lpWideCharStr);
		if (len >= INT32_MAX)
			return 0;
		cbCharLen = (int)len + 1;
	}
	else
		cbCharLen = cchWideChar;

	/*
	 * if cbMultiByte is 0, the function returns the required buffer size
	 * in bytes for lpMultiByteStr and makes no use of the output parameter itself.
	 */
	return convert(lpWideCharStr, cbCharLen * sizeof(WCHAR), lpMultiByteStr, cbMultiByte, FALSE);
}
