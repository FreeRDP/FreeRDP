#!/bin/bash -e
#

SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
SCRIPT_PATH=$(realpath "$SCRIPT_PATH")
SRC_PATH="${SCRIPT_PATH}/.."

if [ $# -ne 0 ]; then
  if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "usage: $0 [options]"
    echo "\t--check.-c  ... run format check only, no files changed (default)"
    echo "\t--format,-f ... format files in place"
    echo "\t--help,-h   ... print this help"

    exit 1
  fi

  if [ "$1" = "--check" ] || [ "$1" = "-c" ]; then
    FORMAT_ARG=""
    REST_ARGS="${@:2}"
  fi
  if [ "$1" = "--format" ] || [ "$1" = "-f" ]; then
    FORMAT_ARG="-w"
    REST_ARGS="${@:2}"
  fi
fi

SCRIPTS=$(find ${SRC_PATH} -name "*.sh" -not -path "${SRC_PATH}/.git/*")
for script in $SCRIPTS; do
  echo $script
  shfmt -i 2 $FORMAT_ARG $script
done
