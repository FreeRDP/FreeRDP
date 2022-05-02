#include <freerdp/settings.h>
#include "settings_property_lists.h"

static BOOL compare(const ADDIN_ARGV* got, const ADDIN_ARGV* expect)
{
	int x;
	if (!got && expect)
		return FALSE;
	if (got && !expect)
		return FALSE;
	if (got->argc != expect->argc)
		return FALSE;

	for (x = 0; x < expect->argc; x++)
	{
		if (strcmp(got->argv[x], expect->argv[x]) != 0)
			return FALSE;
	}
	return TRUE;
}

static BOOL test_dyn_channels(void)
{
	BOOL rc = FALSE;
	BOOL test;
	UINT32 u32;
	rdpSettings* settings = freerdp_settings_new(0);
	const char* argv1[] = { "foobar" };
	ADDIN_ARGV* args1 = NULL;
	const ADDIN_ARGV* cmp1;
	const char* argv2[] = { "gaga", "abba", "foo" };
	ADDIN_ARGV* args2 = NULL;
	const ADDIN_ARGV* cmp2;
	const ADDIN_ARGV* got;

	if (!settings)
		goto fail;

	u32 = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount);
	if (u32 != 0)
		goto fail;

	/* Test the function return an error for unknown channels */
	test = freerdp_dynamic_channel_collection_del(settings, "foobar");
	if (test)
		goto fail;
	got = freerdp_dynamic_channel_collection_find(settings, "foobar");
	if (got)
		goto fail;

	/* Add the channel */
	cmp1 = args1 = freerdp_addin_argv_new(ARRAYSIZE(argv1), argv1);
	test = freerdp_dynamic_channel_collection_add(settings, args1);
	if (!test)
		goto fail;
	args1 = NULL; /* settings have taken ownership */

	u32 = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount);
	if (u32 != 1)
		goto fail;
	u32 = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelArraySize);
	if (u32 < 1)
		goto fail;

	cmp2 = args2 = freerdp_addin_argv_new(ARRAYSIZE(argv2), argv2);
	test = freerdp_dynamic_channel_collection_add(settings, args2);
	if (!test)
		goto fail;
	args2 = NULL; /* settings have taken ownership */

	u32 = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount);
	if (u32 != 2)
		goto fail;
	u32 = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelArraySize);
	if (u32 < 2)
		goto fail;

	/* Test the function return success for known channels */
	got = freerdp_dynamic_channel_collection_find(settings, "foobar");
	if (!compare(got, cmp1))
		goto fail;
	got = freerdp_dynamic_channel_collection_find(settings, "gaga");
	if (!compare(got, cmp2))
		goto fail;
	test = freerdp_dynamic_channel_collection_del(settings, "foobar");
	if (!test)
		goto fail;
	u32 = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount);
	if (u32 != 1)
		goto fail;
	u32 = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelArraySize);
	if (u32 < 1)
		goto fail;
	got = freerdp_dynamic_channel_collection_find(settings, "foobar");
	if (compare(got, cmp1))
		goto fail;
	got = freerdp_dynamic_channel_collection_find(settings, "gaga");
	if (!compare(got, cmp2))
		goto fail;
	test = freerdp_dynamic_channel_collection_del(settings, "gaga");
	if (!test)
		goto fail;
	u32 = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount);
	if (u32 != 0)
		goto fail;
	got = freerdp_dynamic_channel_collection_find(settings, "foobar");
	if (compare(got, cmp1))
		goto fail;
	got = freerdp_dynamic_channel_collection_find(settings, "gaga");
	if (compare(got, cmp2))
		goto fail;

	rc = TRUE;

fail:
	freerdp_settings_free(settings);
	freerdp_addin_argv_free(args1);
	freerdp_addin_argv_free(args2);
	return rc;
}

static BOOL test_static_channels(void)
{
	BOOL rc = FALSE;
	BOOL test;
	UINT32 u32;
	rdpSettings* settings = freerdp_settings_new(0);
	const char* argv1[] = { "foobar" };
	ADDIN_ARGV* args1 = NULL;
	const ADDIN_ARGV* cmp1;
	const char* argv2[] = { "gaga", "abba", "foo" };
	ADDIN_ARGV* args2 = NULL;
	const ADDIN_ARGV* cmp2;
	const ADDIN_ARGV* got;

	if (!settings)
		goto fail;

	u32 = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount);
	if (u32 != 0)
		goto fail;

	/* Test the function return an error for unknown channels */
	test = freerdp_static_channel_collection_del(settings, "foobar");
	if (test)
		goto fail;
	got = freerdp_static_channel_collection_find(settings, "foobar");
	if (got)
		goto fail;

	/* Add the channel */
	cmp1 = args1 = freerdp_addin_argv_new(ARRAYSIZE(argv1), argv1);
	test = freerdp_static_channel_collection_add(settings, args1);
	if (!test)
		goto fail;
	args1 = NULL; /* settings have taken ownership */

	u32 = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount);
	if (u32 != 1)
		goto fail;
	u32 = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelArraySize);
	if (u32 < 1)
		goto fail;

	cmp2 = args2 = freerdp_addin_argv_new(ARRAYSIZE(argv2), argv2);
	test = freerdp_static_channel_collection_add(settings, args2);
	if (!test)
		goto fail;
	args2 = NULL; /* settings have taken ownership */

	u32 = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount);
	if (u32 != 2)
		goto fail;
	u32 = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelArraySize);
	if (u32 < 2)
		goto fail;

	/* Test the function return success for known channels */
	got = freerdp_static_channel_collection_find(settings, "foobar");
	if (!compare(got, cmp1))
		goto fail;
	got = freerdp_static_channel_collection_find(settings, "gaga");
	if (!compare(got, cmp2))
		goto fail;
	test = freerdp_static_channel_collection_del(settings, "foobar");
	if (!test)
		goto fail;
	u32 = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount);
	if (u32 != 1)
		goto fail;
	u32 = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelArraySize);
	if (u32 < 1)
		goto fail;
	got = freerdp_static_channel_collection_find(settings, "foobar");
	if (compare(got, cmp1))
		goto fail;
	got = freerdp_static_channel_collection_find(settings, "gaga");
	if (!compare(got, cmp2))
		goto fail;
	test = freerdp_static_channel_collection_del(settings, "gaga");
	if (!test)
		goto fail;
	u32 = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount);
	if (u32 != 0)
		goto fail;
	got = freerdp_static_channel_collection_find(settings, "foobar");
	if (compare(got, cmp1))
		goto fail;
	got = freerdp_static_channel_collection_find(settings, "gaga");
	if (compare(got, cmp2))
		goto fail;

	rc = TRUE;

fail:
	freerdp_settings_free(settings);
	freerdp_addin_argv_free(args1);
	freerdp_addin_argv_free(args2);
	return rc;
}
int TestSettings(int argc, char* argv[])
{
	int rc = -1;
	size_t x;
	rdpSettings* settings = NULL;
	rdpSettings* cloned = NULL;
	rdpSettings* cloned2 = NULL;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_dyn_channels())
		return -1;
	if (!test_static_channels())
		return -1;
	settings = freerdp_settings_new(0);

	if (!settings)
	{
		printf("Couldn't create settings\n");
		return -1;
	}

	if (!freerdp_settings_set_string(settings, FreeRDP_Username, "abcdefg"))
		goto fail;
	if (!freerdp_settings_set_string(settings, FreeRDP_Password, "xyz"))
		goto fail;

	cloned = freerdp_settings_clone(settings);

	if (!cloned)
		goto fail;

#if defined(have_bool_list_indices)

	for (x = 0; x < ARRAYSIZE(bool_list_indices); x++)
	{
		const size_t key = bool_list_indices[x];
		const char* name = freerdp_settings_get_name_for_key(key);
		const BOOL val = freerdp_settings_get_bool(settings, key);
		const BOOL cval = freerdp_settings_get_bool(cloned, key);
		if (val != cval)
		{
			printf("mismatch for key %s: %u -> copy %u\n", name, val, cval);
			goto fail;
		}
		if (!freerdp_settings_set_bool(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_int16_list_indices)

	for (x = 0; x < ARRAYSIZE(int16_list_indices); x++)
	{
		const size_t key = int16_list_indices[x];
		const char* name = freerdp_settings_get_name_for_key(key);
		const INT16 val = freerdp_settings_get_int16(settings, key);
		const INT16 cval = freerdp_settings_get_int16(cloned, key);
		if (val != cval)
		{
			printf("mismatch for key %s: %" PRId16 " -> copy %" PRId16 "\n", name, val, cval);
			goto fail;
		}
		if (!freerdp_settings_set_int16(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_uint16_list_indices)

	for (x = 0; x < ARRAYSIZE(uint16_list_indices); x++)
	{
		const size_t key = uint16_list_indices[x];
		const char* name = freerdp_settings_get_name_for_key(key);
		const UINT16 val = freerdp_settings_get_uint16(settings, key);
		const UINT16 cval = freerdp_settings_get_uint16(cloned, key);
		if (val != cval)
		{
			printf("mismatch for key %s: %" PRIu16 " -> copy %" PRIu16 "\n", name, val, cval);
			goto fail;
		}
		if (!freerdp_settings_set_uint16(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_uint32_list_indices)

	for (x = 0; x < ARRAYSIZE(uint32_list_indices); x++)
	{
		const size_t key = uint32_list_indices[x];
		const char* name = freerdp_settings_get_name_for_key(key);
		const UINT32 val = freerdp_settings_get_uint32(settings, key);
		const UINT32 cval = freerdp_settings_get_uint32(cloned, key);
		if (val != cval)
		{
			printf("mismatch for key %s: %" PRIu32 " -> copy %" PRIu32 "\n", name, val, cval);
			goto fail;
		}
		if (!freerdp_settings_set_uint32(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_int32_list_indices)

	for (x = 0; x < ARRAYSIZE(int32_list_indices); x++)
	{
		const size_t key = int32_list_indices[x];
		const char* name = freerdp_settings_get_name_for_key(key);
		const INT32 val = freerdp_settings_get_int32(settings, key);
		const INT32 cval = freerdp_settings_get_int32(cloned, key);
		if (val != cval)
		{
			printf("mismatch for key %s: %" PRId32 " -> copy %" PRId32 "\n", name, val, cval);
			goto fail;
		}
		if (!freerdp_settings_set_int32(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_uint64_list_indices)

	for (x = 0; x < ARRAYSIZE(uint64_list_indices); x++)
	{
		const size_t key = uint64_list_indices[x];
		const char* name = freerdp_settings_get_name_for_key(key);
		const UINT64 val = freerdp_settings_get_uint64(settings, key);
		const UINT64 cval = freerdp_settings_get_uint64(cloned, key);
		if (val != cval)
		{
			printf("mismatch for key %s: %" PRIu64 " -> copy %" PRIu64 "\n", name, val, cval);
			goto fail;
		}
		if (!freerdp_settings_set_uint64(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_int64_list_indices)

	for (x = 0; x < ARRAYSIZE(int64_list_indices); x++)
	{
		const size_t key = int64_list_indices[x];
		const char* name = freerdp_settings_get_name_for_key(key);
		const INT64 val = freerdp_settings_get_int64(settings, key);
		const INT64 cval = freerdp_settings_get_int64(cloned, key);
		if (val != cval)
		{
			printf("mismatch for key %s: %" PRId64 " -> copy %" PRId64 "\n", name, val, cval);
			goto fail;
		}
		if (!freerdp_settings_set_int64(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_string_list_indices)

	for (x = 0; x < ARRAYSIZE(string_list_indices); x++)
	{
		const size_t key = string_list_indices[x];
		const char val[] = "test-string";
		const char* res;
		const char* name = freerdp_settings_get_name_for_key(key);
		const char* oval = freerdp_settings_get_string(settings, key);
		const char* cval = freerdp_settings_get_string(cloned, key);
		if ((oval != cval) && (strcmp(oval, cval) != 0))
		{
			printf("mismatch for key %s: %s -> copy %s\n", name, oval, cval);
			goto fail;
		}
		if (!freerdp_settings_set_string(settings, key, val))
			goto fail;

		res = freerdp_settings_get_string(settings, key);

		if (strncmp(val, res, sizeof(val)) != 0)
			goto fail;
	}

#endif
#if defined(have_pointer_list_indices)

	for (x = 0; x < ARRAYSIZE(pointer_list_indices); x++)
	{
		const size_t key = pointer_list_indices[x];
		const void* val = freerdp_settings_get_pointer(settings, key);
		WINPR_UNUSED(val);
	}

#endif
	cloned2 = freerdp_settings_clone(settings);
	if (!cloned2)
		goto fail;
	if (!freerdp_settings_copy(cloned2, cloned))
		goto fail;

	rc = 0;
fail:
	freerdp_settings_free(cloned);
	freerdp_settings_free(cloned2);
	freerdp_settings_free(settings);
	return rc;
}
