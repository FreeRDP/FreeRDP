#!/bin/sh
tmp0="$(mktemp /tmp/cmdline_0.XXXXXXXXX)" || exit 1
tmp1="$(mktemp /tmp/cmdline_1.XXXXXXXXX)" || exit 1
grep "{ \"" client/common/cmdline.h | cut -d"\"" -f2 > $tmp0
sort $tmp0 > $tmp1
diff -u $tmp0 $tmp1
rm $tmp0 $tmp1
