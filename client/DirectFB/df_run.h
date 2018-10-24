/**
 * FreeRDP: A Remote Desktop Protocol Client
 * DirectFB Graphical Objects
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

#ifndef __DF_RUN_H
#define __DF_RUN_H

#include "dfreerdp.h"

#define DF_LOCK_BIT_INIT		1
#define DF_LOCK_BIT_PAINT		2

boolean df_lock_fb(dfInfo *dfi, uint8 mask);
boolean df_unlock_fb(dfInfo *dfi, uint8 mask);

void df_run_register(freerdp* instance);
void df_run(freerdp* instance);

#endif /* __DF_RUN_H */
