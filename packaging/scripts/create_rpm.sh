#!/bin/bash -xe
#
# Create a RPM package
SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
SCRIPT_PATH=$(realpath "$SCRIPT_PATH")

mkdir -p ~/rpmbuild/SOURCES/
$SCRIPT_PATH/prepare_rpm_freerdp-nightly.sh

BASE_PREFIX="--prefix=freerdp-nightly-3.0/"
FILES=""
for dir in $(find external/webview -type d -not -path "external/webview/.git*"); do
  FILES+=" $BASE_PREFIX$dir/"
  for file in $(find $dir -maxdepth 1 -type f -not -path "$dir/.git*"); do
    FILES+=" --add-file=$file"
  done
done
FILES+=" $BASE_PREFIX"

git archive --format=tar $FILES HEAD --output ~/rpmbuild/SOURCES/freerdp-nightly-3.0.tar.bz2
cp source_version ~/rpmbuild/SOURCES/
rpmbuild -ba "$SCRIPT_PATH/../rpm/freerdp-nightly.spec"
