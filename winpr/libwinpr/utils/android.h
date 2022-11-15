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

#ifndef WINPR_UTILS_ANDROID_PRIV_H
#define WINPR_UTILS_ANDROID_PRIV_H

#include <jni.h>

extern JavaVM* jniVm;

jboolean winpr_jni_attach_thread(JNIEnv** env);
void winpr_jni_detach_thread(void);

#endif /* WINPR_UTILS_ANDROID_PRIV_H */
