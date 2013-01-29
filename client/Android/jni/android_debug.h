/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Android Debug Interface
 *
 * Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FREERDP_ANDROID_DEBUG_H
#define FREERDP_ANDROID_DEBUG_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <android/log.h>

#define TAG	"LibFreeRDP"

#define DEBUG_ANDROID_NULL(fmt, ...) do { } while (0)
#define DEBUG_ANDROID_PRINT(_dbg_str, fmt, ...) __android_log_print(ANDROID_LOG_INFO, TAG, _dbg_str fmt "\n" , __FUNCTION__, __LINE__, ## __VA_ARGS__)
#define DEBUG_ANDROID_CLASS(_dbg_class, fmt, ...) DEBUG_ANDROID_PRINT("DBG_" #_dbg_class " %s (%d): ", fmt, ## __VA_ARGS__)

#ifdef WITH_DEBUG_ANDROID_JNI
#define DEBUG_ANDROID(fmt, ...)	DEBUG_ANDROID_PRINT("DBG %s (%d): ", fmt, ## __VA_ARGS__)
#else
#define DEBUG_ANDROID(fmt, ...) DEBUG_ANDROID_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* FREERDP_ANDROID_DEBUG_H */

