#!/bin/bash -e

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

git archive ${TAG} --prefix=freerdp-${TAG}/ |gzip -9 > ${TMPDIR}/freerdp-${TAG}.tar.gz
tar xzvf ${TMPDIR}/freerdp-${TAG}.tar.gz -C ${TMPDIR}
echo ${TAG} > ${TMPDIR}/freerdp-${TAG}/.source_version
pushd .
cd  $TMPDIR
tar czvf freerdp-${TAG}.tar.gz freerdp-${TAG}
md5sum freerdp-${TAG}.tar.gz > freerdp-${TAG}.tar.gz.md5
sha1sum freerdp-${TAG}.tar.gz > freerdp-${TAG}.tar.gz.sha1
sha256sum freerdp-${TAG}.tar.gz > freerdp-${TAG}.tar.gz.sha256
popd
mv ${TMPDIR}/freerdp-${TAG}.tar.gz* .
rm -rf ${TMPDIR}
exit 0
