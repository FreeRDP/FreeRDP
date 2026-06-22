#!/bin/sh
set -eu

FRAMEWORKS_DIR="$1"
shift

if [ "${CODE_SIGNING_ALLOWED:-YES}" = "NO" ]; then
    exit 0
fi

if [ -z "${EXPANDED_CODE_SIGN_IDENTITY:-}" ]; then
    exit 0
fi

for item in "$@"; do
    path="$FRAMEWORKS_DIR/$item"
    if [ -e "$path" ]; then
        /usr/bin/codesign --force --sign "$EXPANDED_CODE_SIGN_IDENTITY" --timestamp=none "$path"
    fi
done
