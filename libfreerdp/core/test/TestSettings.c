#include <freerdp/settings.h>
#include "settings_property_lists.h"

int TestSettings(int argc, char* argv[])
{
	int rc = -1;
	size_t x;
	rdpSettings* settings = NULL;
	rdpSettings* cloned;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	settings = freerdp_settings_new(0);

	if (!settings)
	{
		printf("Couldn't create settings\n");
		return -1;
	}

	settings->Username = _strdup("abcdefg");
	settings->Password = _strdup("xyz");
	cloned = freerdp_settings_clone(settings);

	if (!cloned)
	{
		printf("Problem cloning settings\n");
		freerdp_settings_free(settings);
		return -1;
	}

#if defined(have_bool_list_indices)

	for (x = 0; x < ARRAYSIZE(bool_list_indices); x++)
	{
		const size_t key = bool_list_indices[x];
		const BOOL val = freerdp_settings_get_bool(settings, key);

		if (!freerdp_settings_set_bool(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_int16_list_indices)

	for (x = 0; x < ARRAYSIZE(int16_list_indices); x++)
	{
		const size_t key = int16_list_indices[x];
		const INT16 val = freerdp_settings_get_int16(settings, key);

		if (!freerdp_settings_set_int16(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_uint16_list_indices)

	for (x = 0; x < ARRAYSIZE(uint16_list_indices); x++)
	{
		const size_t key = uint16_list_indices[x];
		const UINT16 val = freerdp_settings_get_uint16(settings, key);

		if (!freerdp_settings_set_uint16(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_uint32_list_indices)

	for (x = 0; x < ARRAYSIZE(uint32_list_indices); x++)
	{
		const size_t key = uint32_list_indices[x];
		const UINT32 val = freerdp_settings_get_uint32(settings, key);

		if (!freerdp_settings_set_uint32(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_int32_list_indices)

	for (x = 0; x < ARRAYSIZE(int32_list_indices); x++)
	{
		const size_t key = int32_list_indices[x];
		const INT32 val = freerdp_settings_get_int32(settings, key);

		if (!freerdp_settings_set_int32(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_uint64_list_indices)

	for (x = 0; x < ARRAYSIZE(uint64_list_indices); x++)
	{
		const size_t key = uint64_list_indices[x];
		const UINT64 val = freerdp_settings_get_uint64(settings, key);

		if (!freerdp_settings_set_uint64(settings, key, val))
			goto fail;
	}

#endif
#if defined(have_int64_list_indices)

	for (x = 0; x < ARRAYSIZE(int64_list_indices); x++)
	{
		const size_t key = int64_list_indices[x];
		const INT64 val = freerdp_settings_get_int64(settings, key);

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
	}

#endif
	rc = 0;
fail:
	freerdp_settings_free(cloned);
	freerdp_settings_free(settings);
	return rc;
}
