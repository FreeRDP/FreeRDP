
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
	wIniFile* ini;

	ini = IniFile_New();

	IniFile_Parse(ini, "/tmp/sample.ini");

	IniFile_Free(ini);

	return 0;
}

