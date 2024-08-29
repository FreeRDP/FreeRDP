#include <freerdp/version.h>
#include <freerdp/freerdp.h>

int TestVersion(int argc, char* argv[])
{
	const char* version = NULL;
	const char* git = NULL;
	const char* build = NULL;
	int major = 0;
	int minor = 0;
	int revision = 0;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	freerdp_get_version(&major, &minor, &revision);

	if (major != FREERDP_VERSION_MAJOR)
		return -1;

	if (minor != FREERDP_VERSION_MINOR)
		return -1;

	if (revision != FREERDP_VERSION_REVISION)
		return -1;

	version = freerdp_get_version_string();

	if (!version)
		return -1;

	git = freerdp_get_build_revision();

	if (!git)
		return -1;

	if (strncmp(git, FREERDP_GIT_REVISION, sizeof(FREERDP_GIT_REVISION)) != 0)
		return -1;

	build = freerdp_get_build_config();

	if (!build)
		return -1;

	return 0;
}
