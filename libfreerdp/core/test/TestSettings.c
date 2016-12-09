#include <freerdp/settings.h>

int TestSettings(int argc, char* argv[])
{
	rdpSettings *settings = NULL;
	rdpSettings *cloned;

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
	
	freerdp_settings_free(cloned);
	freerdp_settings_free(settings);
	return 0;
}

