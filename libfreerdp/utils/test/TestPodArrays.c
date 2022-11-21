/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * TestPodArrays
 *
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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
#include <freerdp/utils/pod_arrays.h>

static BOOL cb_compute_sum(UINT32* v, void* target)
{
	UINT32* ret = (UINT32*)target;
	*ret += *v;
	return TRUE;
}

static BOOL cb_stop_at_5(UINT32* v, void* target)
{
	UINT32* ret = (UINT32*)target;
	*ret += 1;
	return (*ret != 5);
}

static BOOL cb_set_to_1(UINT32* v, void* target)
{
	*v = 1;
	return TRUE;
}

static BOOL cb_reset_after_1(UINT32* v, void* target)
{
	ArrayUINT32* a = (ArrayUINT32*)target;
	array_uint32_reset(a);
	return TRUE;
}

typedef struct
{
	UINT32 v1;
	UINT16 v2;
} BasicStruct;

static BOOL cb_basic_struct(BasicStruct* v, void* target)
{
	return (v->v1 == 1) && (v->v2 == 2);
}

POD_ARRAYS_IMPL(BasicStruct, basicstruct)

int TestPodArrays(int argc, char* argv[])
{
	UINT32 i, sum, foreach_index;
	ArrayUINT32 uint32s;
	UINT32* ptr;
	const UINT32* cptr;
	ArrayBasicStruct basicStructs;
	BasicStruct basicStruct = { 1, 2 };

	array_uint32_init(&uint32s);
	for (i = 0; i < 10; i++)
		if (!array_uint32_append(&uint32s, i))
			return -1;

	sum = 0;
	if (!array_uint32_foreach(&uint32s, cb_compute_sum, &sum))
		return -2;

	if (sum != 45)
		return -3;

	foreach_index = 0;
	if (array_uint32_foreach(&uint32s, cb_stop_at_5, &foreach_index))
		return -4;

	if (foreach_index != 5)
		return -5;

	if (array_uint32_get(&uint32s, 4) != 4)
		return -6;

	array_uint32_set(&uint32s, 4, 5);
	if (array_uint32_get(&uint32s, 4) != 5)
		return -7;

	ptr = array_uint32_data(&uint32s);
	if (*ptr != 0)
		return -8;

	cptr = array_uint32_cdata(&uint32s);
	if (*cptr != 0)
		return -9;

	/* test modifying values of the array during the foreach */
	if (!array_uint32_foreach(&uint32s, cb_set_to_1, NULL) || array_uint32_get(&uint32s, 5) != 1)
		return -10;

	/* this one is to test that we can modify the array itself during the foreach and that things
	 * go nicely */
	if (!array_uint32_foreach(&uint32s, cb_reset_after_1, &uint32s) || array_uint32_size(&uint32s))
		return -11;

	array_uint32_uninit(&uint32s);

	/* give a try with an array of BasicStructs */
	array_basicstruct_init(&basicStructs);
	if (!array_basicstruct_append(&basicStructs, basicStruct))
		return -20;

	if (!array_basicstruct_foreach(&basicStructs, cb_basic_struct, NULL))
		return -21;

	array_basicstruct_uninit(&basicStructs);
	return 0;
}
