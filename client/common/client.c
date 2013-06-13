/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Client Common
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "freerdp/client.h"


freerdp* freerdp_client_get_instance(rdpContext* cfc)
{
    return cfc->instance;
}

BOOL freerdp_client_get_param_bool(rdpContext* cfc, int id)
{
    rdpSettings* settings = cfc->instance->settings;

    return freerdp_get_param_bool(settings, id);
}

int freerdp_client_set_param_bool(rdpContext* cfc, int id, BOOL param)
{
    rdpSettings* settings = cfc->instance->settings;

    return freerdp_set_param_bool(settings, id, param);
}

UINT32 freerdp_client_get_param_uint32(rdpContext* cfc, int id)
{
    rdpSettings* settings = cfc->instance->settings;

    return freerdp_get_param_uint32(settings, id);
}

int freerdp_client_set_param_uint32(rdpContext* cfc, int id, UINT32 param)
{
    rdpSettings* settings = cfc->instance->settings;

    return freerdp_set_param_uint32(settings, id, param);
}

UINT64 freerdp_client_get_param_uint64(rdpContext* cfc, int id)
{
    rdpSettings* settings = cfc->instance->settings;

    return freerdp_get_param_uint64(settings, id);
}

int freerdp_client_set_param_uint64(rdpContext* cfc, int id, UINT64 param)
{
    rdpSettings* settings = cfc->instance->settings;

    return freerdp_set_param_uint64(settings, id, param);
}

char* freerdp_client_get_param_string(rdpContext* cfc, int id)
{
    rdpSettings* settings = cfc->instance->settings;

    return freerdp_get_param_string(settings, id);
}

int freerdp_client_set_param_string(rdpContext* cfc, int id, char* param)
{
    rdpSettings* settings = cfc->instance->settings;

    return freerdp_set_param_string(settings, id, param);
}




