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

// NOLINTNEXTLINE(readability-non-const-parameter)
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
// NOLINTNEXTLINE(bugprone-suspicious-memory-comparison,cert-exp42-c,cert-flp37-c)
POD_ARRAYS_IMPL(BasicStruct, basicstruct)

int TestPodArrays(int argc, char* argv[])
{
	int rc = -1;
	UINT32 sum = 0;
	UINT32 foreach_index = 0;
	ArrayUINT32 uint32s = { 0 };
	UINT32* ptr = NULL;
	const UINT32* cptr = NULL;
	ArrayBasicStruct basicStructs = { 0 };
	BasicStruct basicStruct = { 1, 2 };

	array_uint32_init(&uint32s);
	array_basicstruct_init(&basicStructs);

	for (UINT32 i = 0; i < 10; i++)
		if (!array_uint32_append(&uint32s, i))
			goto fail;

	sum = 0;
	if (!array_uint32_foreach(&uint32s, cb_compute_sum, &sum))
		goto fail;

	if (sum != 45)
		goto fail;

	foreach_index = 0;
	if (array_uint32_foreach(&uint32s, cb_stop_at_5, &foreach_index))
		goto fail;

	if (foreach_index != 5)
		goto fail;

	if (array_uint32_get(&uint32s, 4) != 4)
		goto fail;

	array_uint32_set(&uint32s, 4, 5);
	if (array_uint32_get(&uint32s, 4) != 5)
		goto fail;

	ptr = array_uint32_data(&uint32s);
	if (*ptr != 0)
		goto fail;

	cptr = array_uint32_cdata(&uint32s);
	if (*cptr != 0)
		goto fail;

	/* test modifying values of the array during the foreach */
	if (!array_uint32_foreach(&uint32s, cb_set_to_1, NULL) || array_uint32_get(&uint32s, 5) != 1)
		goto fail;

	/* this one is to test that we can modify the array itself during the foreach and that things
	 * go nicely */
	if (!array_uint32_foreach(&uint32s, cb_reset_after_1, &uint32s) || array_uint32_size(&uint32s))
		goto fail;

	/* give a try with an array of BasicStructs */
	if (!array_basicstruct_append(&basicStructs, basicStruct))
		goto fail;

	if (!array_basicstruct_foreach(&basicStructs, cb_basic_struct, NULL))
		goto fail;

	rc = 0;

fail:
	array_uint32_uninit(&uint32s);
	array_basicstruct_uninit(&basicStructs);

	return rc;
}
