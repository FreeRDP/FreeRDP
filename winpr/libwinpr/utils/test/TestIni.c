
#include <winpr/crt.h>

#include <winpr/ini.h>

const char TEST_INI_FILE[] =
	"; This is a sample .ini config file\n"
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

int TestIni(int argc, char* argv[])
{
	int i, j;
	int nKeys;
	int nSections;
	char* sValue;
	UINT32 iValue;
	wIniFile* ini;
	char** keyNames;
	char** sectionNames;

	ini = IniFile_New();

	IniFile_ParseString(ini, TEST_INI_FILE);
	
	sectionNames = IniFile_GetSectionNames(ini, &nSections);
	
	for (i = 0; i < nSections; i++)
	{
		keyNames = IniFile_GetSectionKeyNames(ini, sectionNames[i], &nKeys);
		
		printf("[%s]\n", sectionNames[i]);
		
		for (j = 0; j < nKeys; j++)
		{
			sValue = IniFile_GetKeyValueString(ini, sectionNames[i], keyNames[j]);
			printf("%s = %s\n", keyNames[j], sValue);
		}
		
		free(keyNames);
	}
	
	free(sectionNames);

	iValue = IniFile_GetKeyValueInt(ini, "first_section", "one");
	
	if (iValue != 1)
	{
		printf("IniFile_GetKeyValueInt failure\n");
		return -1;
	}
	
	iValue = IniFile_GetKeyValueInt(ini, "first_section", "five");
	
	if (iValue != 5)
	{
		printf("IniFile_GetKeyValueInt failure\n");
		return -1;
	}
	
	sValue = IniFile_GetKeyValueString(ini, "first_section", "animal");
	
	if (strcmp(sValue, "BIRD") != 0)
	{
		printf("IniFile_GetKeyValueString failure\n");
		return -1;	
	}
	
	sValue = IniFile_GetKeyValueString(ini, "second_section", "path");
	
	if (strcmp(sValue, "/usr/local/bin") != 0)
	{
		printf("IniFile_GetKeyValueString failure\n");
		return -1;	
	}
	
	sValue = IniFile_GetKeyValueString(ini, "second_section", "URL");
	
	if (strcmp(sValue, "http://www.example.com/~username") != 0)
	{
		printf("IniFile_GetKeyValueString failure\n");
		return -1;	
	}
	
	IniFile_Free(ini);

	return 0;
}

