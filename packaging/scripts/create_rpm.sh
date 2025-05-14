#!/bin/bash -xe
#
# Create a RPM package
SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
SCRIPT_PATH=$(realpath "$SCRIPT_PATH")

mkdir -p ~/rpmbuild/SOURCES/
$SCRIPT_PATH/prepare_rpm_freerdp-nightly.sh

git archive --format=tar --prefix=freerdp-nightly-3.0/ HEAD --output ~/rpmbuild/SOURCES/freerdp-nightly-3.0.tar.bz2
cp source_version ~/rpmbuild/SOURCES/
rpmbuild -ba "$SCRIPT_PATH/../rpm/freerdp-nightly.spec"
