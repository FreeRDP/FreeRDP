#!/bin/bash -e

function run {
	"$@"
	RES=$?
	if [[ $RES -ne 0 ]];
	then
		echo "[ERROR] $@ returned $RES" >&2
		exit 1
	fi
}

if [ -z ${TAG:-} ];then
	echo "No TAG set - trying to detect"
	TAG=$(git describe --tags)
	echo "Is the TAG ${TAG} ok (YES|NO)?"
	read answ
	case "$answ" in
		YES):
			;;
		*)
			echo 'stopping here'
			exit 1
	esac
fi

TMPDIR=$(mktemp -d -t release-${TAG}-XXXXXXXXXX)

run git archive --prefix=freerdp-${TAG}/ --format=tar.gz -o ${TMPDIR}/freerdp-${TAG}.tar.gz ${TAG}
run tar xzvf ${TMPDIR}/freerdp-${TAG}.tar.gz -C ${TMPDIR}
run echo ${TAG} > ${TMPDIR}/freerdp-${TAG}/.source_version

pushd .
cd  $TMPDIR
run tar czvf freerdp-${TAG}.tar.gz freerdp-${TAG}
run md5sum freerdp-${TAG}.tar.gz > freerdp-${TAG}.tar.gz.md5
run sha1sum freerdp-${TAG}.tar.gz > freerdp-${TAG}.tar.gz.sha1
run sha256sum freerdp-${TAG}.tar.gz > freerdp-${TAG}.tar.gz.sha256

run zip -r freerdp-${TAG}.zip freerdp-${TAG}
run md5sum freerdp-${TAG}.zip > freerdp-${TAG}.zip.md5
run sha1sum freerdp-${TAG}.zip > freerdp-${TAG}.zip.sha1
run sha256sum freerdp-${TAG}.zip > freerdp-${TAG}.zip.sha256
popd

run mv ${TMPDIR}/freerdp-${TAG}.tar.gz* .
run mv ${TMPDIR}/freerdp-${TAG}.zip* .
run rm -rf ${TMPDIR}
exit 0
