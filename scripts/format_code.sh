#!/bin/bash

ASTYLE=$(which astyle)

if [ ! -x $ASTYLE ]; then
  echo "No astyle found in path, please install."
  exit 1
fi

# Need at least astyle 2.03 due to bugs in older versions
# indenting headers with extern "C"
STR_VERSION=$($ASTYLE --version 2>&1)
VERSION=$(echo $STR_VERSION | cut -d ' ' -f4)
MAJOR_VERSION=$(echo $VERSION | cut -d'.' -f1)
MINOR_VERSION=$(echo $VERSION | cut -d'.' -f2)

if [ "$MAJOR_VERSION" -lt "2" ];
then
  echo "Your version of astyle($VERSION) is too old, need at least 2.03"
  exit 1
elif [ "$MAJOR_VERSION" -eq "2" ];
then
  if [ "$MINOR_VERSION" -lt "3" ];
  then
    echo "Your version of astyle($VERSION) is too old, need at least 2.03"
    exit 1
  fi
fi

if [ $# -le 0 ]; then
  echo "Usage:"
  echo -e "\t$0 <file1> [<file2> ...]"
  exit 2
fi

$ASTYLE --lineend=linux --mode=c --indent=tab=4 --pad-header --pad-oper --style=allman --min-conditional-indent=0 \
        --indent-switches --indent-cases --indent-preprocessor -k1 --max-code-length=100 \
        --indent-col1-comments --delete-empty-lines --break-closing-brackets \
        --align-pointer=type --indent-labels -xe --break-after-logical \
        --unpad-paren --break-blocks $@

exit $?
