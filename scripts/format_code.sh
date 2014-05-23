#!/bin/sh

ASTYLE=`which astyle`

if [ ! -x $ASTYLE ];
then
  echo "No astyle found in path, please install."
  exit 1
fi

if [ $# -le 0 ]; then
  echo "Usage:"
  echo "\t$0 <file1> [<file2> ...]"
#  echo "\t$0 -r <directory>"
  exit 2
fi

$ASTYLE --lineend=linux --mode=c --indent=force-tab=4 --brackets=linux --pad-header \
				--indent-switches --indent-cases --indent-preprocessor \
				--indent-col1-comments --delete-empty-lines --break-closing-brackets \
				--align-pointer=name --indent-labels --brackets=break \
				--unpad-paren --break-blocks $@
exit $?
