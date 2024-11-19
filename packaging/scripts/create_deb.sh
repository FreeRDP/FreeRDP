#!/bin/bash -xe

SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
SCRIPT_PATH=$(realpath "$SCRIPT_PATH")

BUILD_DEPS=$(/usr/bin/which dpkg-checkbuilddeps)
BUILD_PKG=$(/usr/bin/which dpkg-buildpackage)

if [ -z "$BUILD_DEPS" ] || [ -z "$BUILD_PKG" ];
then
    echo "dpkg-buildpackage [$BUILD_PKG] and dpkg-checkbuilddeps [$BUILD_DEPS] required"
    echo "Install with 'sudo apt install dpkg-dev'"
    exit 1
fi

# First create a link to the debian/control folder
cd "$SCRIPT_PATH/../.."
ln -sf "packaging/deb/freerdp-nightly" "debian"

# Check all dependencies are installed
$BUILD_DEPS "debian/control"

# And finally build the package
$BUILD_PKG
