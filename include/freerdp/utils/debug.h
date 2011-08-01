/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Debug Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __UTILS_DEBUG_H
#define __UTILS_DEBUG_H

#include "config.h"

#include <stdio.h>

#define DEBUG_NULL(fmt, ...) do { } while (0)
#define DEBUG_PRINT(_dbg_str, fmt, ...) printf(_dbg_str fmt "\n" , __FUNCTION__, __LINE__, ## __VA_ARGS__)
#define DEBUG_CLASS(_dbg_class, fmt, ...) DEBUG_PRINT("DBG_" #_dbg_class " %s (%d): ", fmt, ## __VA_ARGS__)
#define DEBUG_WARN(fmt, ...) DEBUG_PRINT("Warning %s (%d): ", fmt, ## __VA_ARGS__)

#ifdef WITH_DEBUG
#define DEBUG(fmt, ...)	DEBUG_PRINT("DBG %s (%d): ", fmt, ## __VA_ARGS__)
#else
#define DEBUG(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __UTILS_DEBUG_H */
