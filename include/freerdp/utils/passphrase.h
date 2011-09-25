/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Passphrase Handling Utils
 *
 * Copyright 2011 Shea Levy <shea@shealevy.com>
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

#ifndef __UTILS_PASSPHRASE_H
#define __UTILS_PASSPHRASE_H

#include <stdlib.h>
#include <freerdp/api.h>

FREERDP_API char* freerdp_passphrase_read(const char* prompt, char* buf, size_t bufsiz);

#endif /* __UTILS_PASSPHRASE_H */
