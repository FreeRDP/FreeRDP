/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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

#ifndef FREERDP_SERVER_PROXY_FILTERS_API_H
#define FREERDP_SERVER_PROXY_FILTERS_API_H

#include <inttypes.h>
#include <stdbool.h>

enum pf_filter_result {
    FILTER_PASS = 0,
    FILTER_DROP,
    FILTER_IGNORE
};

typedef enum pf_filter_result PF_FILTER_RESULT;
typedef struct connection_info connectionInfo;
typedef struct proxy_events proxyEvents;
typedef struct proxy_keyboard_event_info proxyKeyboardEventInfo;
typedef struct proxy_mouse_event_info proxyMouseEventInfo;
typedef PF_FILTER_RESULT(*proxyEvent)(connectionInfo* info, void* param);

struct connection_info {
    char* TargetHostname;
    char* ClientHostname;
    char* Username;
};

struct proxy_events {
    proxyEvent KeyboardEvent;
    proxyEvent MouseEvent;
};

#pragma pack(push, 1)
struct proxy_keyboard_event_info {
    uint16_t flags;
    uint16_t rdp_scan_code;
};

struct proxy_mouse_event_info {
    uint16_t flags;
    uint16_t x;
    uint16_t y;
};
#pragma pack(pop)


/* implement this method and register callbacks for proxy events
 * return TRUE if initialization succeeded, otherwise FALSE.
 **/
bool filter_init(proxyEvents* events);

#endif /* FREERDP_SERVER_PROXY_FILTERS_API_H */
