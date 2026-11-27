#!/bin/bash -e
#
# debian architectures:
#
# * riscv64
# * s390x
# * armel
# * armhf
# * arm64
# * x86
# * x86_64
# * powerpc
# * ppc64el
#
SCRIPT_PATH="$(dirname -- "${BASH_SOURCE[0]}")" # relative
SCRIPT_PATH="$(cd -- "$SCRIPT_PATH" && pwd)"    # absolutized and normalized

BIN=$(type -P podman)
ARCHS=$(uname -m)

usage() {
    echo "Usage: $0 [-a <arch1;arch2> ]" 1>&2;
    echo "where <arch> is one of:";
    echo " * riscv64"
    echo " * s390x"
    echo " * armel"
    echo " * armhf"
    echo " * arm64"
    echo " * x86"
    echo " * x86_64"
    echo " * powerpc"
    echo " * ppc64el"
    echo ""
    echo "defaults to: host architecture"
    exit 1;
}

while getopts "a:" o; do
    case "${o}" in
        a)
            ARCHS=${OPTARG}
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

if [ -z "$BIN" ] || [ ! -x "$BIN" ];
then
    "$BIN does not exist or is not executable"
    exit 1
fi

for ARCH in $ARCHS;
do
    $BIN build --platform linux/$ARCH -t freerdp:$ARCH .
    $BIN run --rm -v $SCRIPT_PATH/../../:/source freerdp:$ARCH
done
