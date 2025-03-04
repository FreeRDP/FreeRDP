/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 debug helper header
 *
 * Copyright 2025 Armin Novak <anovak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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

#pragma once

#include <freerdp/config.h>
#include <freerdp/log.h>

#define DBG_TAG CLIENT_TAG("x11")

#ifdef WITH_DEBUG_X11
#define DEBUG_X11(...) WLog_DBG(DBG_TAG, __VA_ARGS__)
#else
#define DEBUG_X11(...) \
	do                 \
	{                  \
	} while (0)
#endif
