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

function create_hash {
	NAME=$1
	run md5sum ${NAME} > ${NAME}.md5
	run sha1sum ${NAME} > ${NAME}.sha1
	run sha256sum ${NAME} > ${NAME}.sha256
	run sha512sum ${NAME} > ${NAME}.sha512
}

function create_tar {
	ARGS=$1
	EXT=$2
	TAG=$3

	NAME=freerdp-${TAG}${EXT}
	run tar $ARGS ${NAME} freerdp-${TAG}
	create_hash ${NAME}
}

TMPDIR=$(mktemp -d -t release-${TAG}-XXXXXXXXXX)

run git archive --prefix=freerdp-${TAG}/ --format=tar.gz -o ${TMPDIR}/freerdp-${TAG}.tar.gz ${TAG}
run tar xzvf ${TMPDIR}/freerdp-${TAG}.tar.gz -C ${TMPDIR}
run echo ${TAG} > ${TMPDIR}/freerdp-${TAG}/.source_version

pushd .
cd  $TMPDIR
create_tar czf .tar.gz ${TAG}
create_tar cvjSf .tar.bz2 ${TAG}
create_tar cfJ .tar.xz ${TAG}

ZIPNAME=freerdp-${TAG}.zip
run zip -r ${ZIPNAME} freerdp-${TAG}
create_hash ${ZIPNAME}
popd

run mv ${TMPDIR}/freerdp-${TAG}.tar* .
run mv ${TMPDIR}/freerdp-${TAG}.zip* .
run rm -rf ${TMPDIR}
exit 0
