
#include <winpr/crt.h>
#include <winpr/ini.h>

static const char TEST_INI_01[] = "; This is a sample .ini config file\n"
                                  "\n"
                                  "[first_section]\n"
                                  "one = 1\n"
                                  "five = 5\n"
                                  "animal = BIRD\n"
                                  "\n"
                                  "[second_section]\n"
                                  "path = \"/usr/local/bin\"\n"
                                  "URL = \"http://www.example.com/~username\"\n"
                                  "\n";

static const char TEST_INI_02[] = "[FreeRDS]\n"
                                  "prefix=\"/usr/local\"\n"
                                  "bindir=\"bin\"\n"
                                  "sbindir=\"sbin\"\n"
                                  "libdir=\"lib\"\n"
                                  "datarootdir=\"share\"\n"
                                  "localstatedir=\"var\"\n"
                                  "sysconfdir=\"etc\"\n"
                                  "\n";

static const char TEST_INI_03[] = "[FreeRDS]\n"
                                  "prefix=\"/usr/local\"\n"
                                  "bindir=\"bin\"\n"
                                  "# some illegal string\n"
                                  "sbindir=\"sbin\"\n"
                                  "libdir=\"lib\"\n"
                                  "invalid key-value pair\n"
                                  "datarootdir=\"share\"\n"
                                  "localstatedir=\"var\"\n"
                                  "sysconfdir=\"etc\"\n"
                                  "\n";

int TestIni(int argc, char* argv[])
{
	int rc = -1;
	int i, j;
	int nKeys;
	int nSections;
	UINT32 iValue;
	wIniFile* ini = NULL;
	const char* sValue;
	char** keyNames = NULL;
	char** sectionNames = NULL;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* First Sample */
	ini = IniFile_New();
	if (!ini)
		goto fail;

	if (IniFile_ReadBuffer(ini, TEST_INI_01) < 0)
		goto fail;

	free(sectionNames);
	sectionNames = IniFile_GetSectionNames(ini, &nSections);
	if (!sectionNames && (nSections > 0))
		goto fail;

	for (i = 0; i < nSections; i++)
	{
		free(keyNames);
		keyNames = IniFile_GetSectionKeyNames(ini, sectionNames[i], &nKeys);
		printf("[%s]\n", sectionNames[i]);
		if (!keyNames && (nKeys > 0))
			goto fail;
		for (j = 0; j < nKeys; j++)
		{
			sValue = IniFile_GetKeyValueString(ini, sectionNames[i], keyNames[j]);
			printf("%s = %s\n", keyNames[j], sValue);
		}
	}

	iValue = IniFile_GetKeyValueInt(ini, "first_section", "one");

	if (iValue != 1)
	{
		printf("IniFile_GetKeyValueInt failure\n");
		goto fail;
	}

	iValue = IniFile_GetKeyValueInt(ini, "first_section", "five");

	if (iValue != 5)
	{
		printf("IniFile_GetKeyValueInt failure\n");
		goto fail;
	}

	sValue = IniFile_GetKeyValueString(ini, "first_section", "animal");

	if (strcmp(sValue, "BIRD") != 0)
	{
		printf("IniFile_GetKeyValueString failure\n");
		goto fail;
	}

	sValue = IniFile_GetKeyValueString(ini, "second_section", "path");

	if (strcmp(sValue, "/usr/local/bin") != 0)
	{
		printf("IniFile_GetKeyValueString failure\n");
		goto fail;
	}

	sValue = IniFile_GetKeyValueString(ini, "second_section", "URL");

	if (strcmp(sValue, "http://www.example.com/~username") != 0)
	{
		printf("IniFile_GetKeyValueString failure\n");
		goto fail;
	}

	IniFile_Free(ini);
	/* Second Sample */
	ini = IniFile_New();
	if (!ini)
		goto fail;
	if (IniFile_ReadBuffer(ini, TEST_INI_02) < 0)
		goto fail;
	free(sectionNames);
	sectionNames = IniFile_GetSectionNames(ini, &nSections);
	if (!sectionNames && (nSections > 0))
		goto fail;

	for (i = 0; i < nSections; i++)
	{
		free(keyNames);
		keyNames = IniFile_GetSectionKeyNames(ini, sectionNames[i], &nKeys);
		printf("[%s]\n", sectionNames[i]);

		if (!keyNames && (nKeys > 0))
			goto fail;
		for (j = 0; j < nKeys; j++)
		{
			sValue = IniFile_GetKeyValueString(ini, sectionNames[i], keyNames[j]);
			printf("%s = %s\n", keyNames[j], sValue);
		}
	}

	IniFile_Free(ini);
	/* Third sample - invalid input */
	ini = IniFile_New();

	if (IniFile_ReadBuffer(ini, TEST_INI_03) != -1)
		goto fail;

	rc = 0;
fail:
	free(keyNames);
	free(sectionNames);
	IniFile_Free(ini);
	return rc;
}
