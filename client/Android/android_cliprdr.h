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

#include <freerdp/client/cliprdr.h>

#include "android_freerdp.h"

int android_cliprdr_send_client_format_list(CliprdrClientContext* cliprdr);

int android_cliprdr_init(androidContext* afc, CliprdrClientContext* cliprdr);
int android_cliprdr_uninit(androidContext* afc, CliprdrClientContext* cliprdr);

#endif /* __ANDROID_CLIPRDR_H__ */
