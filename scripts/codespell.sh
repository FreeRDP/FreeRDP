#!/bin/bash -x

SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
SCRIPT_PATH=$(realpath "$SCRIPT_PATH")

# ignore the following regular expressions:
#
# 1. All words consisting of only 2 characters (too many issues with variable names)
# 2. Every word of the form 'pEvent', e.g. variable prefixed with p for pointer
# 3. Every word prefixed by e.g. '\tSome text', e.g. format string escapes
codespell \
    -I "$SCRIPT_PATH/codespell.ignore" \
    -S ".git,*.ai,*.svg,*.rtf,*/assets/de_*,*/res/values-*,*/protocols/xdg*,*/test/*" \
    --ignore-regex "\b[a-zA-Z][a-zA-Z]\b|\bp[A-Z].*|\\\\[a-z][a-zA-Z].*" \
    --count $SCRIPT_PATH/..
