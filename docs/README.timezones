## Timezone related options

* WITH_TIMEZONE_COMPILED   Use compiled in mapping extracted with tzextract (default ON)
* WITH_TIMEZONE_FROM_FILE  Use JSON file mapping generated with tzextract (default OFF)
* WITH_TIMEZONE_ICU        Use ICU to map IANA to windows timezones (default OFF)
* WITH_TIMEZONE_UPDATER    Build the tzextract utility (default OFF)

### Suggested usage

* WITH_TIMEZONE_COMPILED is suggested for single binary builds (aka fully static) binaries that
  can not rely on external files to exist.
* WITH_TIMEZONE_FROM_FILE is suggested for distribution packages as this allows updating timezone
  mappings without a necessary recompile of the FreeRDP binaries. Set WITH_TIMEZONE_COMPILED=OFF
  in such cases as both options can be used in combination. (entries from file taking preference
  over compiled in versions)
* WITH_TIMEZONE_ICU is suggested if the target is already linked against ICU. This eliminates a
  required mapping table generated from WindowsZones.xml. This option is only a fallback if the
  compiled in or loaded from file mappings do not match any.

## Keeping timezone mappings up to date

On an up to date windows machine run the following binary after a build with -DWITH_TIMEZONE_UPDATER=ON (from build directory):

tzextract <path to source>\winpr\libwinpr\timezone

After running the scripts check
 * git clang-format -f to eliminate formatting changes
 * winpr/libwinpr/timezone/WindowsZones.c
for changes.

on any machine run the following script from checkout root:

./scripts/update-windows-zones.py

After running the scripts check
 * git clang-format -f to eliminate formatting changes
 * winpr/libwinpr/timezone/WindowsZones.c
for changes.

Commit if the definitions changed:
1. run `git clang-format -f` on these changed files
2. commit these changes
3. create a pull request at https://github.com/FreeRDP/FreeRDP
