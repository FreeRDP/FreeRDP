/**
 * FreeRDP: A Remote Desktop Protocol Client
 * DATABLOB Utils
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

#ifndef __DATABLOB_UTILS_H
#define __DATABLOB_UTILS_H

struct _DATABLOB
{
	void* data;
	int length;
};
typedef struct _DATABLOB DATABLOB;

void datablob_alloc(DATABLOB *datablob, int length);
void datablob_free(DATABLOB *datablob);

#endif /* __DATABLOB_UTILS_H */
