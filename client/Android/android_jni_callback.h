/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Android JNI Callback Helpers
 *
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011-2013 Thincast Technologies GmbH, Author: Martin Fleisz
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FREERDP_ANDROID_JNI_CALLBACK_H
#define FREERDP_ANDROID_JNI_CALLBACK_H

#include <jni.h>
#include <stdarg.h>
#include <freerdp/api.h>

FREERDP_LOCAL jint init_callback_environment(JavaVM* vm, JNIEnv* env);
FREERDP_LOCAL jboolean jni_attach_thread(JNIEnv** env);
FREERDP_LOCAL void jni_detach_thread(void);
FREERDP_LOCAL void freerdp_callback(const char* callback, const char* signature,
                                    ...);
FREERDP_LOCAL jboolean freerdp_callback_bool_result(const char* callback,
        const char* signature, ...);
FREERDP_LOCAL jint freerdp_callback_int_result(const char* callback,
        const char* signature, ...);

#endif /* FREERDP_ANDROID_JNI_CALLBACK_H */

