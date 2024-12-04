#!/bin/bash -e

SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
SCRIPT_PATH=$(realpath "$SCRIPT_PATH")
SRC_PATH="${SCRIPT_PATH}/.."

FORMAT_ARG="--check"
if [ $# -ne 0 ]
then
	if [ "$1" = "--help" ] || [ "$1" = "-h" ];
	then
		echo "usage: $0"
		echo "\t--check.-c  ... run format check only, no files changed (default)"
		echo "\t--format,-f ... format files in place"
		echo "\t--help,-h   ... print this help"

		exit 1
	fi

	if [ "$1" = "--check" ] || [ "$1" = "-c" ];
	then
		FORMAT_ARG="--check"
	fi
	if [ "$1" = "--format" ] || [ "$1" = "-f" ];
	then
		FORMAT_ARG="-i"
	fi
fi

CMAKE_FILES=$(find ${SRC_PATH} -name "*.cmake" -o -name "CMakeLists.txt")
CMAKE_CI_FILES=$(find ${SRC_PATH}/ci -name "*.txt")
for FILE in $CMAKE_FILES $CMAKE_CI_FILES;
do
	echo "processing file $FILE..."
	cmake-format -c "$SCRIPT_PATH/cmake-format.yml" $FORMAT_ARG $FILE
done
