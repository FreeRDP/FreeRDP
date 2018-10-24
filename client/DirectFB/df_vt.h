/**
 * FreeRDP: A Remote Desktop Protocol Client
 * DirectFB Graphical Objects
 *
 * Copyright 2014 Killer{R} <support@killpprog.com>
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

#ifndef __DF_VT_H
#define __DF_VT_H

#include "dfreerdp.h"

void df_vt_register();
void df_vt_deregister();

boolean df_vt_is_disactivated_slow();
boolean df_vt_is_disactivated_fast_unreliable();

#endif // __DF_VT_H
