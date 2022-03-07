As FreeRDPs is build on different OS with different build tools and methods the
"version detection" has grown historically.
This document quickly describes how it's currently used.

When doing a `xfreerdp /version` for example the following is shown

`This is FreeRDP version 3.0.0-dev (c99c4cecddee4e5b914b122bc1531d47a668bb8e)`

The first part ist the Version as defined in `RAW_VERSION_STRING` and the second part, in braces,
the `GIT_REVISON` of this version.

`RAW_VERSION_STRING` is very vital as it determines the version used for libraries as well also for
all sub-projects as WinPR.

As default both variables are equal.

For nightly or development builds it is often of advantage to have the actual version from git
instead of having the hard coded value set in CMakeLists.txt. For this the cmake variable `USE_VERSION_FROM_GIT_TAG`
can be set. In order for this to work you need a) source checkout and b) git command line utility.
If enabled the information from the last git tag (in the format major.minor.patch-extra like
2.6.0-android12) will be used.

If you are building FreeRDP and can't use git because it's not available or the source is not in an
git repository - for example when building packages - the files `.source_tag` and  `.source_version`
in the top-level source directory can be used. `.source_tag` is equal to `RAW_VERSION_STRING` and
need to contain the version in the same format as the git tag. `.source_version` is used to pre-fill
`GIT_REVISON`. Although mostly used for that it must not contain a git commit or tag - it can be
used to set additional arbitrary information. Our recommendation for packagers is to create
`.source_version` when importing and set it to the upstream commit or tag to simplify issue
tracking.

As summary the different mechanisms are applied in that order:
* `.source_tag` and  `.source_version` if found
* version set from the last git tag if `RAW_VERSION_STRING` is set
* hard coded version in CMakeLists.txt
