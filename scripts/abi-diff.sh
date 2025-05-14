#!/bin/bash -xe
#
SCRIPT_NAME="${BASH_SOURCE[0]}"
SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
SCRIPT_PATH=$(realpath "$SCRIPT_PATH")

if [ $# -ne 1 ]; then
  echo "$0 <reference commit or tag>"
  exit 1
fi

BASE_REF=$1

cd "$SCRIPT_PATH/.."
mkdir -p abi-checker
cp ci/cmake-preloads/config-abi.txt abi-checker/
cp scripts/abi-suppr.txt abi-checker/
curl https://gist.githubusercontent.com/akallabeth/aa35caed0d39241fa17c3dc8a0539ea3/raw/ef12f8c720ac6be51aa1878710e2502b1b39cf4c/check-abi -o abi-checker/check-abi
chmod +x abi-checker/check-abi

./abi-checker/check-abi -s abi-checker/abi-suppr.txt --parameters="-Cabi-checker/config-abi.txt" $BASE_REF $(git rev-parse HEAD)
