/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Android Clipboard Redirection
 *
 * Copyright 2013 Felix Long
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

#ifndef __ANDROID_CLIPRDR_H__
#define __ANDROID_CLIPRDR_H__

#include "android_freerdp.h"

void android_cliprdr_init(freerdp* inst);
void android_cliprdr_uninit(freerdp* inst);
void android_process_cliprdr_send_clipboard_data(freerdp* inst, void* data, int len);
void android_process_cliprdr_event(freerdp* inst, wMessage* event);

#endif /* __ANDROID_CLIPRDR_H__ */
